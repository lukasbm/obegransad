#include "state.h"

#include "app_events.h"
#include "clock.h"
#include "device.h"
#include "helper.hpp" // millis()
#include "overlays.hpp"
#include "popup.h"
#include "scene_switcher.h"
#include "weather_client.h"
#include "ikea-obegransad-panel.h"

// Sprites
#include "sprites/bold_glyphs.hpp"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <sdkconfig.h>

static const char* TAG = "StateMachine";

// How long the preset-number / Wi-Fi popups stay on screen (from Kconfig).
static constexpr uint32_t POPUP_MS = CONFIG_OBG_POPUP_MS;

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

void StateMachine::show_preset_popup() {
    popup_show(overlay_preset_number(preset_current()), POPUP_MS);
    reset_scene_dwell(); // the new preset's first scene gets a full interval
}

void StateMachine::announce_preset() { show_preset_popup(); }

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

    // Determine the initial state by polling once; subsequent changes arrive
    // as events. With no credentials configured the device simply stays in
    // DEGRADED (there is no provisioning mode anymore).
    set_state(wifi_check() ? AppState::OPERATIONAL : AppState::DEGRADED);
}

void StateMachine::update() {
    // Apply any pending events first, then render for the (possibly new) state.
    process_events();

    // A popup owns the whole panel while it is up; nothing else draws.
    if (popup_is_active()) {
        popup_render();
        popup_was_active = true;
        return;
    }
    if (popup_was_active) {
        popup_was_active = false;
        scene_force_redraw();  // the popup overwrote the scene's frame
        reset_scene_dwell();   // and the scene gets a full interval
    }

    // Automatic scene rotation within the active preset while in a display state
    // (OPERATIONAL/DEGRADED). Driven here on the main task so it survives Wi-Fi
    // flaps and needs no timer. A preset with dwell_ms == 0 never rotates.
    const uint32_t dwell_ms = preset_dwell_ms();
    if (dwell_ms > 0 && is_rotating_state(current_state) &&
        (millis() - last_scene_advance_ms) >= dwell_ms) {
        rotation_advance();
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

            // Draw warning dot (bottom-left pixel) - Overlay
            panel_setPixel(15, 0, PANEL_BRIGHTNESS_1);
            panel_commit(); // Commit overlay
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
                // Long press is intentionally a no-op (the factory reset was
                // removed together with the captive portal).
                ESP_LOGI(TAG, "button: long press (ignored)");
                break;
            case APP_EVT_BUTTON_DOUBLE:
                ESP_LOGI(TAG, "button: double press");
                on_button_double_press();
                break;
            case APP_EVT_TIME_SYNCED:
                ESP_LOGI(TAG, "Time synced");
                // Timestamps are meaningful now; refresh weather.
                weather_client_request_fetch();
                break;
            case APP_EVT_WEATHER_DATA_READY:
                ESP_LOGI(TAG, "Weather data ready");
                break;
            case APP_EVT_SCENE_ADVANCE:
                if (is_rotating_state(current_state)) {
                    rotation_advance();
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
    // Announce a *regained* connection only — the first connect after boot is
    // expected and needs no popup.
    if (had_connection) {
        popup_show(overlay_wifi(true), POPUP_MS);
    }
    had_connection = true;

    // Now that we have connectivity, kick an immediate NTP re-poll so the clock
    // updates promptly instead of waiting for the next scheduled interval, and
    // request a fresh weather fetch.
    clock_start_sync();
    weather_client_request_fetch();

    switch (current_state) {
        case AppState::DEGRADED:
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
        popup_show(overlay_wifi(false), POPUP_MS);
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

        case AppState::ERROR:
            scene_switcher_set_wifi_available(false);
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

void StateMachine::on_button_short_press() {
    switch (current_state) {
        case AppState::SLEEPING:
            // Already handled by wake up? 
            // If we are in the loop, we are not sleeping.
            break;
            
        case AppState::OPERATIONAL:
        case AppState::DEGRADED:
            preset_next();
            show_preset_popup();
            break;

        case AppState::ERROR:
            // Retry connectivity (short press used to leave ERROR into SETUP).
            set_state(AppState::DEGRADED);
            break;
            
        default:
            break;
    }
}

void StateMachine::on_button_double_press() {
    switch (current_state) {
        case AppState::OPERATIONAL:
        case AppState::DEGRADED:
            preset_prev();
            show_preset_popup();
            break;

        case AppState::ERROR:
            set_state(AppState::DEGRADED);
            break;

        default:
            break;
    }
}
