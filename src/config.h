#pragma once

#include <Arduino.h>
#include <Preferences.h>

// trying to use the names consistent between this struct, json and preferences (NVS)
struct Settings
{
    uint8_t brightness_day, brightness_night;
    uint32_t off_hours; // bit mask of 24 bits, each bit represents an hour of the day (0-23), LSB is 0:00, "MSB" is 23:00
    double weather_latitude, weather_longitude;
    String timezone;
    uint8_t anniversary_day, anniversary_month;
    // TODO: game_of_life initial state

    // central validation
    bool valid() const
    {
        auto geoOk = [](double la, double lo)
        {
            return la >= -90 && la <= 90 && lo >= -180 && lo <= 180;
        };

        // skipping timezone validation for now, as it is a string and can be anything
        return geoOk(weather_latitude, weather_longitude) &&
               anniversary_day >= 1 && anniversary_day <= 31 &&
               anniversary_month >= 1 && anniversary_month <= 12;
    }
};

// persistent storage functions
void write_to_persistent_storage(const Settings &settings);
Settings read_from_persistent_storage();
void clear_persistent_storage(void);

extern Settings gSettings;
