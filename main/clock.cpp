#include "time.h"
#include <sys/select.h>
#include <sys/time.h>
#include <time.h>

/*
relevant functions:
- gettimeofday
- time
- asctime
- clock
- ctime
- difftime
- gmtime
- localtime
- mktime
- strftime
- adjtime
*/

void clock_init() {
  setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);
  tzset();
}

void sntp_sync() {}

// for one second resolution!
time_t get_current_time() {
  time_t now;
  char strftime_buf[64];
  struct tm timeinfo;

  time(&now);

  localtime_r(&now, &timeinfo);
  strftime(strftime_buf, sizeof(strftime_buf), "%Y-%m-%d %H:%M:%S", &timeinfo);

  return now;
}

// for one microsecond resolution
time_t get_current_time_us() {
  struct timeval tv_now;
  gettimeofday(&tv_now, NULL);
  int64_t time_us = (int64_t)tv_now.tv_sec * 1000000L + (int64_t)tv_now.tv_usec;
  return time_us;
}

