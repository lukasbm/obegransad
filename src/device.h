#pragma once

#include <WiFi.h>
#include <esp_sleep.h>
#include <string.h>
#include <Arduino.h>

#define WAKEUP_PIN GPIO_NUM_9 // button pin!
#define MY_NTP_SERVER "pool.ntp.org"
// german time zone
#define MY_TZ "CET-1CEST,M3.5.0,M10.5.0/3"
// TODO: Add WiFi credentials
#define WIFI_SSID ""
#define WIFI_PASSWORD ""

void enter_light_sleep(uint64_t seconds);
void wifi_connect(void);
void wifi_off();

void time_setup();
void time_syncNTP();
static void time_fetch();
int time_hour();
int time_minute();
int time_second();
bool isNight();
const char *getTimeString();
struct tm time_full();
