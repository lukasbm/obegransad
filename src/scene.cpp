#include <Arduino.h>
#include "led.cpp"
#include "time.cpp"
#include "config.cpp"
#include "weather.cpp"
class SceneSwitcher
{
private:
    Scene scenes[2] = {
        WeatherScene(),
        ClockScene()};
    int currentSceneIndex = -1; // -1 means no scene is active

public:
    void nextScene()
    {
        if (currentSceneIndex != -1)
        {
            scenes[currentSceneIndex].deactivate();
        }
        currentSceneIndex = (currentSceneIndex + 1) % 2;
        Serial.printf("Switching to scene %d\n", currentSceneIndex);
        scenes[currentSceneIndex].activate();
    }
    void tick()
    {
        scenes[currentSceneIndex].update();
    }
};

class Scene
{
public:
    virtual void activate();
    virtual void deactivate();
    virtual void update();
};

class WeatherScene : public Scene
{
private:
    struct WeatherData weatherData;
    int secondsSinceLastFetch = 0;

    void drawWeatherData()
    {
        // TODO:
    }

public:
    void activate() override
    {
        weatherData = fetchWeather(settings.weather_latitude, settings.weather_longitude);
    }

    void update() override
    {
        if (fullSecond)
        {
            secondsSinceLastFetch++;
            if (secondsSinceLastFetch >= settings.weather_update_interval)
            {
                weatherData = fetchWeather(settings.weather_latitude, settings.weather_longitude);
                secondsSinceLastFetch = 0;
            }

            drawWeatherData();
        }
    }
};

class ClockScene : public Scene
{
private:
    void print_time(uint8_t hour, uint8_t minute)
    {
        panel_clear();
        panel_printChar(2, 0, (hour / 10) + 48);
        panel_printChar(9, 0, (hour % 10) + 48);
        panel_printChar(2, 9, (minute / 10) + 48);
        panel_printChar(9, 9, (minute % 10) + 48);
        panel_show(1, settings.brightness); // refreshes display
    }

public:
    void activate() override
    {
        panel_debugTest();
        print_time(hour, minute);
    }

    void update() override
    {
        // JEDE SEKUNDE
        if (millis() > mil + 1000)
        {
            mil = millis();
            // PRINT THE TIME

            panel_printChar(2, 0, (hour / 10) + 48);
            panel_printChar(9, 0, (hour % 10) + 48);
            panel_printChar(2, 9, (minute / 10) + 48);
            panel_printChar(9, 9, (minute % 10) + 48);
            panel_show(); // refreshes display

            // JEDE MINUTE
            sec++;
            // every minute set the time
            if (sec > 60)
            {
                sec = 0;
                set_clock_from_tm();
                set_ntp_time();
            }
            Serial.printf("Current time: %s\n", getTimeString());
        }
    }
};
