#include "system.hpp"
#include "desktop.hpp"
#include "hal.hpp"
#include "utils.hpp"
#include <stdint.h>

void sys::proc_mouse(hal::mouse::MState &mstate) {
  // mstate.isMoving = true;

  uint8_t data = 0;
  while (hal::mouse::get_packet().pop(data)) {

    if (mstate.packet_idx == 0)
      if (!(data & 0x08))
        continue;

    mstate.packet[mstate.packet_idx++] = data;

    int packet_size = mstate.has_wheel ? 4 : 3;

    if (mstate.packet_idx < packet_size)
      continue;

    mstate.packet_idx = 0;

    /* Validasi: bit 3 dari byte 0 harus selalu set */
    if (!(mstate.packet[0] & 0x08)) {
      mstate.packet_idx = 0;
      continue;
    }

    mstate.prev_x = mstate.x;
    mstate.prev_y = mstate.y;

    /* Decode packet */
    mstate.prev_left = mstate.left;
    mstate.prev_right = mstate.right;

    mstate.left = (mstate.packet[0] & 0x01) != 0;
    mstate.right = (mstate.packet[0] & 0x02) != 0;
    mstate.middle = (mstate.packet[0] & 0x04) != 0;

    int32_t dx = (int8_t)mstate.packet[1];
    int32_t dy = (int8_t)mstate.packet[2];

    /* Overflow check */
    if (mstate.packet[0] & 0x40)
      dx = 0;
    if (mstate.packet[0] & 0x80)
      dy = 0;

    mstate.x += dx;
    mstate.y -= dy;

    mstate.isMoving = (dx != 0 || dy != 0);

    /* Clamp ke layar */
    clamp_to_screen(mstate.x, mstate.y, 0, 0, Desktop::get_state().screen_w,
                    Desktop::get_state().screen_h);

    bool changed = (dx != 0 || dy != 0 || mstate.left != mstate.prev_left ||
                    mstate.right != mstate.prev_right);

    if (changed)
      mstate.event_pending = changed;
  }
}

void sys::time_update() {}
