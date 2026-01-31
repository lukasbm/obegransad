enum State {
    STATE_OFFLINE,
    STATE_ONLINE,
    STATE_STARTUP,
    STATE_SLEEP,
    STATE_ERROR
};


enum Services {
    SERVICE_CAPTIVE_PORTAL,
    SERVICE_SERVER,
    SERVICE_CLOCK,
    SERVICE_SCENES,
};


enum Peripherals {
    PERIPHERAL_WIFI,
    PERIPHERAL_PANEL,
    PERIPHERAL_BUTTON,
    // maybe NVS?
};


State get_current_state();
