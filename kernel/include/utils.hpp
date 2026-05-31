#pragma once

#include <stdint.h>

extern void numstream(int64_t u, int x = -1, int y = -1);
extern "C" void print(const char *fmr, ...);

inline void clamp_to_screen(int32_t &x, int32_t &y, int32_t min_x,
                            int32_t min_y, int32_t max_x, int32_t max_y) {

  if (x < min_x)
    x = min_x;
  else if (x > max_x)
    x = max_x;

  if (y < min_y)
    y = min_y;
  else if (y > max_y)
    y = max_y;
}

namespace Color {
inline uint32_t RGBA(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
  return r | (g << 8) | (b << 16) | (a << 24);
}

} // namespace Color
