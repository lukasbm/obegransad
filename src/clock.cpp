#include "clock.h"
#include "config.h"

void time_setup()
{
    time_syncNTP();
}

void time_syncNTP()
{
    configTzTime(gSettings.timezone.c_str(), MY_NTP_SERVER);
    struct tm temp;
    if (!getLocalTime(&temp, 2000))
    {
        Serial.println("Failed to obtain time");
        return;
    }
}

// FIXME: ditch this function and use sunrise and sunset times from weather API (only use this if no wifi)
bool isNight(struct tm const &time)
{
    if (time.tm_hour >= 22 || time.tm_hour < 6)
    {
        return true; // night
    }
    return false; // day
}

bool shouldTurnOff(struct tm &time)
{
    return true; // TODO: implement the logic to check if the current time is in the off hours
}

struct tm time_fetch()
{
    static time_t lastFetch = 0;
    static time_t now = 0;
    time(&now);                          // get the current time
    if (difftime(now, lastFetch) > 3600) // if more than an hour has passed
    {
        time_syncNTP();
        lastFetch = now; // update the last fetch time
    }
    // fetch the time again.
    struct tm timeinfo;
    getLocalTime(&timeinfo, 2000);
    return timeinfo;
}

// https://fcds.cs.put.poznan.pl/MyWeb/Praca/Ubiquitous/LunarPhases.pdf
MoonPhase getMoonPhase(struct tm &time)
{
    return FULL;
}
