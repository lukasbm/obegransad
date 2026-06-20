#include "state.h"

#include "app_events.h"
#include "clock.h"
#include "device.h"
#include "helper.hpp" // millis()
#include "scene_switcher.h"
#include "weather_client.h"
#include "ikea-obegransad-panel.h"

// Sprites
#include "sprites/wifi.hpp"
#include "sprites/bold_glyphs.hpp"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <sdkconfig.h>

static const char* TAG = "StateMachine";

// How long each scene is shown before auto-advancing (from Kconfig). The rotation
// alternates clock and non-clock scenes, so a clock is visible at least every
// other dwell interval.
static constexpr uint32_t SCENE_DWELL_MS = CONFIG_OBG_SCENE_DWELL_MS;

// OPERATIONAL and DEGRADED both rotate scenes; transitions between them must not
// reset the dwell timer.
static bool is_rotating_state(AppState s) {
    return s == AppState::OPERATIONAL || s == AppState::DEGRADED;
}

StateMachine& StateMachine::instance() {
    static StateMachine instance;
    return instance;
}

StateMachine::StateMachine() = default;

void StateMachine::reset_scene_dwell() {
    last_scene_advance_ms = millis();
}

void StateMachine::init() {
    ESP_LOGI(TAG, "Initializing State Machine");

    // Subscribe to the application event bus. The handler only enqueues; the
    // events are applied on the main task in process_events().
    event_queue = xQueueCreate(16, sizeof(int32_t));
    if (event_queue == nullptr) {
        ESP_LOGE(TAG, "Failed to create event queue");
    }
    ESP_ERROR_CHECK(esp_event_handler_register(
        APP_EVENTS, ESP_EVENT_ANY_ID, &StateMachine::on_app_event, nullptr));

    // Determine initial state based on WiFi/Credentials. Subsequent changes
    // arrive as events; we only poll once here to pick the starting state.
    if (wifi_has_credentials()) {
        if (wifi_check()) {
            set_state(AppState::OPERATIONAL);
        } else {
            // Try to connect, if we are not connected yet but have creds
            // We start in DEGRADED and hope to connect
            set_state(AppState::DEGRADED);
        }
    } else {
        set_state(AppState::SETUP);
    }
}

void StateMachine::update() {
    // Apply any pending events first, then render for the (possibly new) state.
    process_events();

    // Automatic scene rotation while in a display state (OPERATIONAL/DEGRADED).
    // Driven here on the main task so it survives Wi-Fi flaps and needs no timer.
    // Can be toggled off with a double button-press (then only single presses
    // change scenes).
    if (auto_advance_enabled && is_rotating_state(current_state) &&
        (millis() - last_scene_advance_ms) >= SCENE_DWELL_MS) {
        next_auto_scene();
        last_scene_advance_ms = millis();
    }

    switch (current_state) {
        case AppState::OPERATIONAL:
            tick(); // Update scenes
            // No extra commit needed as scenes usually commit.
            // But to be safe if we add overlays later:
            // panel_commit();
            break;

        case AppState::DEGRADED:
            tick(); // Update scenes

            // Draw warning dot (Top Right Pixel) - Overlay
            panel_setPixel(15, 0, PANEL_BRIGHTNESS_1);
            panel_commit(); // Commit overlay
            break;

        case AppState::SETUP:
            // In SETUP, we show the WiFi icon
            panel_clear();
            wifi_sprite.draw(0, 0); 
            panel_commit();
            
            if (wifi_has_credentials()) {
                 // If creds appeared, maybe user saved them. 
            }
            break;

        case AppState::ERROR:
            panel_clear();
            // Draw '!'
            font_bold.drawGlyph('!', 4, 4);
            panel_commit();
            break;
            
        case AppState::SLEEPING:
            // Should be sleeping
            break;
    }
}

void StateMachine::on_app_event(void * /*arg*/, esp_event_base_t /*base*/,
                                int32_t id, void * /*data*/) {
    StateMachine &self = StateMachine::instance();
    if (self.event_queue != nullptr) {
        // Non-blocking: dropping an event under extreme backpressure is
        // preferable to stalling the event-loop task.
        xQueueSend(self.event_queue, &id, 0);
    }
}

void StateMachine::process_events() {
    if (event_queue == nullptr) return;

    int32_t id;
    while (xQueueReceive(event_queue, &id, 0) == pdTRUE) {
        switch (id) {
            case APP_EVT_WIFI_CONNECTED:        on_wifi_connected();     break;
            case APP_EVT_WIFI_DISCONNECTED:     on_wifi_disconnected();  break;
            case APP_EVT_BUTTON_SHORT:
                ESP_LOGI(TAG, "button: short press");
                on_button_short_press();
                break;
            case APP_EVT_BUTTON_LONG:
                ESP_LOGI(TAG, "button: long press");
                on_button_long_press();
                break;
            case APP_EVT_BUTTON_DOUBLE:
                ESP_LOGI(TAG, "button: double press");
                on_button_double_press();
                break;
            case APP_EVT_CAPTIVE_PORTAL_ACTIVE: /* informational */      break;
            case APP_EVT_TIME_SYNCED:
                ESP_LOGI(TAG, "Time synced");
                // Timestamps are meaningful now; refresh weather.
                weather_client_request_fetch();
                break;
            case APP_EVT_WEATHER_DATA_READY:
                ESP_LOGI(TAG, "Weather data ready");
                break;
            case APP_EVT_SCENE_ADVANCE:
                if (current_state == AppState::OPERATIONAL ||
                    current_state == AppState::DEGRADED) {
                    next_auto_scene();
                }
                break;
            case APP_EVT_ERROR_OCCURED:
                set_state(AppState::ERROR);
                break;
            default: break;
        }
    }
}

