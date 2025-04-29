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
    int currentSceneIndex = 0;

    SceneSwitcher(void)
    {
        // start with first scene
        scenes[0].activate();
    }

public:
    void nextScene()
    {
        scenes[currentSceneIndex].deactivate();
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
    long mil;
    uint8_t sec;
    uint8_t minute;
    uint8_t hour;
    time_t now; // this is the epoch
    tm tm;

public:
    void activate() override
    {
    }

    void update() override
    {
        // JEDE SEKUNDE
        if (millis() > mil + 1000)
        {
            mil = millis();
            // PRINT THE TIME

            p_printChar(2, 0, (hour / 10) + 48);
            p_printChar(9, 0, (hour % 10) + 48);
            p_printChar(2, 9, (minute / 10) + 48);
            p_printChar(9, 9, (minute % 10) + 48);
            panel_scan(); // refreshes display

            // JEDE MINUTE
            sec++;
            // every minute set the time
            if (sec > 60)
            {
                sec = 0;
                set_clock_from_tm();
                set_clock();
            }
            Serial.printf("Current time: %s\n", getTimeString());
        }

        // TODO: move somewhere else! BUTTON
        if (digitalRead(P_KEY) == 0)
        {
            brightness += 50;
            if (brightness > 200)
                brightness = 0;
            analogWrite(P_EN, brightness); // full brightness
            delay(500);
        }
    }
};
