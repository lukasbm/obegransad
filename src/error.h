#pragma once

enum DeviceError
{
    ERR_NONE = 0,
    ERR_WIFI,
    ERR_SLEEP,
    ERR_LITTLE_FS,
    ERR_PREFERENCES,
    ERR_JSON,
    ERR_UNKNOWN, // catch-all for unknown errors
};

void display_wifi_setup_prompt(void);
void display_device_error(DeviceError err);
