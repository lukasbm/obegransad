#include "error.h"
#include "led.h"
#include "sprites/wifi.hpp"

void display_wifi_logo(void)
{
    // display the wifi logo while connecting
    panel_clear();
    panel_drawSprite(3, 5, wifi_sprite.data, wifi_sprite.width, wifi_sprite.height);
    // panel_hold();
}

void display_device_error(DeviceError err)
{
    // panel_clear();
    // TODO: draw error icons!!!
    // panel_hold();
}
