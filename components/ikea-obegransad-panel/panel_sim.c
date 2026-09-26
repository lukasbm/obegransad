// Simulator backend: no SPI/RMT/GPIO. Frames are published to the host
// renderer through the obegransad-sim link (see tools/sim/renderer.py).

#include "sdkconfig.h"

#if !CONFIG_OBG_PANEL_BACKEND_HW

#include "panel_internal.h"

#include "esp_log.h"
#include "obegransad-sim.h"

static const char *TAG = "panel_sim";

// Only publish when the framebuffer actually changed; the link resends the
// last frame by itself when a renderer (re)connects.
static bool s_dirty = true;

esp_err_t panel_backend_init(const panel_config_t *config) {
  (void)config; // pins/host/speed are meaningless without hardware; gamma is
                // applied in panel_core.c
  ESP_LOGI(TAG, "Simulator panel backend: frames go to the host renderer");
  s_dirty = true;
  return sim_link_init();
}

void panel_backend_rebuild(void) { s_dirty = true; }

void panel_backend_display(void) {
  if (!s_dirty) {
    return;
  }
  s_dirty = false;
  sim_link_publish_frame(panel_core_framebuffer());
}

#endif // !CONFIG_OBG_PANEL_BACKEND_HW
