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
#include <string.h>

// Minimal in-house Wi-Fi station driver.
//
// Replaces the esp-wifi-connect component: credentials come from Kconfig (see
// main/Kconfig.projbuild) and are kept in RAM only, so no NVS churn and no
// stale credentials. The supervisor below handles reconnect backoff; the
// captive portal was removed (see docs/known-issues.md).
static const char *TAG = "device";

// state variables
static bool wifi_station_started = false;
static bool wifi_connected = false;

// --- Wi-Fi supervisor -------------------------------------------------------
//
// Every failed connection attempt surfaces as a WIFI_EVENT_STA_DISCONNECTED.
// Posting those straight onto the app bus would flap OPERATIONAL<->DEGRADED
// during perfectly normal reconnects, so the link counts as down only after it
// has stayed down for a grace period; while down, the driver is nudged back
// into a connect cycle on a backoff schedule.
static bool link_announced_up = false; // last state posted to the app bus
static uint32_t link_down_since_ms = 0;
static uint32_t next_nudge_ms = 0;   // when to force the next reconnect attempt
static uint32_t nudge_backoff_ms = 0;
static int last_disconnect_reason = 0;

static constexpr uint32_t DOWN_GRACE_MS = CONFIG_OBG_WIFI_DOWN_GRACE_MS;
// First reconnect attempt after a disconnect, then doubling up to NUDGE_MAX_MS.
// (The removed esp-wifi-connect component used to do the quick retries; the
// supervisor owns them now.)
static constexpr uint32_t NUDGE_FIRST_MS = 2 * 1000;
static constexpr uint32_t NUDGE_MAX_MS = 300 * 1000;

static inline uint32_t now_ms() {
  return (uint32_t)(esp_timer_get_time() / 1000);
}

// Translate low-level Wi-Fi/IP driver events into high-level app events so the
// rest of the system can subscribe to connectivity changes without polling.
static void wifi_event_handler(void * /*arg*/, esp_event_base_t event_base,
                               int32_t event_id, void *event_data) {
  if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
    esp_wifi_connect();
  } else if (event_base == WIFI_EVENT &&
             event_id == WIFI_EVENT_STA_DISCONNECTED) {
    wifi_connected = false;
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
    wifi_connected = true;
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
      esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                 &wifi_event_handler, nullptr),
      TAG, "Failed to register WiFi event handler");
  ESP_RETURN_ON_ERROR(
      esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                 &wifi_event_handler, nullptr),
      TAG, "Failed to register IP event handler");

  // Initialize NVS flash (app config lives there; see config_store.cpp)
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

void wifi_init() {
  // Credentials come from Kconfig (normally sdkconfig.defaults.local).
  if (CONFIG_OBG_WIFI_SSID[0] == '\0') {
    ESP_LOGE(TAG, "No WiFi SSID configured (CONFIG_OBG_WIFI_SSID); set it in "
                  "sdkconfig.defaults.local and reflash (there is no captive "
                  "portal, see docs/known-issues.md)");
    return;
  }

  if (esp_netif_create_default_wifi_sta() == nullptr) {
    ESP_LOGE(TAG, "Failed to create station netif");
    return;
  }

  wifi_init_config_t init_config = WIFI_INIT_CONFIG_DEFAULT();
  ESP_RETURN_VOID_ON_ERROR(esp_wifi_init(&init_config), TAG,
                           "Failed to initialize WiFi");
  // Credentials come from Kconfig on every boot: keep them in RAM only.
  ESP_RETURN_VOID_ON_ERROR(esp_wifi_set_storage(WIFI_STORAGE_RAM), TAG,
                           "Failed to set WiFi storage");

  wifi_config_t wifi_config = {};
  strlcpy((char *)wifi_config.sta.ssid, CONFIG_OBG_WIFI_SSID,
          sizeof(wifi_config.sta.ssid));
  strlcpy((char *)wifi_config.sta.password, CONFIG_OBG_WIFI_PASSWORD,
          sizeof(wifi_config.sta.password));
  // Accept any auth mode up to WPA3 (WIFI_AUTH_OPEN is the lowest threshold).
  wifi_config.sta.threshold.authmode = WIFI_AUTH_OPEN;

  ESP_LOGI(TAG, "Connecting to configured WiFi SSID: %s", CONFIG_OBG_WIFI_SSID);
  ESP_RETURN_VOID_ON_ERROR(esp_wifi_set_mode(WIFI_MODE_STA), TAG,
                           "Failed to set WiFi mode");
  ESP_RETURN_VOID_ON_ERROR(esp_wifi_set_config(WIFI_IF_STA, &wifi_config), TAG,
                           "Failed to set WiFi config");
  ESP_RETURN_VOID_ON_ERROR(esp_wifi_start(), TAG, "Failed to start WiFi");

  link_down_since_ms = now_ms();
  next_nudge_ms = link_down_since_ms + NUDGE_FIRST_MS;
  nudge_backoff_ms = NUDGE_FIRST_MS;
  wifi_station_started = true;
}

bool wifi_check() { return wifi_connected; }

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
