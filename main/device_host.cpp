// Host device backend: native Linux/POSIX builds use the host network stack
// directly, so there is no esp_wifi, no esp_netif and no DHCP to manage. The
// app event contract stays the same: the link is announced as connected on
// the first supervisor tick (which runs after the state machine subscribed),
// so state, clock and weather behave exactly like on the target.
//
// Wi-Fi credentials and the captive portal are meaningless here; the portal
// is reduced to a state flag.

#include "sdkconfig.h"

#if CONFIG_OBG_DEVICE_BACKEND_HOST

#include "device.h"

#include "app_events.h"

#include "esp_check.h"
#include "esp_err.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "psa/crypto.h"

static const char *TAG = "device_host";

static bool s_link_announced = false;
static bool s_captive_portal_active = false;

esp_err_t device_init() {
  // mbedTLS's PSA layer is initialized lazily on target, but on the Linux
  // target the first TLS handshake fails (PSA_ERROR_INSUFFICIENT_ENTROPY)
  // unless it is initialized up front.
  psa_status_t psa_status = psa_crypto_init();
  if (psa_status != PSA_SUCCESS) {
    ESP_LOGW(TAG, "psa_crypto_init failed: %d (TLS may not work)", (int)psa_status);
  }

  ESP_RETURN_ON_ERROR(esp_event_loop_create_default(), TAG,
                      "Failed to create default event loop");

  // NVS on the host target is backed by a file in the build directory.
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
  ESP_LOGI(TAG, "Host build: using the host network stack (no Wi-Fi driver)");
}

void wifi_supervisor_tick() {
  if (!s_link_announced) {
    s_link_announced = true;
    ESP_LOGI(TAG, "Host network available");
    app_post_event(APP_EVT_WIFI_CONNECTED);
  }
}

bool wifi_check() { return true; }

bool wifi_has_credentials() { return true; }

void wifi_clear_credentials() {
  ESP_LOGI(TAG, "Host build: wifi_clear_credentials() is a no-op");
}

bool wifi_wait_for_connection(uint32_t timeout_ms) {
  (void)timeout_ms;
  return true;
}

void start_captive_portal() {
  if (s_captive_portal_active) {
    return;
  }
  ESP_LOGW(TAG, "Host build: captive portal is unavailable; SETUP state only");
  s_captive_portal_active = true;
  app_post_event(APP_EVT_CAPTIVE_PORTAL_ACTIVE);
}

void stop_captive_portal() { s_captive_portal_active = false; }

bool is_captive_portal_active() { return s_captive_portal_active; }

void enter_light_sleep() {
  ESP_LOGW(TAG, "Host build: light sleep ignored");
}

#endif // CONFIG_OBG_DEVICE_BACKEND_HOST
