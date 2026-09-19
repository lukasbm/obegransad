#include "popup.h"

#include "helper.hpp" // millis()

namespace {
Frame s_frame;
unsigned long s_shown_at_ms = 0;
uint32_t s_duration_ms = 0;
} // namespace

void popup_show(const Frame &frame, uint32_t duration_ms) {
  s_frame = frame;
  s_shown_at_ms = millis();
  s_duration_ms = duration_ms;
  s_frame.present(); // no need to wait for the next UI tick
}

bool popup_is_active() {
  return s_duration_ms != 0 && (millis() - s_shown_at_ms) < s_duration_ms;
}

void popup_render() {
  if (popup_is_active()) {
    s_frame.present();
  }
}

void popup_dismiss() { s_duration_ms = 0; }
