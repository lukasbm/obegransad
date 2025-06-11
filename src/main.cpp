#include <Arduino.h>
#include <OneButton.h>

#include "weather.h"
#include "device.h"
#include "config.h"
#include "led.h"
#include "sprites/wifi.hpp"
#include "clock.h"
#include "server.h"
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
#include "scenes/fireworks.hpp"
#include "scenes/game_of_life.hpp"

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
FireworksScene fireworksScene;
GameOfLifeScene gameOfLifeScene;

// other components
SettingsServer settingsServer; // handles settings via web server

enum State
{
    STATE_NORMAL = 0,     // normal operation, wifi connected, no captive portal, settings server on
    STATE_CAPTIVE_PORTAL, // captive portal active, wifi disconnected
    STATE_NO_WIFI,        // wifi disconnected, captive portal not active, settings server running but not accessible
    STATE_SETUP           // settings server not running, captive portal not yet active
};
static State state = STATE_SETUP;

/* ULTRA FAST panel refresh 500 Hz */
static hw_timer_t *panelTimer = nullptr;
void IRAM_ATTR panel_isr(void)
{
    panel_show();
}
void start_panel_timer()
{
    panelTimer = timerBegin(0, 80, true); // 1 µs tick (80 MHz / 80 = 1 MHz)
    timerAttachInterrupt(panelTimer, &panel_isr, true);
    timerAlarmWrite(panelTimer, 2000, true); // 2 ms
    timerAlarmEnable(panelTimer);
}
void stop_panel_timer()
{
    if (panelTimer)
    {
        // timerDetachInterrupt(panelTimer);
        // timerEnd(panelTimer);
        timerAlarmDisable(panelTimer);
        panelTimer = nullptr;
    }
}

// switcher
constexpr size_t NUM_SCENES = 4; // number of scenes
SceneSwitcher<NUM_SCENES> sceneSwitcher(
    std::array<Scene *, NUM_SCENES>{
        &snakeScene,
        &clockSceneSecond,
        &concentricCircleScene,
        &gameOfLifeScene});

static void conduct_checks();

void setup()
{
    delay(3000); // wait for serial monitor to connect, otherwise monitor serial will not work
    Serial.begin(115200);
    Serial.println("Starting setup...");

    buttonSetup();               // set up the button
    panel_init();                // initialize the LED panel
    display_wifi_setup_prompt(); // show the Wi-Fi logo
    wifi_setup();                // start captive portal if no Wi-Fi credentials are stored

    // only switch to first scene once we have state NORMAL or NO_WIFI, keep wifi logo as long as we are in SETUP or CAPTIVE_PORTAL state
}

//     // NTP sync
//     time_setup();
//     // accept new configs
//     settingsServer.start();
//     Serial.println("Setup done!");
//     conduct_checks();
//     // Start with the first scene
//     sceneSwitcher.nextScene();
// }

// TODO: basically implement a state machine here (based on STATE)
void loop()
{
    static unsigned long lastChecks = millis();
    if (millis() - lastChecks > 10000) // check every 10 seconds
    {
        conduct_checks();
        lastChecks = millis();
    }

    // Update the button
    button.tick();

    // Update the current scene
    sceneSwitcher.tick();

    captive_portal_tick(); // handle captive portal events

    // refresh the display
    panel_show();
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

static void conduct_checks()
{
    Serial.println("Conducting checks...");
    struct tm time = time_fetch();

    // adjust brightness
    gBright = isNight(time) ? gSettings.brightness_night : gSettings.brightness_day;

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
