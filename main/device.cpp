#include "device.h"

#include "app_events.h"
#include "sdkconfig.h"

// ESP-IDF core dependencies
#include "esp_check.h"
#include "esp_err.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include <esp_log.h>
#include <esp_netif.h>
#include <esp_sleep.h>
#include <esp_system.h>
#include <esp_wifi.h>

// esp-wifi-connect headers
#include "ssid_manager.h"          // SsidManager::GetInstance()
#include "wifi_configuration_ap.h" // WifiConfigurationAp::GetInstance()
#include "wifi_station.h"          // WifiStation::GetInstance()

static const char *TAG = "device";

// state variables
static bool captive_portal_active = false;
static bool wifi_station_started = false;

// Translate low-level Wi-Fi/IP driver events into high-level app events so the
// rest of the system can subscribe to connectivity changes without polling.
static void wifi_event_handler(void * /*arg*/, esp_event_base_t event_base,
                               int32_t event_id, void * /*event_data*/) {
  if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
    app_post_event(APP_EVT_WIFI_DISCONNECTED);
  } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
    app_post_event(APP_EVT_WIFI_CONNECTED);
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

  // Stop captive portal if active
  stop_captive_portal();
}

void start_captive_portal() {
  if (captive_portal_active) {
    ESP_LOGW(TAG, "Captive portal already active, skipping start");
    return;
  }

  // Stop station mode first to prevent APSTA interference
  // Only stop if it was actually started to avoid ESP_ERR_WIFI_NOT_INIT
  if (wifi_station_started) {
    ESP_LOGI(TAG, "Stopping WifiStation before starting captive portal");
    WifiStation::GetInstance().Stop();
    wifi_station_started = false;
  }

  ESP_LOGI(TAG, "Starting captive portal...");
  auto &ap = WifiConfigurationAp::GetInstance();
  ap.SetSsidPrefix("Obegransad");
  ap.Start();
  captive_portal_active = true;
  app_post_event(APP_EVT_CAPTIVE_PORTAL_ACTIVE);

  ESP_LOGI(TAG, "Captive portal started - SSID: %s", ap.GetSsid().c_str());
}

void stop_captive_portal() {
  if (!captive_portal_active) {
    ESP_LOGW(TAG, "Captive portal not active, skipping stop");
    return;
  }

  WifiConfigurationAp::GetInstance().Stop();
  captive_portal_active = false;
  ESP_LOGI(TAG, "Captive portal stopped");
}

bool is_captive_portal_active() { return captive_portal_active; }

bool wifi_has_credentials() {
  return !SsidManager::GetInstance().GetSsidList().empty();
}

void wifi_init() {
  // Credentials come from Kconfig. Seed the SsidManager so WifiStation connects
  // to exactly the configured network (no captive portal when an SSID is set).
  if (CONFIG_OBG_WIFI_SSID[0] == '\0') {
    ESP_LOGW(TAG, "No WiFi SSID configured (CONFIG_OBG_WIFI_SSID); "
                  "starting captive portal");
    start_captive_portal();
    return;
  }

  auto &mgr = SsidManager::GetInstance();
  mgr.Clear();
  mgr.AddSsid(CONFIG_OBG_WIFI_SSID, CONFIG_OBG_WIFI_PASSWORD);

  ESP_LOGI(TAG, "Connecting to configured WiFi SSID: %s", CONFIG_OBG_WIFI_SSID);
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
