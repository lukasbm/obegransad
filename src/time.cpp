#include <time.h>
#include <string.h>

#define NTP_SERVER "pool.ntp.org"

static time_t now;
static struct tm timeinfo;

struct tm *getTime()
{
    time(&now);
    localtime_r(&now, &timeinfo);
    return &timeinfo;
}

void setTime()
{
    configTime(0, 0, NTP_SERVER);
    Serial.println("Waiting for time sync...");
    while (!getLocalTime(&timeinfo, 1000))
    {
        Serial.print(".");
        delay(1000);
    }
    Serial.println("");
    Serial.println("Time synced!");
    Serial.print("Current time: ");
    Serial.println(asctime(&timeinfo));
}

const char *getTimeString(void)
{
    static char acTimeString[32];
    time_t now = time(nullptr);
    ctime_r(&now, acTimeString);
    size_t stLength;
    while (((stLength = strlen(acTimeString))) &&
           ('\n' == acTimeString[stLength - 1]))
    {
        acTimeString[stLength - 1] = 0; // Remove trailing line break...
    }
    return acTimeString;
}

void set_clock(void)
{

    // configTime(((MY_TIMEZ) * 3600), (DST_OFFSET * 3600), "pool.ntp.org", "time.nist.gov", "time.windows.com");
    configTime(MY_TZ, MY_NTP_SERVER); // --> Here is the IMPORTANT ONE LINER needed in your sketch!

    Serial.print("Waiting for NTP time sync: ");
    time_t now = time(nullptr); // Secs since 01.01.1970 (when uninitialized starts with (8 * 3600 = 28800)
    while (now < 8 * 3600 * 2)
    { // Wait for realistic value

        Serial.print(".");
        now = time(nullptr);
    }
    Serial.println("");
    Serial.printf("Current time: %s\n", getTimeString());
}

void set_clock_from_tm()
{
    time(&now);             // read the current time
    localtime_r(&now, &tm); // update the structure tm with the current time

    // update time from struct
    minute = tm.tm_min;
    hour = tm.tm_hour;
}

void setup_clock()
{
    setTime();
}