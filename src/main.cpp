#include <Arduino.h>
#include <OneButton.h>

#include "weather.h"
#include "device.h"
#include "config.h"
#include "led.h"
#include "scene.cpp"
#include "sprites/wifi.hpp"
#include "clock.h"

SceneSwitcher sceneSwitcher;

OneButton button;
void buttonSetup();
void buttonSingleClick();
void buttonLongPressStart();
void buttonLongPressStop();
int buttonLongPressTimer = 0;

static void conduct_checks();

void setup()
{
  Serial.begin(115200);

  buttonSetup();

  panel_init();

  // display the wifi logo while connecting
  panel_clear();
  panel_drawSprite(3, 5, sprite_wifi.data, sprite_wifi.width, sprite_wifi.height);
  panel_show();
  panel_hold();

  // mostly wifi setup in here
  setup_device();

  // NTP sync
  time_setup();

  // accept new configs
  setup_config_server();

  Serial.println("Setup done!");

  conduct_checks();

  // Start with the first scene
  sceneSwitcher.nextScene();
}

static void conduct_checks()
{
  Serial.println("Conducting checks...");
  struct tm time = time_fetch();
  // adjust brightness
  gBright = isNight(time) ? settings.brightness_night : settings.brightness_day;

  // also check if it is time to shut off!
  // if (shouldTurnOff(time))
  // {
  //   enter_light_sleep() // TODO: make it also return the sleep duration
  // }
}

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

  // refresh the display
  panel_show();
}

///// button stuff

void buttonSetup()
{
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
