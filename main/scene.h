#pragma once

#include "esp_log.h"
#include "helper.hpp" // millis()
#include <cstdint>

// Base class for all scenes.
//
// Subclasses implement render(dt_ms) and optionally on_activate()/on_deactivate().
// The base paces rendering to target_fps() so scenes no longer reimplement a
// millis() throttle. Scenes draw with the panel_* API and sprites.
//
// Authoring a scene:
//   class MyScene : public Scene {
//   public:
//     const char *get_scene_name() const override { return "My Scene"; }
//     uint16_t target_fps() const override { return 10; } // optional, default 10
//     bool requires_wifi() const override { return false; } // optional
//   protected:
//     void on_activate() override { /* one-time setup */ }
//     void render(uint32_t dt_ms) override { /* draw one frame */ }
//   };
// Then add it to the table in scenes/scene_registry.hpp.
class Scene {
public:
  virtual ~Scene() = default;

  // Human-readable name (also used in logs).
  virtual const char *get_scene_name() const = 0;

  // If true, the scene switcher skips this scene while Wi-Fi is unavailable.
  virtual bool requires_wifi() const { return false; }

  // Desired render rate. Return 0 for a static/one-shot scene: render() then
  // runs once right after activation and not again. Default 10 FPS.
  virtual uint16_t target_fps() const { return 10; }

  // --- Called by the scene switcher; not meant to be overridden. ---
  void activate() {
    has_rendered = false; // force an immediate first render
    log_scene_event("activated");
    on_activate();
    update(); // draw the first frame right away
  }

  void deactivate() {
    on_deactivate();
    log_scene_event("deactivated");
  }

  void update() {
    const unsigned long now = millis();
    const uint16_t fps = target_fps();
    if (has_rendered) {
      if (fps == 0) {
        return; // one-shot scene, already drawn
      }
      const unsigned long interval_ms = 1000u / fps;
      if (now - last_render_ms < interval_ms) {
        return; // throttle to target FPS
      }
    }
    const uint32_t dt = has_rendered ? (uint32_t)(now - last_render_ms) : 0;
    last_render_ms = now;
    has_rendered = true;
    render(dt);
  }

protected:
  // Draw one frame. dt_ms is elapsed since the previous render (0 on first).
  virtual void render(uint32_t dt_ms) = 0;
  // Optional one-time hooks around (de)activation.
  virtual void on_activate() {}
  virtual void on_deactivate() {}

  void log_scene_event(const char *event) const {
    ESP_LOGI("scene", "%s: %s", get_scene_name(), event);
  }

private:
  unsigned long last_render_ms = 0;
  bool has_rendered = false;
};
