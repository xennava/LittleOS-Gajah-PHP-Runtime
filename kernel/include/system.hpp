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
extern void proc_mouse(hal::mouse::MState &mstate);
extern void time_update();
}; // namespace sys
