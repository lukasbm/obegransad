#pragma once

#include "ikea-obegransad-panel.h"

// Interface between the panel core (framebuffer, brightness, refresh timing)
// and the two backends:
//   panel_hw.c  - SPI + RMT + GPIO shift-register output (real hardware)
//   panel_sim.c - publishes frames to the host simulator (QEMU/host builds)
//
// The core never includes backend headers; backends never touch the
// framebuffer directly, they read it through panel_core_framebuffer().

// Number of BCM bit planes (shared by core and hardware backend).
#define PANEL_BIT_DEPTH 4

// Row-major framebuffer (PANEL_WIDTH * PANEL_HEIGHT bytes) with the current
// logical brightness values after Brightness mapping. Owned by the core.
const uint8_t *panel_core_framebuffer(void);

// Gamma-corrected global brightness (0-255); the hardware backend scales
// bitplane on-times with it.
uint8_t panel_core_brightness(void);

// Called once from panel_init(); configures whatever the backend needs.
esp_err_t panel_backend_init(const panel_config_t *config);

// The framebuffer changed: the hardware backend rebuilds its bit planes here,
// the simulator backend has nothing to do.
void panel_backend_rebuild(void);

// One 500 Hz refresh tick: shift out the next bit plane (hardware) or hand the
// current frame to the simulator link.
void panel_backend_display(void);
