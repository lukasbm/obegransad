#include "device.h"

// ESP-IDF core dependencies
#include "esp_check.h"
#include "esp_err.h"   // for esp_err_t and ESP_ERROR_CHECK
#include "esp_event.h" // for esp_event_loop_create_default()
#include "nvs_flash.h" // for nvs_flash_init()
#include <esp_littlefs.h>
#include <esp_log.h>
#include <esp_sleep.h>
#include <esp_system.h>

// esp-wifi-connect headers
#include "ssid_manager.h"          // SsidManager::GetInstance()
#include "wifi_configuration_ap.h" // WifiConfigurationAp::GetInstance()
#include "wifi_station.h"          // WifiStation::GetInstance()

static const char *TAG = "device";

static esp_err_t littlefs_init() {
  esp_vfs_littlefs_conf_t conf = {};
  conf.base_path = "/littlefs";
  conf.partition_label = "storage";
  conf.format_if_mount_failed = true;
  conf.dont_mount = false;

  // Use settings defined above to initialize and mount LittleFS filesystem.
  // Note: esp_vfs_littlefs_register is an all-in-one convenience function.
  ESP_RETURN_ON_ERROR(esp_vfs_littlefs_register(&conf), TAG,
                      "Failed to register LittleFS");

  size_t total = 0, used = 0;
  esp_err_t ret = esp_littlefs_info(conf.partition_label, &total, &used);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to get LittleFS partition information (%s)",
             esp_err_to_name(ret));
    esp_littlefs_format(conf.partition_label);
  } else {
    ESP_LOGI(TAG, "Partition size: total: %d, used: %d", total, used);
  }

  return ESP_OK;
}

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

  // Little FS
  ESP_RETURN_ON_ERROR(littlefs_init(), TAG, "Failed to initialize LittleFS");

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
