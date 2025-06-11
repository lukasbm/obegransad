#include "config.h"

Settings gSettings = Settings();

static const char *PREF_NAME = "app"; // preferences namespace

// implicitly sets default values.
void read_from_persistent_storage(Settings &settings)
{
    Preferences p;
    p.begin(PREF_NAME, true); // read-only mode
    // panel
    gSettings.brightness_day = p.getUChar("brightness_day", 200);
    gSettings.brightness_night = p.getUChar("brightness_night", 50);
    // offtime
    gSettings.off_hours = p.getUInt("off_hours", 0x00000000); // default: no off hours (all bits are 0)
    // weather
    gSettings.weather_latitude = p.getDouble("weather_latitude", 0.0);   // equator
    gSettings.weather_longitude = p.getDouble("weather_longitude", 0.0); // prime meridian
    // timezone
    gSettings.timezone = p.getString("timezone", "UTC");
    // anniversary
    gSettings.anniversary_day = p.getUChar("anniversary_day", 1);
    gSettings.anniversary_month = p.getUChar("anniversary_month", 1);

    p.end();
}

void write_to_persistent_storage(Settings &settings)
{
    Preferences p;
    p.begin(PREF_NAME, false); // read-write mode
    // panel
    p.putUChar("brightness_day", gSettings.brightness_day);
    p.putUChar("brightness_night", gSettings.brightness_night);
    // offtime
    p.putUInt("off_hours", gSettings.off_hours); // bit mask of 24 bits, each bit represents an hour of the day (0-23)
    // weather
    p.putDouble("weather_latitude", gSettings.weather_latitude);
    p.putDouble("weather_longitude", gSettings.weather_longitude);
    // timezone
    p.putString("timezone", gSettings.timezone);
    // anniversary
    p.putUChar("anniversary_day", gSettings.anniversary_day);
    p.putUChar("anniversary_month", gSettings.anniversary_month);

    p.end();
}

void clear_persistent_storage(void)
{
    Preferences p;
    p.begin(PREF_NAME, false); // read-write mode
    p.clear();                 // clear all keys
    p.end();
}
