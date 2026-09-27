#pragma once

#include "esp_event.h"
#include <cstdint>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

enum class AppState {
    SLEEPING,   // Display Off, Chip Sleeping, Wi-Fi Off
    SETUP,      // Display On, Chip On, no Wi-Fi credentials configured
    OPERATIONAL,// Display On, Chip On, Wi-Fi Connected
    ERROR,      // Display On, Chip On, Wi-Fi Off/Error
    DEGRADED    // Display On, Chip On, Wi-Fi Disconnected (Temporary)
};

class StateMachine {
public:
    static StateMachine& instance();

    void init();
    void update(); // Called periodically from main loop; drives scene rendering

    AppState get_state() const { return current_state; }

    // Show the active preset's number popup and restart the dwell timer. Used
    // by the button handlers and by external control (HTTP API, simulator).
    void announce_preset();

private:
    StateMachine();
    void set_state(AppState new_state);

    // Restart the auto-advance dwell so the current scene gets a full interval.
    void reset_scene_dwell();

    // Show the active preset's number fullscreen and restart the dwell.
    void show_preset_popup();

    // esp_event handler (runs in the event-loop task): enqueues events for the
    // main task so all state/scene/panel mutation stays single-threaded.
    static void on_app_event(void *arg, esp_event_base_t base, int32_t id,
                             void *data);
    void process_events(); // drain the queue from the main task

    // Event reactions (always invoked on the main task via process_events)
    void on_wifi_connected();
    void on_wifi_disconnected();
    void on_button_short_press();
    void on_button_long_press();
    void on_button_double_press();

    // State handlers
    void enter_state(AppState state);
    void exit_state(AppState state);
    void update_state(AppState state);

    QueueHandle_t event_queue = nullptr;
    AppState current_state = AppState::SLEEPING; // Default, will change in init

    // Automatic scene rotation is driven from update() on the main task (robust,
    // no esp_timer dependency). This marks when the current scene started; when
    // the active preset's dwell elapses while in a rotating state, we advance.
    unsigned long last_scene_advance_ms = 0;

    // Tracks the popup->normal edge so the scene can be told to repaint.
    bool popup_was_active = false;

    // True once Wi-Fi has connected at least since boot, so that only a
    // *regained* connection pops up.
    bool had_connection = false;
};