void StateMachine::on_wifi_connected() {
    // Now that we have connectivity, kick an immediate NTP re-poll so the clock
    // updates promptly instead of waiting for the next scheduled interval, and
    // request a fresh weather fetch.
    clock_start_sync();
    weather_client_request_fetch();

    switch (current_state) {
        case AppState::DEGRADED:
        case AppState::SETUP:
            ESP_LOGI(TAG, "WiFi connected, entering OPERATIONAL state");
            set_state(AppState::OPERATIONAL);
            break;
        default:
            break;
    }
}

void StateMachine::on_wifi_disconnected() {
    if (current_state == AppState::OPERATIONAL) {
        ESP_LOGW(TAG, "WiFi lost, entering DEGRADED state");
        set_state(AppState::DEGRADED);
    }
}

void StateMachine::set_state(AppState new_state) {
    if (current_state == new_state) return;

    ESP_LOGI(TAG, "State transition: %d -> %d", (int)current_state, (int)new_state);

    // OPERATIONAL and DEGRADED form one "scene-rotating" super-state. The dwell
    // timer must keep running across OPERATIONAL<->DEGRADED flaps (common with
    // weak Wi-Fi) — only start/stop it when entering/leaving the super-state, so
    // it isn't reset every time Wi-Fi blips.
    const bool was_rotating = is_rotating_state(current_state);
    const bool will_rotate = is_rotating_state(new_state);

    exit_state(current_state);
    current_state = new_state;
    enter_state(current_state);

    // Give the first scene a full dwell when we (re)enter the rotating super-state
    // from a non-rotating state; flaps between OPERATIONAL<->DEGRADED leave the
    // dwell running so rotation isn't reset by Wi-Fi blips.
    if (will_rotate && !was_rotating) {
        last_scene_advance_ms = millis();
    }
}

void StateMachine::enter_state(AppState state) {
    switch (state) {
        case AppState::OPERATIONAL:
            scene_switcher_set_wifi_available(true);
            break;

        case AppState::DEGRADED:
            scene_switcher_set_wifi_available(false);
            break;
            
        case AppState::SETUP:
            scene_switcher_set_wifi_available(false);
            start_captive_portal();
            break;
            
        case AppState::ERROR:
            scene_switcher_set_wifi_available(false);
            wifi_clear_credentials(); // Maybe? Or just stop trying.
            break;
            
        case AppState::SLEEPING:
            scene_switcher_set_wifi_available(false);
            panel_clear();
            panel_commit(); // Ensure off
            enter_light_sleep();
            // After wake up:
            set_state(AppState::DEGRADED); // Safe default?
            break;
    }
}

void StateMachine::exit_state(AppState state) {
    switch (state) {
        case AppState::SETUP:
            stop_captive_portal();
            break;
        default:
            break;
    }
    // Note: the scene dwell timer is managed in set_state() across the
    // OPERATIONAL/DEGRADED super-state, not started/stopped here.
}

void StateMachine::on_button_short_press() {
    switch (current_state) {
        case AppState::SLEEPING:
            // Already handled by wake up? 
            // If we are in the loop, we are not sleeping.
            break;
            
        case AppState::OPERATIONAL:
        case AppState::DEGRADED:
            next_scene();
            reset_scene_dwell(); // manual switch gets a full dwell interval
            break;

        case AppState::ERROR:
            set_state(AppState::SETUP);
            break;
            
        default:
            break;
    }
}

void StateMachine::on_button_long_press() {
    // In all states, long press -> Reset
    // Except maybe SLEEPING?
    ESP_LOGI(TAG, "Long press - Resetting");
    wifi_clear_credentials();
    esp_restart();
}

void StateMachine::on_button_double_press() {
    switch (current_state) {
        case AppState::OPERATIONAL:
        case AppState::DEGRADED:
            // Toggle automatic scene rotation. When off, only single presses
            // advance scenes.
            auto_advance_enabled = !auto_advance_enabled;
            ESP_LOGI(TAG, "Auto scene-switching %s",
                     auto_advance_enabled ? "ENABLED" : "DISABLED");
            if (auto_advance_enabled) {
                reset_scene_dwell(); // give the current scene a full interval
            }
            break;

        case AppState::ERROR:
            set_state(AppState::DEGRADED);
            break;

        default:
            break;
    }
}
