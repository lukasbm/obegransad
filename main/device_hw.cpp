#include "sdkconfig.h"

#if CONFIG_OBG_DEVICE_BACKEND_HW

#include "device.h"

#include "app_events.h"

// ESP-IDF core dependencies
#include "esp_check.h"
#include "esp_err.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include <esp_log.h>
#include <esp_netif.h>
#include <esp_sleep.h>
#include <esp_system.h>
#include <esp_timer.h>
#include <esp_wifi.h>

// esp-wifi-connect headers
#include "ssid_manager.h"          // SsidManager::GetInstance()
#include "wifi_station.h"          // WifiStation::GetInstance()

static const char *TAG = "device";

// state variables
static bool wifi_station_started = false;

// --- Wi-Fi supervisor -------------------------------------------------------
//
// The vendored esp-wifi-connect component already retries (5 immediate
// esp_wifi_connect() attempts, then the next AP from its scan queue), but every
// failed attempt surfaces as another WIFI_EVENT_STA_DISCONNECTED. Posting those
// straight onto the app bus made the app flap OPERATIONAL<->DEGRADED during
// perfectly normal reconnects. So we debounce here: the link counts as down
// only after it has stayed down for a grace period, and while it is down we
// nudge the driver on a backoff schedule in case its own retries gave up.
//
// Known wart we do not own: wifi_station.cc re-arms its rescan timer with
// `10 * 1000` microseconds — 10 ms, almost certainly meant to be 10 s — so a
// long outage also has that component scanning in a tight loop.
static bool link_announced_up = false; // last state posted to the app bus
static uint32_t link_down_since_ms = 0;
static uint32_t next_nudge_ms = 0;   // when to force the next reconnect attempt
static uint32_t nudge_backoff_ms = 0;
static int last_disconnect_reason = 0;

static constexpr uint32_t DOWN_GRACE_MS = CONFIG_OBG_WIFI_DOWN_GRACE_MS;
static constexpr uint32_t NUDGE_FIRST_MS = 30 * 1000;
static constexpr uint32_t NUDGE_MAX_MS = 300 * 1000;

static inline uint32_t now_ms() {
  return (uint32_t)(esp_timer_get_time() / 1000);
}

// Translate low-level Wi-Fi/IP driver events into high-level app events so the
// rest of the system can subscribe to connectivity changes without polling.
static void wifi_event_handler(void * /*arg*/, esp_event_base_t event_base,
                               int32_t event_id, void *event_data) {
  if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
    if (event_data != nullptr) {
      const auto *d =
          static_cast<wifi_event_sta_disconnected_t *>(event_data);
      // reason 15 = 4WAY_HANDSHAKE_TIMEOUT (wrong password); 201 = NO_AP_FOUND;
      // 2/4 = AUTH/ASSOC_EXPIRE (often weak signal). See esp_wifi_types.h.
      last_disconnect_reason = d->reason;
      ESP_LOGD(TAG, "WiFi disconnected from '%s' (reason=%d, rssi=%d)", d->ssid,
               d->reason, d->rssi);
    }
    if (link_down_since_ms == 0) {
      link_down_since_ms = now_ms();
      next_nudge_ms = link_down_since_ms + NUDGE_FIRST_MS;
      nudge_backoff_ms = NUDGE_FIRST_MS;
    }
    // The app event is posted by wifi_supervisor_tick() once the link has been
    // down long enough to be worth reporting.
  } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
    link_down_since_ms = 0;
    if (!link_announced_up) {
      link_announced_up = true;
      app_post_event(APP_EVT_WIFI_CONNECTED);
    }
  }
}

void wifi_supervisor_tick() {
  if (link_down_since_ms == 0) {
    return; // link is up (or has never been down)
  }

  const uint32_t down_for = now_ms() - link_down_since_ms;

  if (link_announced_up && down_for >= DOWN_GRACE_MS) {
    link_announced_up = false;
    ESP_LOGW(TAG, "WiFi down for %ums (reason=%d); reporting disconnected",
             (unsigned)down_for, last_disconnect_reason);
    app_post_event(APP_EVT_WIFI_DISCONNECTED);
  }

  // Nudge the driver back into a connect cycle with a growing backoff, in case
  // its own retries have been exhausted.
  if (wifi_station_started && (int32_t)(now_ms() - next_nudge_ms) >= 0) {
    ESP_LOGI(TAG, "WiFi down for %us; forcing a reconnect attempt",
             (unsigned)(down_for / 1000));
    esp_wifi_disconnect();
    esp_wifi_connect();

    nudge_backoff_ms = (nudge_backoff_ms >= NUDGE_MAX_MS / 2)
                           ? NUDGE_MAX_MS
                           : nudge_backoff_ms * 2;
    next_nudge_ms = now_ms() + nudge_backoff_ms;
  }
}

