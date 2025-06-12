#include "config.h"

static const char *PREF_NAME = "app"; // preferences namespace

Settings gSettings; // global settings object, initialized with default values

// implicitly sets default values.
Settings read_from_persistent_storage()
{
    Preferences p;
    p.begin(PREF_NAME, false); // always open in read-write mode, so that non-existing repositories can be created

    Settings settings;
    // panel
    settings.brightness_day = p.getUChar("brightness_day", 200);
    settings.brightness_night = p.getUChar("brightness_night", 50);
    // offtime
    settings.off_hours = p.getUInt("off_hours", 0x00000000); // default: no off hours (all bits are 0)
    // weather
    settings.weather_latitude = p.getDouble("weather_latitude", 0.0);   // equator
    settings.weather_longitude = p.getDouble("weather_longitude", 0.0); // prime meridian
    // timezone
    settings.timezone = p.getString("timezone", "UTC");
    // anniversary
    settings.anniversary_day = p.getUChar("anniversary_day", 1);
    settings.anniversary_month = p.getUChar("anniversary_month", 1);

    p.end();

    return settings;
}

void write_to_persistent_storage(const Settings &settings)
{
    Preferences p;
    p.begin(PREF_NAME, false); // read-write mode
    // panel
    p.putUChar("brightness_day", settings.brightness_day);
    p.putUChar("brightness_night", settings.brightness_night);
    // offtime
    p.putUInt("off_hours", settings.off_hours); // bit mask of 24 bits, each bit represents an hour of the day (0-23)
    // weather
    p.putDouble("weather_latitude", settings.weather_latitude);
    p.putDouble("weather_longitude", settings.weather_longitude);
    // timezone
    p.putString("timezone", settings.timezone);
    // anniversary
    p.putUChar("anniversary_day", settings.anniversary_day);
    p.putUChar("anniversary_month", settings.anniversary_month);

    p.end();
}

void clear_persistent_storage(void)
{
    Preferences p;
    p.begin(PREF_NAME, false); // read-write mode
    p.clear();                 // clear all keys
    p.end();
}
