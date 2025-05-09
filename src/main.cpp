#include <Arduino.h>
#include <OneButton.h>

#include "weather.h"
#include "device.h"
#include "config.h"
#include "led.h"
#include "scene.cpp"

// FIXME: i think this is the problem
// Scene Is never initialized
SceneSwitcher sceneSwitcher;

OneButton button;
void buttonSetup();
void buttonSingleClick();
// TODO: maybe add double click to enter sleep?
void buttonLongPressStart();
void buttonLongPressStop();
int buttonLongPressTimer = 0;

void setup()
{
  Serial.begin(115200);

  // buttonSetup();

  panel_init();
  for (uint8_t y = 0; y < 16; y++)
    for (uint8_t x = 0; x < 16; x++)
      panel_setPixel(y, x, y * 16 + x);

  // setup_device();

  // wifi_connect();

  // time_setup();

  // setup_config_server();

  Serial.println("Setup done!");

  // Start with the first scene
  // sceneSwitcher.nextScene();
}

void loop()
{
  Serial.println("Looping...");

  // Update the button
  // button.tick();

  // Update the current scene
  // sceneSwitcher.tick();

  // refresh the display
  panel_show();
}

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
