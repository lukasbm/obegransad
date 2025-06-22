#include <unity.h>
#include "weather.h"

void setUp(void)
{
    // set stuff up here
}

void tearDown(void)
{
    // clean stuff up here
}

void test_weather_important_fields_present()
{
    WeatherData data = weather_fetch(49.4542, 11.0775); // Nürnberg.

    TEST_ASSERT_GREATER_THAN(-30, data.temperature);
}

int main(int argc, char **argv)
{
    UNITY_BEGIN();
    RUN_TEST(test_weather_important_fields_present);
    UNITY_END();
}
