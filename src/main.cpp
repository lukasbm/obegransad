#include <Arduino.h>
#include "weather.cpp"
#include "device.cpp"
#include "config.cpp"
#include "led.cpp"
#include "scene.cpp"

#define BUTTON_PIN GPIO_NUM_9 // button pin!

SceneSwitcher sceneSwitcher;

void setup()
{
  Serial.begin(74880); // native baud rate of ESP32

  panel_init();
  panel_debugTest();

  // TODO: set up clock!

  wifi_connect();

  time_setup();

  setup_config_server();

  sceneSwitcher.nextScene(); // Start with the first scene
}

void loop()
{

  // Check if the button is pressed
  if (digitalRead(BUTTON_PIN) == LOW)
  {
    Serial.println("Button pressed!");
    sceneSwitcher.nextScene();
  }

  // Update the current scene
  sceneSwitcher.tick();

  delay(50);
}
