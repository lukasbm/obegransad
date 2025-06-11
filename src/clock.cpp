#include "clock.h"
#include "config.h"

void time_syncNTP()
{
    configTzTime(gSettings.timezone.c_str(), MY_NTP_SERVER);
    struct tm temp;
    // check if we can get the local time
    if (!getLocalTime(&temp, 2000))
    {
        Serial.println("Failed to obtain time");
        return;
    }
}

// common night time hours, only used if no wifi is present
bool time_isNight(struct tm const &time)
{
    if (time.tm_hour >= 22 || time.tm_hour < 6)
    {
        return true; // night
    }
    return false; // day
}

bool shouldTurnOff(struct tm const &time)
{
    return gSettings.off_hours & (1 << time.tm_hour);
}

struct tm time_get()
{
    struct tm timeinfo;
    getLocalTime(&timeinfo, 200);
    return timeinfo;
}

// https://fcds.cs.put.poznan.pl/MyWeb/Praca/Ubiquitous/LunarPhases.pdf
MoonPhase getMoonPhase(struct tm &time)
{
    return FULL; // TODO: implement this properly for moon calendar scene?
}
