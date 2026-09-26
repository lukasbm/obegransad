#pragma once

#include "esp_err.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Transport between the firmware and the host-side simulator renderer
// (tools/sim/renderer.py). One TCP connection, opened by the firmware (the
// renderer listens); 16x16 frames travel to the host, single-character button
// commands come back.

// Framebuffer size in bytes (16x16, one byte per pixel).
#define SIM_LINK_FRAME_BYTES 256

// Start the background link task. Safe to call more than once.
esp_err_t sim_link_init(void);

// Hand the current frame to the link task. Never blocks; if a frame is still
// pending it is replaced. Frames are dropped silently while no renderer is
// connected.
void sim_link_publish_frame(const uint8_t *frame);

// Next command byte from the renderer ('S' short, 'D' double, 'L' long press),
// or 0 when nothing is queued.
int sim_link_poll_command(void);

#ifdef __cplusplus
}
#endif
