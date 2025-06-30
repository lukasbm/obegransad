#include <Arduino.h>
#include <OneButton.h>
#include <WiFi.h>

#include "weather.h"
#include "device.h"
#include "config.h"
#include "led.h"
#include "sprites/wifi.hpp"
#include "clock.h"
#include "server.h"
#include "scenes/helper.hpp"
#include "scenes/switcher.hpp"

#include "scenes/scene_anniversary.hpp"
#include "scenes/scene_clock_second_ring.hpp"
#include "scenes/scene_clock.hpp"
#include "scenes/scene_empty.hpp"
#include "scenes/scene_snake.hpp"
#include "scenes/scene_weather_forecast.hpp"
#include "scenes/scene_weather_minmax.hpp"
#include "scenes/scene_weather.hpp"
#include "scenes/scene_concentric_circles.hpp"
#include "scenes/fireworks.hpp"
#include "scenes/game_of_life.hpp"

// all scenes live here, in RAM
AnniversaryScene anniversaryScene;
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
SettingsServer settingsServer;                // handles settings via web server
time_t nextSleepDuration = 0;                 // next sleep duration in seconds, used to wake up the device from light sleep
RenderTimer timeSyncTimer(60 * 30 * 1000);    // 30 minute timer for NTP sync
RenderTimer weatherSyncTimer(60 * 30 * 1000); // 30 minute timer for weather sync

// switcher
constexpr size_t NUM_SCENES = 5; // number of scenes
SceneSwitcher<NUM_SCENES> sceneSwitcher(
    std::array<Scene *, NUM_SCENES>{
        &snakeScene,
        &clockSceneSecond,
        &concentricCircleScene,
        &gameOfLifeScene,
        &anniversaryScene});

// button definitions
OneButton button;
void buttonSetup();
void buttonSingleClick();
void buttonLongPressStart();
void buttonLongPressStop();
int buttonLongPressTimer = 0;

// state model
enum State
{
    STATE_NORMAL = 0,     // normal operation, wifi connected, no captive portal, settings server on
    STATE_CAPTIVE_PORTAL, // captive portal active, wifi disconnected
    STATE_NO_WIFI,        // wifi disconnected, captive portal not active, settings server running but not accessible
    STATE_SETUP,          // settings server not running, captive portal not yet active
    STATE_SLEEPING        // device is in light sleep mode, waiting for button press or timer wakeup (wifi off, but the sleep call is blocking anyway)
};
static State state = STATE_SETUP;

// manages the state transitions
// components to keep track of:
// - captive portal
// - settings server
// - panel (either scene or wifi logo)
// ? stuff related to sleep?
static void update_state(State next)
{
    if (state == next)
    {
        return; // no change
    }

    // we don't have to consider what the last state was if all the functions we use below are idempotent
    switch (next)
    {
    case STATE_NORMAL:
        Serial.println("State change to NORMAL");
        captive_portal_stop();
        // panel_timer_start();
        settingsServer.start();
        timeSyncTimer.reset();    // reset the timer so that we sync immediately
        weatherSyncTimer.reset(); // reset the timer so that we fetch weather immediately
        sceneSwitcher.skipTo(0);  // display a scene
        break;

    case STATE_CAPTIVE_PORTAL:
        Serial.println("State change to CAPTIVE_PORTAL");
        settingsServer.stop();
        // panel_timer_stop();
        display_wifi_logo(); // show the Wi-Fi logo
        captive_portal_start();
        break;

    case STATE_NO_WIFI:
        Serial.println("State change to NO_WIFI");
        // panel_timer_start();
        settingsServer.stop();
        captive_portal_stop();
        sceneSwitcher.skipTo(0); // display a scene
        break;

    case STATE_SLEEPING:
        Serial.println("State change to SLEEPING");
        // panel_timer_stop(); // stop the panel timer
        settingsServer.stop();
        captive_portal_stop();
        panel_clear();                        // clear the panel
        enter_light_sleep(nextSleepDuration); // enter light sleep
        break;
    }

    state = next;
}

