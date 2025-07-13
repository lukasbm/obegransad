#include "device.h"

// ESP-IDF core dependencies
#include "esp_check.h"
#include "esp_err.h"   // for esp_err_t and ESP_ERROR_CHECK
#include "esp_event.h" // for esp_event_loop_create_default()
#include "nvs_flash.h" // for nvs_flash_init()
#include <esp_sleep.h>

// esp-wifi-connect headers
#include "ssid_manager.h"          // SsidManager::GetInstance()
#include "wifi_configuration_ap.h" // WifiConfigurationAp::GetInstance()
#include "wifi_station.h"          // WifiStation::GetInstance()

static const char *TAG = "device";

esp_err_t device_init() {
  // network stack
  ESP_RETURN_ON_ERROR(esp_netif_init(), TAG, "Failed to initialize netif");

  // default event loop needed for wifi
  ESP_RETURN_ON_ERROR(esp_event_loop_create_default(), TAG,
                      "Failed to create default event loop");

  // Initialize NVS flash (e.g. to store wifi creds)
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
      ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_RETURN_ON_ERROR(nvs_flash_erase(), TAG,
                        "Failed to erase NVS flash, retrying init");
    ret = nvs_flash_init();
  }
  ESP_RETURN_ON_ERROR(ret, TAG, "Failed to initialize NVS flash");

  return ESP_OK;
}

void wifi_clear_credentials() {
  // Clear stored Wi-Fi credentials
  SsidManager::GetInstance().Clear();
  WifiStation::GetInstance().Stop();
  stop_captive_portal();
}

// FIXME: check if already active??
void start_captive_portal() {
  auto &ap = WifiConfigurationAp::GetInstance();
  ap.SetSsidPrefix("Obegransad");
  ap.Start();
}

// FIXME: check if already active??
void stop_captive_portal() {
  // Stop the captive portal
  WifiConfigurationAp::GetInstance().Stop();
}

void wifi_init() {
  auto &ssid_list = SsidManager::GetInstance().GetSsidList();
  if (ssid_list.empty()) {
    start_captive_portal();
  } else {
    WifiStation::GetInstance().Start();
  }
}

bool wifi_check() { return WifiStation::GetInstance().IsConnected(); }

void enter_light_sleep() {
  // Enter light sleep mode
  // esp_sleep_enable_ext0_wakeup(BUTTON_PIN, 0); // Wake up on button press
  esp_light_sleep_start();
}
