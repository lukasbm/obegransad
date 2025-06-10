#include "config.h"
#include <Preferences.h>

Settings settings = Settings();

static const char *PREF_NAME = "app"; // preferences namespace

// implicitly sets default values.
void read_from_persistent_storage(Settings &settings)
{
    Preferences p;
    p.begin(PREF_NAME, true); // read-only mode
    // setup
    settings.initial_setup_done = p.getBool("initial_setup_done", false); // default is false, so captive portal will open on first boot
    // wifi
    settings.ssid = p.getString("wifi_ssid", "");
    settings.password = p.getString("wifi_password", "");
    // panel
    settings.brightness_day = p.getUChar("brightness_day", 200);
    settings.brightness_night = p.getUChar("brightness_night", 50);
    // offtime 1
    settings.offtime1.from_hour = p.getUChar("offtime1_from_hour", 0);
    settings.offtime1.from_minute = p.getUChar("offtime1_from_minute", 0);
    settings.offtime1.to_hour = p.getUChar("offtime1_to_hour", 0);
    settings.offtime1.to_minute = p.getUChar("offtime1_to_minute", 0);
    settings.offtime1.days = p.getUChar("offtime1_days", 0b0); // no days
    // offtime 2
    settings.offtime2.from_hour = p.getUChar("offtime2_from_hour", 0);
    settings.offtime2.from_minute = p.getUChar("offtime2_from_minute", 0);
    settings.offtime2.to_hour = p.getUChar("offtime2_to_hour", 0);
    settings.offtime2.to_minute = p.getUChar("offtime2_to_minute", 0);
    settings.offtime2.days = p.getUChar("offtime2_days", 0b0); // no days
    // offtime 3
    settings.offtime3.from_hour = p.getUChar("offtime3_from_hour", 0);
    settings.offtime3.from_minute = p.getUChar("offtime3_from_minute", 0);
    settings.offtime3.to_hour = p.getUChar("offtime3_to_hour", 0);
    settings.offtime3.to_minute = p.getUChar("offtime3_to_minute", 0);
    settings.offtime3.days = p.getUChar("offtime3_days", 0b0); // no days
    // weather
    settings.weather_latitude = p.getDouble("weather_latitude", 0.0);   // equator
    settings.weather_longitude = p.getDouble("weather_longitude", 0.0); // prime meridian
    // timezone
    settings.timezone = p.getString("timezone", "UTC");
    // anniversary
    settings.anniversary_day = p.getUChar("anniversary_day", 1);
    settings.anniversary_month = p.getUChar("anniversary_month", 1);

    p.end();
}

void write_to_persistent_storage(Settings &settings)
{
    Preferences p;
    p.begin(PREF_NAME, false); // read-write mode
    // wifi
    p.putString("wifi_ssid", settings.ssid);
    p.putString("wifi_password", settings.password);
    // panel
    p.putUChar("brightness_day", settings.brightness_day);
    p.putUChar("brightness_night", settings.brightness_night);
    // offtime 1
    p.putUChar("offtime1_from_hour", settings.offtime1.from_hour);
    p.putUChar("offtime1_from_minute", settings.offtime1.from_minute);
    p.putUChar("offtime1_to_hour", settings.offtime1.to_hour);
    p.putUChar("offtime1_to_minute", settings.offtime1.to_minute);
    p.putUChar("offtime1_days", settings.offtime1.days);
    // offtime 2
    p.putUChar("offtime2_from_hour", settings.offtime2.from_hour);
    p.putUChar("offtime2_from_minute", settings.offtime2.from_minute);
    p.putUChar("offtime2_to_hour", settings.offtime2.to_hour);
    p.putUChar("offtime2_to_minute", settings.offtime2.to_minute);
    p.putUChar("offtime2_days", settings.offtime2.days);
    // offtime 3
    p.putUChar("offtime3_from_hour", settings.offtime3.from_hour);
    p.putUChar("offtime3_from_minute", settings.offtime3.from_minute);
    p.putUChar("offtime3_to_hour", settings.offtime3.to_hour);
    p.putUChar("offtime3_to_minute", settings.offtime3.to_minute);
    p.putUChar("offtime3_days", settings.offtime3.days);
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

bool OffTime::isInside(const struct tm &time) const
{

    if ((days & (1 << time.tm_wday)) == 0)
        return false; // day is not in the range

    // check if the time is in the range
    if (time.tm_hour < from_hour || time.tm_hour > to_hour)
        return false; // time is not in the range
    if (time.tm_hour == from_hour && time.tm_min < from_minute)
        return false; // time is not in the range
    if (time.tm_hour == to_hour && time.tm_min > to_minute)
        return false; // time is not in the range
    return true;      // time is in the range
}
