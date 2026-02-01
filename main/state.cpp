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
            }
            tick(); // Update scenes
            break;
            
        case AppState::DEGRADED:
            if (wifi_check()) {
                ESP_LOGI(TAG, "WiFi restored, entering OPERATIONAL state");
                set_state(AppState::OPERATIONAL);
            }
            tick(); // Update scenes
            
            // Draw warning dot (Top Right Pixel)
            panel_setPixel(15, 0, PANEL_BRIGHTNESS_1);
            break;
            
        case AppState::SETUP:
            // In SETUP, we show the WiFi icon
            panel_clear();
            wifi_sprite.draw(0, 0); // centered? WiFi sprite is likely 16x16 or smaller
            // If it's smaller we might need centering, but let's assume 0,0 for now
            // Check if user configured wifi? 
            // Usually CP will handle creds and we might need a reboot or check.
            if (wifi_has_credentials()) {
                 // If creds appeared, maybe user saved them. 
                 // We could try to connect or just reboot.
                 // Doc says "When setup succeeds, we enter OPERATIONAL state."
                 // But typically we need to switch from AP to Station.
                 // device.cpp wifi_init() handles this logic on boot.
                 // Runtime switching might be complex. 
                 // For now, let's just stay in SETUP until reboot or manual transition.
            }
            break;

        case AppState::ERROR:
            panel_clear();
            // Draw '!'
            font_bold.draw_char(4, 4, '!'); 
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
            // Ensure server is on (if implemented)
            start_webserver(); 
            // Ensure Station is active (it should be if we are here)
            break;
            
        case AppState::DEGRADED:
            // Stop server if needed
            stop_webserver();
            break;
            
        case AppState::SETUP:
            start_captive_portal();
            break;
            
        case AppState::ERROR:
            wifi_clear_credentials(); // Maybe? Or just stop trying.
            break;
            
        case AppState::SLEEPING:
            panel_clear();
            panel_commit(); // Ensure off
            enter_light_sleep();
            // After wake up:
            // We need to decide where to go.
            // For now, let's just reset to init logic or assume we wake up in previous intention?
            // Usually reset triggers reboot or we wake up and continue.
            // If we continue, we should probably go to DEGRADED or OPERATIONAL.
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
