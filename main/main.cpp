// #define BUTTON_GPIO 0

// typedef enum
// {
//     APP_STATE_SETUP = 0,
//     APP_STATE_NORMAL,
//     APP_STATE_CAPTIVE_PORTAL,
//     APP_STATE_NO_WIFI,
//     APP_STATE_SLEEPING,
// } app_state_t;

// static httpd_handle_t portal_srv = NULL;

// static esp_err_t wifi_portal_start(void) { /* httpd_start(); set AP creds */ }
// static void wifi_portal_stop(void) { httpd_stop(portal_srv); }

// static volatile app_state_t g_state = APP_STATE_SETUP;
// static void app_update_state(app_state_t next);

// static button_handle_t btn;

// static void cb_single(void *arg, void *data)
// {
//     if (g_state == APP_STATE_NORMAL)
//     {
//         scene_switcher_next();
//     }
//     else if (g_state == APP_STATE_CAPTIVE_PORTAL)
//     {
//         app_update_state(wifi_is_connected() ? APP_STATE_NORMAL : APP_STATE_NO_WIFI);
//     }
// }

// static void cb_long_start(void *arg, void *data) { /* timestamp if you like */ }

// static void cb_long_up(void *arg, void *data)
// {
//     /* 10-s long press means erase credentials */
//     if (iot_button_get_ticks_time((button_handle_t)arg) >= 10 * 1000)
//     {
//         wifi_clear_credentials();
//     }
// }

// static void button_init(void)
// {
//     const button_config_t cfg = {
//         .type = BUTTON_TYPE_GPIO,
//         .long_press_time = 10000, // ms
//     };
//     const button_gpio_config_t gpio_cfg = {
//         .gpio_num = BUTTON_GPIO,
//         .active_level = 0, // active-low
//     };
//     ESP_ERROR_CHECK(iot_button_create(&cfg, &gpio_cfg, &btn)); // g_state safe

//     iot_button_register_cb(btn, BUTTON_SINGLE_CLICK, NULL, cb_single, NULL);
//     iot_button_register_cb(btn, BUTTON_LONG_PRESS_START, NULL, cb_long_start, NULL);
//     iot_button_register_cb(btn, BUTTON_LONG_PRESS_UP, NULL, cb_long_up, NULL);
// }

// static void weather_task(void *arg)
// {
//     const TickType_t delay = 30 * 60 * 1000 / portTICK_PERIOD_MS;
//     while (1)
//     {
//         if (g_state == APP_STATE_NORMAL)
//         {
//             weather_fetch_once(); // esp_http_client_perform()
//         }
//         vTaskDelay(delay);
//     }
// }

// static void ntp_task(void *arg)
// {
//     while (1)
//     {
//         sntp_sync_time(); // blocks until update or timeout
//         /* sntp has its own interval but we can force sync if needed */
//         vTaskDelay(pdMS_TO_TICKS(30 * 60 * 1000));
//     }
// }

extern "C" void app_main()
{
}
