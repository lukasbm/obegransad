#include "device.h"

void device_init()
{
    // Initialize the default event loop
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Initialize NVS flash for Wi-Fi configuration
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
}

void wifi_init()
{
    // Get the Wi-Fi configuration
    auto &ssid_list = SsidManager::GetInstance().GetSsidList();
    if (ssid_list.empty())
    {
        // Start the Wi-Fi configuration AP
        auto &ap = WifiConfigurationAp::GetInstance();
        ap.SetSsidPrefix("ESP32");
        ap.Start();
        return;
    }

    // Otherwise, connect to the Wi-Fi network
    WifiStation::GetInstance().Start();
}
