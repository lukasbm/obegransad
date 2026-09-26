// Simulator device backend: QEMU/host builds have no esp_wifi, so "Wi-Fi" is
// QEMU's emulated OpenCores Ethernet (openeth) with DHCP. The app event
// contract is identical to device_hw.cpp: a working IP link posts
// APP_EVT_WIFI_CONNECTED, so the state machine, SNTP and weather are unchanged.
//
// Wi-Fi credentials are irrelevant here; the captive portal cannot run without
// AP mode and is reduced to a state flag.

#include "sdkconfig.h"

#if !CONFIG_OBG_DEVICE_BACKEND_HW

#include "device.h"

#include "app_events.h"

#include "esp_check.h"
#include "esp_err.h"
#include "esp_event.h"
#include "esp_eth.h"
#include "esp_eth_mac_openeth.h"
#include "esp_eth_phy.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"

static const char *TAG = "device_sim";

static bool s_got_ip = false;
static bool s_link_announced_up = false;
static bool s_captive_portal_active = false;
static esp_eth_handle_t s_eth = nullptr;

static void eth_event_handler(void * /*arg*/, esp_event_base_t event_base,
                              int32_t event_id, void *event_data) {
  if (event_base == IP_EVENT && event_id == IP_EVENT_ETH_GOT_IP) {
    const auto *event = static_cast<ip_event_got_ip_t *>(event_data);
    s_got_ip = true;
    if (!s_link_announced_up) {
      s_link_announced_up = true;
      ESP_LOGI(TAG, "Emulated Ethernet up: " IPSTR, IP2STR(&event->ip_info.ip));
      app_post_event(APP_EVT_WIFI_CONNECTED);
    }
  }
}

esp_err_t device_init() {
  ESP_RETURN_ON_ERROR(esp_netif_init(), TAG, "Failed to initialize netif");
  ESP_RETURN_ON_ERROR(esp_event_loop_create_default(), TAG,
                      "Failed to create default event loop");
  ESP_RETURN_ON_ERROR(
      esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP,
                                 &eth_event_handler, nullptr),
      TAG, "Failed to register Ethernet IP handler");

  // NVS is still used for app config (and persists across QEMU runs when the
  // runner is started with --persist).
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

void wifi_init() {
  ESP_LOGI(TAG, "Simulator: starting emulated Ethernet (QEMU openeth)");

  eth_mac_config_t mac_config = ETH_MAC_DEFAULT_CONFIG();
  esp_eth_mac_t *mac = esp_eth_mac_new_openeth(&mac_config);
  eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();
  phy_config.phy_addr = 1; // QEMU emulates a DP83848C at address 1
  esp_eth_phy_t *phy = esp_eth_phy_new_generic(&phy_config);
  esp_eth_config_t eth_config = ETH_DEFAULT_CONFIG(mac, phy);

  esp_err_t err = esp_eth_driver_install(&eth_config, &s_eth);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Ethernet driver install failed: %s", esp_err_to_name(err));
    return;
  }

  esp_netif_config_t netif_config = ESP_NETIF_DEFAULT_ETH();
  esp_netif_t *netif = esp_netif_new(&netif_config);
  if (netif == nullptr ||
      esp_netif_attach(netif, esp_eth_new_netif_glue(s_eth)) != ESP_OK) {
    ESP_LOGE(TAG, "Ethernet netif setup failed");
    return;
  }

  err = esp_eth_start(s_eth);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Ethernet start failed: %s", esp_err_to_name(err));
    return;
  }
  ESP_LOGI(TAG, "Waiting for DHCP lease...");
}

bool wifi_check() { return s_got_ip; }

void wifi_supervisor_tick() {
  // The emulated Ethernet link either has an IP or does not; nothing to nurse.
}

bool wifi_has_credentials() {
#if CONFIG_OBG_SIM_FAKE_NO_CREDS
  return false;
#else
  return true; // credentials are irrelevant for the emulated Ethernet link
#endif
}

void wifi_clear_credentials() {
  ESP_LOGI(TAG, "Simulator: wifi_clear_credentials() is a no-op");
}

bool wifi_wait_for_connection(uint32_t timeout_ms) {
  ESP_LOGI(TAG, "Waiting for emulated Ethernet (timeout: %lu ms)...",
           timeout_ms);
  const uint32_t check_interval_ms = 500;
  for (uint32_t elapsed_ms = 0; elapsed_ms < timeout_ms;
       elapsed_ms += check_interval_ms) {
    if (wifi_check()) {
      ESP_LOGI(TAG, "Ethernet up after %lu ms", elapsed_ms);
      return true;
    }
    vTaskDelay(pdMS_TO_TICKS(check_interval_ms));
  }
  ESP_LOGW(TAG, "Ethernet timeout after %lu ms", timeout_ms);
  return false;
}

void start_captive_portal() {
  if (s_captive_portal_active) {
    return;
  }
  ESP_LOGW(TAG, "Simulator: captive portal is unavailable (no AP mode); "
                "SETUP state only");
  s_captive_portal_active = true;
  app_post_event(APP_EVT_CAPTIVE_PORTAL_ACTIVE);
}

void stop_captive_portal() { s_captive_portal_active = false; }

bool is_captive_portal_active() { return s_captive_portal_active; }

void enter_light_sleep() {
  // Light sleep is not worth emulating; SLEEPING would otherwise never wake.
  ESP_LOGW(TAG, "Simulator: light sleep ignored");
}

#endif // !CONFIG_OBG_DEVICE_BACKEND_HW
