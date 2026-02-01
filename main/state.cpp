#include "state.h"

#include "device.h"
#include "scene_switcher.h"
#include "server.h"
#include "ikea-obegransad-panel.h"

// Sprites
#include "sprites/wifi.hpp"
#include "sprites/bold_glyphs.hpp"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char* TAG = "StateMachine";

StateMachine& StateMachine::instance() {
    static StateMachine instance;
    return instance;
}

void StateMachine::init() {
    ESP_LOGI(TAG, "Initializing State Machine");
    
    // Determine initial state based on WiFi/Credentials
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
    // Periodic state checks
    
    switch (current_state) {
        case AppState::OPERATIONAL:
            if (!wifi_check()) {
                ESP_LOGW(TAG, "WiFi lost, entering DEGRADED state");
                set_state(AppState::DEGRADED);
                return;
            }
            tick(); // Update scenes
            // No extra commit needed as scenes usually commit. 
            // But to be safe if we add overlays later:
            // panel_commit(); 
            break;
            
        case AppState::DEGRADED:
            if (wifi_check()) {
                ESP_LOGI(TAG, "WiFi restored, entering OPERATIONAL state");
                set_state(AppState::OPERATIONAL);
                return;
            }
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
            font_bold.draw_char(4, 4, '!'); 
            panel_commit();
            break;
            
        case AppState::SLEEPING:
            // Should be sleeping
            break;
    }
}

void StateMachine::set_state(AppState new_state) {
    if (current_state == new_state) return;
    
    ESP_LOGI(TAG, "State transition: %d -> %d", (int)current_state, (int)new_state);
    
    exit_state(current_state);
    current_state = new_state;
    enter_state(current_state);
}

void StateMachine::enter_state(AppState state) {
    switch (state) {
        case AppState::OPERATIONAL:
            scene_switcher_set_wifi_available(true);
            // Ensure server is on (if implemented)
            start_webserver(); 
            // Ensure Station is active (it should be if we are here)
            break;
            
        case AppState::DEGRADED:
            scene_switcher_set_wifi_available(false);
            // Stop server if needed
            stop_webserver();
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
            // skipTo(FAVORITE); // Not implemented yet
            break;
            
        case AppState::ERROR:
            set_state(AppState::DEGRADED);
            break;
            
        default:
            break;
    }
}