void setup()
{
    delay(3000); // wait for serial monitor to connect, otherwise monitor serial will not work
    Serial.begin(115200);
    Serial.println("Starting setup...");

    WiFi.mode(WIFI_STA); // set Wi-Fi mode to station (initially, otherwise will be STA+AP) // has to be here at the beginning, otherwise it blocks?

    // state independent setup code
    captive_portal_setup(); // set up Wi-Fi, will start captive portal if no credentials are stored
    gSettings = read_from_persistent_storage();
    buttonSetup(); // set up the button
    panel_init();  // initialize the LED panel
    Serial.println("Setup done, checking Wi-Fi...");

    if (wifi_setup())
    {
        // already connected
        Serial.println("Wi-Fi connected");
        update_state(STATE_NORMAL);
    }
    else
    {
        // not connected, start captive portal
        Serial.println("Wi-Fi not connected, starting captive portal");
        update_state(STATE_CAPTIVE_PORTAL);
    }

    Serial.println("Setup complete, entering main loop...");

    // only switch to first scene once we have state NORMAL or NO_WIFI, keep wifi logo as long as we are in SETUP or CAPTIVE_PORTAL state
}

void loop()
{
    struct tm time = time_get();

    // BUTTON
    button.tick(); // always tick the button

    // CAPTIVE PORTAL
    if (state == STATE_CAPTIVE_PORTAL)
    {
        if (!captive_portal_active() && !wifi_check()) // if the captive portal is not active and Wi-Fi is not connected
        {
            Serial.println("Captive portal is not active, switching to one of the other states");
            update_state(wifi_check() ? STATE_NORMAL : STATE_NO_WIFI); // if the captive portal is not active, switch to one of the normal states
        }
        else
        {
            captive_portal_tick(); // handle captive portal events
        }
    }

    // SCENE
    if (state == STATE_NO_WIFI || state == STATE_NORMAL)
    {
        sceneSwitcher.tick(); // progress the current scene
    }

    // WIFI
    if (state != STATE_CAPTIVE_PORTAL)
    {
        if (wifi_check())
        {
            update_state(STATE_NORMAL); // if we are in captive portal state and Wi-Fi is connected, switch to normal state
        }
        else
        {
            update_state(STATE_NO_WIFI); // if we are in normal state and Wi-Fi is not connected, switch to no Wi-Fi state
        }
    }

    // WEATHER
    if (state == STATE_NORMAL && weatherSyncTimer.check())
    {
        weather_fetch();
    }

    // TIME
    if (state == STATE_NORMAL && timeSyncTimer.check())
    {
        time_syncNTP();
    }

    // BRIGHTNESS (TODO: ease in and out around the threshold)
    if (state == STATE_NORMAL ? weather_get().isDay : time_isNight(time))
    {
        gBright = gSettings.brightness_night;
    }
    else
    {
        gBright = gSettings.brightness_day;
    }

    // OFF HOURS
    if (shouldTurnOff(time))
    {
        nextSleepDuration = calcTurnOffDuration(time);
        update_state(STATE_SLEEPING); // switch to sleeping state if it is time to turn off
        update_state(STATE_NORMAL);   // switch back to normal state, as everything during the sleep state is blocking
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
    if (state == STATE_NORMAL)
    {
        Serial.println("Button - Single click -> next scene");
        sceneSwitcher.nextScene();
    }
    else if (state == STATE_CAPTIVE_PORTAL)
    {
        Serial.println("Button - Single click -> stop captive portal");
        if (wifi_check())
        {
            update_state(STATE_NORMAL); // if Wi-Fi is connected, switch to normal state
        }
        else
        {
            update_state(STATE_NO_WIFI); // switch to no Wi-Fi state, as we cancelled the captive portal
        }
    }
}

void buttonLongPressStart()
{
    buttonLongPressTimer = millis();
}

void buttonLongPressStop()
{
    int duration = millis() - buttonLongPressTimer;
    Serial.printf("Button long press duration: %d ms\n", duration);
    if (duration > 10000)
    {
        Serial.println("Button long press -> reset WiFi credentials");
        wifi_clear_credentials();
    }
}
