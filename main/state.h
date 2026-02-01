#pragma once

#include <cstdint>

enum class AppState {
    SLEEPING,   // Display Off, Chip Sleeping, Wi-Fi Off
    SETUP,      // Display On, Chip On, Wi-Fi Captive Portal
    OPERATIONAL,// Display On, Chip On, Wi-Fi Connected
    ERROR,      // Display On, Chip On, Wi-Fi Off/Error
    DEGRADED    // Display On, Chip On, Wi-Fi Disconnected (Temporary)
};

class StateMachine {
public:
    static StateMachine& instance();

    void init();
    void update(); // Called periodically from main loop

    // Input events
    void on_button_short_press();
    void on_button_long_press();
    void on_button_double_press();

    AppState get_state() const { return current_state; }

private:
    StateMachine() = default;
    void set_state(AppState new_state);
    
    // State handlers
    void enter_state(AppState state);
    void exit_state(AppState state);
    void update_state(AppState state);

    AppState current_state = AppState::SLEEPING; // Default, will change in init
    
    // Helpers
    void draw_icon(const char* name); // Placeholder for drawing status icons
};
