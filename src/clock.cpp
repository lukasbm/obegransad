#include "clock.h"

static struct tm timeinfo;

void time_setup()
{
    // setenv("TZ", MY_TZ, 1); // set timezone
    tzset();                // apply timezone
}

void time_syncNTP()
{
    // configTime(0, 0, MY_NTP_SERVER); // set NTP server
    if (!getLocalTime(&timeinfo, 2000))
    {
        Serial.println("Failed to obtain time");
        return;
    }
    Serial.println("NTP time synchronized.");
}

static void time_fetch()
{
    getLocalTime(&timeinfo, 2000);
}

int time_hour()
{
    time_fetch();
    return timeinfo.tm_hour;
}

int time_minute()
{
    time_fetch();
    return timeinfo.tm_min;
}

int time_second()
{
    time_fetch();
    return timeinfo.tm_sec;
}

bool isNight()
{
    time_fetch();
    if (timeinfo.tm_hour >= 22 || timeinfo.tm_hour < 6)
    {
        return true; // night
    }
    return false; // day
}

const char *getTimeString()
{
    static char timeString[20];
    snprintf(timeString, sizeof(timeString), "%02d:%02d:%02d", time_hour(), time_minute(), time_second());
    return timeString;
}

struct tm time_full()
{
    time_fetch();
    return timeinfo;
}
