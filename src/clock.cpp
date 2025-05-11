#include "clock.h"
#include "config.h"

void time_setup()
{
    time_syncNTP();
}

void time_syncNTP()
{
    configTzTime(settings.timezone.c_str(), MY_NTP_SERVER);
    struct tm temp;
    if (!getLocalTime(&temp, 2000))
    {
        Serial.println("Failed to obtain time");
        return;
    }
}

bool isNight(struct tm time)
{
    if (time.tm_hour >= 22 || time.tm_hour < 6)
    {
        return true; // night
    }
    return false; // day
}

static bool checkOffTimes(struct tm time, const std::vector<OffTime> &offTimes)
{
    for (const auto &offTime : offTimes)
    {
        if ((time.tm_hour == offTime.from_hour && time.tm_min >= offTime.from_minute) ||
            (time.tm_hour == offTime.to_hour && time.tm_min <= offTime.to_minute) ||
            (time.tm_hour > offTime.from_hour && time.tm_hour < offTime.to_hour))
        {
            return true; // turn off
        }
    }
    return false; // do not turn off
}

bool shouldTurnOff(struct tm time)
{
    // check if the time is in the off time range
    if (checkOffTimes(time, settings.off_time_everyday))
    {
        return true;
    }
    if (checkOffTimes(time, settings.off_time_weekdays) && time.tm_wday != 0 && time.tm_wday != 6)
    {
        return true;
    }
    if (checkOffTimes(time, settings.off_time_weekends) && (time.tm_wday == 0 || time.tm_wday == 6))
    {
        return true;
    }
    return false; // do not turn off
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
