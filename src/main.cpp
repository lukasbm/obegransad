#include <Arduino.h>
#include <OneButton.h>
#include <esp_timer.h>

#include "weather.h"
#include "device.h"
#include "config.h"
#include "led.h"
#include "sprites/wifi.hpp"
#include "clock.h"
#include "scenes/scene_anniversary.hpp"
#include "scenes/scene_brightness.hpp"
#include "scenes/scene_clock_second_ring.hpp"
#include "scenes/scene_clock.hpp"
#include "scenes/scene_empty.hpp"
#include "scenes/scene_snake.hpp"
#include "scenes/scene_test.hpp"
#include "scenes/scene_weather_forecast.hpp"
#include "scenes/scene_weather_minmax.hpp"
#include "scenes/scene_weather.hpp"
#include "scenes/switcher.hpp"
#include "scenes/scene_concentric_circles.hpp"

// button definitions
OneButton button;
void buttonSetup();
void buttonSingleClick();
void buttonLongPressStart();
void buttonLongPressStop();
int buttonLongPressTimer = 0;

// all scenes live here, in RAM
AnniversaryScene anniversaryScene;
BrightnessScenes brightnessScene;
ClockSceneWithSecondHand clockSceneSecond;
ClockScene clockScene;
EmptyScene emptyScene;
SnakeScene snakeScene;
WeatherForecastScene weatherForecastScene;
WeatherMinMaxScene weatherMinMaxScene;
WeatherScene weatherScene;
ConcentricCircleScene concentricCircleScene;

// switcher
constexpr size_t NUM_SCENES = 3; // number of scenes
SceneSwitcher<NUM_SCENES> sceneSwitcher(
    std::array<Scene *, NUM_SCENES>{
        &snakeScene,
        &clockSceneSecond,
        &concentricCircleScene,
    });

static void conduct_checks();

/* ULTRA FAST panel refresh 500 Hz */
void IRAM_ATTR panel_isr(void)
{
    panel_show();
}
void start_panel_timer()
{
    hw_timer_t *t = timerBegin(0, 80, true); // 1 µs tick (80 MHz / 80 = 1 MHz)
    timerAttachInterrupt(t, panel_isr, true);
    timerAlarmWrite(t, 2000, true); // 2 ms
    timerAlarmEnable(t);
}

////////////////
//////// ARDUINO
////////////////

void setup()
{
    Serial.begin(115200);
    Serial.println("Starting setup...");

    buttonSetup();

    panel_init();

    display_wifi_setup_prompt();
    wifi_setup();

    // accept new configs
    // setup_config_server();

    start_panel_timer();

    Serial.println("Setup done!");

    conduct_checks();

    // Start with the first scene
    sceneSwitcher.nextScene();
}

void loop()
{
    static uint32_t lastSceneTick = 0;
    static uint32_t lastWeatherTick = 0;
    static uint32_t lastNTPTick = 0;
    static uint32_t lastCheck = 0;

    auto now = millis();

    auto hasWiFi = wifi_check();

    if (now - lastCheck > 10000) // check every 10 seconds
    {
        conduct_checks();
        lastCheck = millis();
    }

    if (now - lastWeatherTick > 600000 && hasWiFi) // check weather every 10 minutes
    {
        Serial.println("Checking weather...");
        // FIXME: weather_update();  // update global weather data
        lastWeatherTick = millis();
    }

    // FIXME: not needed now, fetch_time takes care of it
    // if (now - lastNTPTick > 3600000 && hasWiFi) // sync time every hour
    // {
    //     Serial.println("Syncing time with NTP...");
    //     time_syncNTP();
    //     lastNTPTick = millis();
    // }

    if (now - lastSceneTick > 100) // update scene every 100 ms (10FPS)
    {
        lastSceneTick = millis();
    }

    // Update the button
    button.tick();

    // tick the wifi manager (as it is non-blocking)
    wifiManager.process();

    // Update the current scene
    sceneSwitcher.tick();

    delay(10); // yield to other (low prio) tasks
}

static void conduct_checks()
{
    Serial.println("Conducting checks...");
    struct tm time = time_fetch();

    // adjust brightness
    uint8_t brightness = isNight(time) ? settings.brightness_night : settings.brightness_day;
    Serial.printf("Setting brightness to %d\n", brightness);
    panel_setBrightness(brightness);

    // also check if it is time to shut off!
    // if (shouldTurnOff(time))
    // {
    //   enter_light_sleep() // TODO: make it also return the sleep duration
    // }

    // check wifi health
    if (!wifi_check())
    {
        Serial.println("Wi-Fi is not connected, trying to reconnect …");
    }
    else
    {
        Serial.println("Wi-Fi is healthy.");
    }
}

///// button stuff

void buttonSetup()
{
    pinMode(BUTTON_PIN, INPUT_PULLUP); // set the button pin as input with pull-up resistor
    button.setup(
        BUTTON_PIN,   // Input pin for the button
        INPUT_PULLUP, // INPUT and enable the internal pull-up resistor
        true          // Active low button (pressed = LOW)
    );
    button.attachClick(buttonSingleClick);
    button.attachLongPressStart(buttonLongPressStart);
    button.attachLongPressStop(buttonLongPressStop);
}

void buttonSingleClick()
{
    Serial.println("Button - Single click -> next scene");
    sceneSwitcher.nextScene();
}

void buttonLongPressStart()
{
    Serial.println("Button - Long press start");
    buttonLongPressTimer = millis();
}

void buttonLongPressStop()
{
    Serial.println("Button - Long press stop");
    int duration = millis() - buttonLongPressTimer;
    Serial.printf("Button long press duration: %d ms\n", duration);
    if (duration > 10000)
    {
        Serial.println("Button long press -> reset WiFi credentials");
        wifi_clear_credentials();
    }
}