esp_err_t device_init() {
  // network stack
  ESP_RETURN_ON_ERROR(esp_netif_init(), TAG, "Failed to initialize netif");

  // default event loop needed for wifi
  ESP_RETURN_ON_ERROR(esp_event_loop_create_default(), TAG,
                      "Failed to create default event loop");

  // Bridge driver events onto the application event bus.
  ESP_RETURN_ON_ERROR(
      esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED,
                                 &wifi_event_handler, nullptr),
      TAG, "Failed to register WiFi event handler");
  ESP_RETURN_ON_ERROR(
      esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                 &wifi_event_handler, nullptr),
      TAG, "Failed to register IP event handler");

  // Initialize NVS flash (e.g. to store wifi creds and config)
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
      ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    // NVS partition was truncated and needs to be erased
    ESP_RETURN_ON_ERROR(nvs_flash_erase(), TAG,
                        "Failed to erase NVS flash, retrying init");
    ret = nvs_flash_init();
  }
  ESP_RETURN_ON_ERROR(ret, TAG, "Failed to initialize NVS flash");

  return ESP_OK;
}

void wifi_clear_credentials() {
  ESP_LOGI(TAG, "Clearing WiFi credentials from NVS");

  // Clear stored Wi-Fi credentials
  SsidManager::GetInstance().Clear();

  // Only stop station if it was started
  if (wifi_station_started) {
    ESP_LOGI(TAG, "Stopping WifiStation");
    WifiStation::GetInstance().Stop();
    wifi_station_started = false;
  }
}

bool wifi_has_credentials() {
  return !SsidManager::GetInstance().GetSsidList().empty();
}

void wifi_init() {
  // Credentials come from Kconfig. Seed the SsidManager so WifiStation connects
  // to exactly the configured network.
  if (CONFIG_OBG_WIFI_SSID[0] == '\0') {
    // The captive portal was removed (it clashed with the config server, see
    // docs/known-issues.md): with no credentials the device has no network
    // until it is reflashed with CONFIG_OBG_WIFI_SSID set.
    ESP_LOGE(TAG, "No WiFi SSID configured (CONFIG_OBG_WIFI_SSID); set it in "
                  "sdkconfig.defaults.local and reflash");
    return;
  }

  auto &mgr = SsidManager::GetInstance();
  mgr.Clear();
  mgr.AddSsid(CONFIG_OBG_WIFI_SSID, CONFIG_OBG_WIFI_PASSWORD);

  ESP_LOGI(TAG, "Connecting to configured WiFi SSID: %s", CONFIG_OBG_WIFI_SSID);
  link_down_since_ms = now_ms();
  next_nudge_ms = link_down_since_ms + NUDGE_FIRST_MS;
  nudge_backoff_ms = NUDGE_FIRST_MS;
  WifiStation::GetInstance().Start();
  wifi_station_started = true;
}

bool wifi_check() { return WifiStation::GetInstance().IsConnected(); }

bool wifi_wait_for_connection(uint32_t timeout_ms) {
  ESP_LOGI(TAG, "Waiting for WiFi connection (timeout: %lu ms)...", timeout_ms);
  
  const uint32_t check_interval_ms = 500;
  uint32_t elapsed_ms = 0;
  
  while (elapsed_ms < timeout_ms) {
    if (wifi_check()) {
      ESP_LOGI(TAG, "WiFi connected after %lu ms", elapsed_ms);
      return true;
    }
    
    vTaskDelay(pdMS_TO_TICKS(check_interval_ms));
    elapsed_ms += check_interval_ms;
    
    // Log progress every 10 seconds
    if (elapsed_ms % 10000 == 0) {
      ESP_LOGI(TAG, "Still waiting for connection... (%lu/%lu ms)", 
               elapsed_ms, timeout_ms);
    }
  }
  
  ESP_LOGW(TAG, "WiFi connection timeout after %lu ms", timeout_ms);
  return false;
}

void enter_light_sleep() {
  // Enter light sleep mode
  // esp_sleep_enable_ext0_wakeup(BUTTON_PIN, 0); // Wake up on button press
  esp_light_sleep_start();
}

#endif // CONFIG_OBG_DEVICE_BACKEND_HW
