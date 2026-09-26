#pragma once

#include "ikea-obegransad-panel.h"
#include "scene.h"
#include <math.h>

// A solid cube tumbling in 3D. Each frame the eight vertices are rotated by two
// angles, projected to the panel with a weak perspective, and the faces that
// point towards the viewer are filled. Face brightness comes from a fixed light
// direction, so the tumbling reads as a shaded solid rather than a flat blob.
class RotatingCubeScene : public Scene {
private:
  float ax = 0.0f;
  float ay = 0.0f;
  float vx[8];
  float vy[8];
  float vz[8];
  int px[8];
  int py[8];

  static int edge(int ax, int ay, int bx, int by, int x, int y) {
    return (x - ax) * (by - ay) - (y - ay) * (bx - ax);
  }

  static int min3(int a, int b, int c) {
    const int m = a < b ? a : b;
    return m < c ? m : c;
  }

  static int max3(int a, int b, int c) {
    const int m = a > b ? a : b;
    return m > c ? m : c;
  }

  void rotate() {
    const float ca = cosf(ax), sa = sinf(ax);
    const float cb = cosf(ay), sb = sinf(ay);
    uint8_t i = 0;
    for (int z = -1; z <= 1; z += 2) {
      for (int y = -1; y <= 1; y += 2) {
        for (int x = -1; x <= 1; x += 2) {
          const float y1 = y * ca - z * sa;
          const float z1 = y * sa + z * ca;
          vx[i] = x * cb + z1 * sb;
          vy[i] = y1;
          vz[i] = -x * sb + z1 * cb;
          i++;
        }
      }
    }
  }

  void project() {
    for (uint8_t k = 0; k < 8; k++) {
      const float s = 4.6f * (1.0f + 0.16f * vz[k]);
      px[k] = (int)lroundf(7.5f + vx[k] * s);
      py[k] = (int)lroundf(7.5f + vy[k] * s);
    }
  }

  void fill_triangle(uint8_t a, uint8_t b, uint8_t c, uint8_t brightness) {
    int area = edge(px[a], py[a], px[b], py[b], px[c], py[c]);
    if (area == 0) {
      return;
    }
    if (area < 0) {
      const uint8_t t = b;
      b = c;
      c = t;
    }
    const int minx = min3(px[a], px[b], px[c]);
    const int maxx = max3(px[a], px[b], px[c]);
    const int miny = min3(py[a], py[b], py[c]);
    const int maxy = max3(py[a], py[b], py[c]);
    for (int y = miny; y <= maxy; y++) {
      for (int x = minx; x <= maxx; x++) {
        if (edge(px[b], py[b], px[c], py[c], x, y) >= 0 &&
            edge(px[c], py[c], px[a], py[a], x, y) >= 0 &&
            edge(px[a], py[a], px[b], py[b], x, y) >= 0) {
          panel_setPixel(y, x, brightness);
        }
      }
    }
  }

  void fill_face(const uint8_t f[4], uint8_t brightness) {
    fill_triangle(f[0], f[1], f[2], brightness);
    fill_triangle(f[0], f[2], f[3], brightness);
  }

public:
  const char *get_scene_name() const override { return "Rotating Cube"; }
  uint16_t target_fps() const override { return 20; }

protected:
  void render(uint32_t /*dt_ms*/) override {
    ax += 0.045f;
    ay += 0.075f;
    if (ax > 6.2831853f) {
      ax -= 6.2831853f;
    }
    if (ay > 6.2831853f) {
      ay -= 6.2831853f;
    }

    rotate();
    project();
    panel_clear();

    static constexpr uint8_t faces[6][4] = {{4, 5, 6, 7}, {1, 0, 3, 2},
                                            {0, 4, 7, 3}, {5, 1, 2, 6},
                                            {0, 1, 5, 4}, {3, 7, 6, 2}};
    static constexpr float light_x = 0.402f;
    static constexpr float light_y = 0.503f;
    static constexpr float light_z = -0.765f;

    for (const auto &f : faces) {
      const float e1x = vx[f[1]] - vx[f[0]];
      const float e1y = vy[f[1]] - vy[f[0]];
      const float e1z = vz[f[1]] - vz[f[0]];
      const float e2x = vx[f[2]] - vx[f[0]];
      const float e2y = vy[f[2]] - vy[f[0]];
      const float e2z = vz[f[2]] - vz[f[0]];
      float nx = e1y * e2z - e1z * e2y;
      float ny = e1z * e2x - e1x * e2z;
      float nz = e1x * e2y - e1y * e2x;
      const float len = sqrtf(nx * nx + ny * ny + nz * nz);
      if (len == 0.0f) {
        continue;
      }
      nx /= len;
      ny /= len;
      nz /= len;
      if (nz <= 0.05f) {
        continue; // back-facing
      }
      const float d = -(nx * light_x + ny * light_y + nz * light_z);
      uint8_t brightness;
      if (d > 0.60f) {
        brightness = PANEL_BRIGHTNESS_3;
      } else if (d > 0.30f) {
        brightness = PANEL_BRIGHTNESS_2;
      } else {
        brightness = PANEL_BRIGHTNESS_1;
      }
      fill_face(f, brightness);
    }
    panel_commit();
  }
};
