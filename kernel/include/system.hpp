#pragma once

/* ============================================================
 * INFORMASI SISTEM
 * ============================================================ */
#include "hal.hpp"
#define LITTLEOS_NAME "LittleOS Gajah PHP"
#define LITTLEOS_VERSION "1.0.0"
#define LITTLEOS_CODENAME "Gajah"
#define LITTLEOS_ARCH "x86_64"
#define LITTLEOS_KERNEL "PHP 8"
#define LITTLEOS_DE "KDE Plasma"
#define LITTLEOS_DISPLAY "Wayland"
#define LITTLEOS_BOOTLOADER "Limine"
#define LITTLEOS_PARTITION "MBR + GPT"
#define LITTLEOS_LANG "PHP"
#define LITTLEOS_INPUT "USB (xHCI) + PS/2"
#define LITTLEOS_NET "Auto"
#define LITTLEOS_UIUX "Tailwind CSS"

namespace sys {

struct TimeState {
  uint64_t ns = 0;
  uint64_t ms = 0;
  uint64_t sec = 0;
};

struct TimeKeeper {
  TimeState current;
  TimeState last;
  TimeState delta;

  void update(uint64_t raw_ns) {
    last = current;

    // Update current
    current.ns = raw_ns;
    current.ms = ((__uint128_t)current.ns / 1'000'000ULL);
    current.sec = ((__uint128_t)current.ms / 1'000ULL);

    // Update delta
    delta.ns = current.ns - last.ns;
    delta.ms = current.ms - last.ms;
    delta.sec = current.sec - last.sec;
  }
};

extern TimeKeeper time;

extern void proc_mouse(hal::mouse::MState &mstate);
extern void time_update();
}; // namespace sys
