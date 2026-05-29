#include "console.hpp"
#define KERNEL_BOOT_HPP
#ifdef KERNEL_BOOT_HPP

#include "hal.hpp"
#include "limine.h"
#include "system.hpp"
#include "themes.hpp"
#include "utils.hpp"
#include <stdint.h>

/* ============================================================
 * LIMINE BOOT REQUESTS
 * Permintaan ke bootloader Limine untuk framebuffer, memory map, dll
 * ============================================================ */

/* Limine base revision */
__attribute__((used, section(".limine_requests"))) static volatile uint64_t
    limine_base_revision[] = LIMINE_BASE_REVISION(3);

/* Marker awal requests */
__attribute__((used,
               section(".limine_requests_start"))) static volatile uint64_t
    limine_requests_start[] = LIMINE_REQUESTS_START_MARKER;

/* Framebuffer request */
__attribute__((
    used,
    section(
        ".limine_requests"))) static volatile struct limine_framebuffer_request
    framebuffer_request = {
        {LIMINE_COMMON_MAGIC, 0x9d5827dcd881dd75, 0xa3148604f6fab11b}, 0, 0};

/* Memory map request */
__attribute__((
    used,
    section(".limine_requests"))) static volatile struct limine_memmap_request
    memmap_request = {
        {LIMINE_COMMON_MAGIC, 0x67cf3d9d378a806f, 0xe304acdfc50c3c62}, 0, 0};

/* HHDM (Higher Half Direct Map) request */
__attribute__((
    used,
    section(".limine_requests"))) static volatile struct limine_hhdm_request
    hhdm_request = {
        {LIMINE_COMMON_MAGIC, 0x48dcf1cb8ad2b852, 0x63984e959a98244b}, 0, 0};

/* Marker akhir requests */
__attribute__((used, section(".limine_requests_end"))) static volatile uint64_t
    limine_requests_end[] = LIMINE_REQUESTS_END_MARKER;

inline void BOOT_Init(struct limine_framebuffer *fb) {
  /* ---- TAHAP 1: Inisialisasi Console (C++ HAL) ---- */
  framebuffer_t fb2;
  fb2.addr = fb->address;
  fb2.width = fb->width;
  fb2.height = fb->height;
  fb2.pitch = fb->pitch;
  Console::initc(fb2);
  Console::clear();

  Console::puts_colored("[ HAL ] ", COLOR_ACCENT);
  Console::puts("Framebuffer console aktif (");
  numstream(fb2.width);
  Console::puts("x");
  numstream(fb2.height);
  Console::puts(")\n");

  /* ---- TAHAP 2: Inisialisasi Memory (C++ HAL) ---- */
  if (memmap_request.response && hhdm_request.response) {
    hal::memory::init(memmap_request.response, hhdm_request.response->offset);
    Console::puts_colored("[ HAL ] ", COLOR_ACCENT);
    Console::puts("Physical memory manager aktif (");
    numstream(hal::memory::get_total() / MEGABYTE);
    Console::puts(" MB total)\n");
  }

  hal::memory::heap_init();
  Console::puts_colored("[ HAL ] ", COLOR_ACCENT);
  Console::puts("Heap allocator aktif\n");

  /* ---- TAHAP 3: Inisialisasi Interrupts (C++ HAL) ---- */
  hal::interrupts::init();
  Console::puts_colored("[ HAL ] ", COLOR_ACCENT);
  Console::puts("IDT dan PIC aktif\n");

  /* ---- TAHAP 4: Inisialisasi Timer (C++ HAL) ---- */
  hal::timer::init();
  Console::puts_colored("[ HAL ] ", COLOR_ACCENT);
  Console::puts("PIT Timer aktif (1000 Hz)\n");

  /* ---- TAHAP 5: Inisialisasi Keyboard (C++ HAL) ---- */
  hal::keyboard::init();
  Console::puts_colored("[ HAL ] ", COLOR_ACCENT);
  Console::puts("PS/2 Keyboard aktif\n");

  /* ---- TAHAP 6: Inisialisasi Mouse (C++ HAL) ---- */
  hal::mouse::init();
  Console::puts_colored("[ HAL ] ", COLOR_ACCENT);
  Console::puts("PS/2 Mouse aktif\n");

  /* ---- TAHAP 7: Inisialisasi RTC (C++ HAL) ---- */
  hal::rtc::init();
  Console::puts_colored("[ HAL ] ", COLOR_ACCENT);
  Console::puts("CMOS RTC aktif\n");

  /* ---- Aktifkan hardware interrupts ---- */
  hal::interrupts::enable();
  Console::puts_colored("[ HAL ] ", COLOR_ACCENT);
  Console::puts("Interrupts enabled\n");
  Console::puts("\n");
  Console::puts_colored("[ HAL ] ", COLOR_SUCCESS);
  Console::puts("Hardware Abstraction Layer (C++) siap.\n");
  Console::puts_colored("[ PHP ] ", COLOR_SUCCESS);
  Console::puts("Memulai PHP 8 Desktop Runtime...\n");
  Console::puts("\n");

  hal::timer::calibrate_tsc();

  /* Tunggu sebentar agar user bisa lihat boot log */
  hal::timer::wait_ms(550);
}

inline void BOOT_ANIM(struct limine_framebuffer *fb) {

  int32_t sw = (int32_t)fb->width;
  int32_t sh = (int32_t)fb->height;

  Console::begin_frame();

  /* Background gelap gradient-like */
  Console::fill_rect(0, 0, sw, sh, TW_SLATE_950);

  /* Blit mascot di tengah-atas */
  int mascot_x = (sw - hal::assets::MASCOT_W) / 2;
  int mascot_y = sh * 13 / 40;
  Console::blit_raw(mascot_x, mascot_y, hal::assets::MASCOT_W,
                    hal::assets::MASCOT_H, hal::assets::mascot_pixels());

  /* Nama OS di bawah mascot */
  const char *os_title = "LittleOS Gajah PHP";
  int title_w = Console::string_pixel_width(os_title);
  Console::draw_string((sw - title_w) / 2, sh * 101 / 200, os_title,
                       TW_SKY_400);

  /* Subtitle */
  const char *subtitle = "PHP 8 Runtime | x86_64 Bare Metal";
  int sub_w = Console::string_pixel_width(subtitle);
  Console::draw_string((sw - sub_w) / 2, sh * 21 / 40, subtitle, TW_SLATE_400);

  /* System info */
  const char *info1 = "Kernel: " LITTLEOS_KERNEL "  |  Arch: " LITTLEOS_ARCH
                      "  |  DE: " LITTLEOS_DE;
  int info1_w = Console::string_pixel_width(info1);
  Console::draw_string((sw - info1_w) / 2, sh * 9 / 16, info1, TW_SLATE_500);

  const char *info2 =
      "Display: " LITTLEOS_DISPLAY "  |  Bootloader: " LITTLEOS_BOOTLOADER
      "  |  UI/UX: " LITTLEOS_UIUX;
  int info2_w = Console::string_pixel_width(info2);
  Console::draw_string((sw - info2_w) / 2, sh * 29 / 50, info2, TW_SLATE_500);

  /* Loading bar background */
  const int bar_w = sw / 4; // 1280 / 4 = 320
  const int bar_h = sh / 80;
  const int bar_x = (sw - bar_w) / 2;
  const int bar_y = sh * 2 / 3;
  Console::fill_rounded_rect(bar_x, bar_y, bar_w, bar_h, 4, TW_SLATE_800);

  /* Loading text */

  /* Animate loading bar */
  {
    const char load_text[] = "Memuat Desktop...";
    const int lt_w = sizeof(load_text) - 1;
    const int lt_x = (sw - lt_w * Console::CELL_W) / 2;
    const int lt_y = bar_y + bar_h * 9 / 5;
    int lt_inc = 1;
    uint8_t col_idx = 0, lt_idx = 0;

    Console::draw_string(lt_x, lt_y, load_text, TW_SLATE_500);

    const int prog_h = bar_h / 2;
    const int prog_y = bar_y + (bar_h - prog_h) / 2;
    const int max_prog_width = bar_w * 39 / 40;
    const int prog_x = bar_x + (bar_w - max_prog_width) / 2;

    int targetProgression = bar_w / 120;
    for (int progress = 0; progress <= max_prog_width;
         progress += targetProgression) {
      if (progress > 0) {
        Console::fill_rounded_rect(prog_x, prog_y, progress, prog_h, 1,
                                   TW_SKY_500);
        if (progress == (lt_inc)*targetProgression) {
          int a = 0;
          int b = 1;
          int c = 2;
          uint32_t col = 0xff * (col_idx == a) | (0xff * (col_idx == b)) << 8 |
                         (0xff * (col_idx == c)) << 16 | 0xff << 24;
          Console::draw_char_transparent(lt_x + lt_idx * Console::CELL_W, lt_y,
                                         load_text[lt_idx], col);
          if (col_idx == 3) {
            c ^= a;
            a ^= c;
            c ^= a;
            b ^= c;
            c ^= b;
            b ^= c;
          }

          col_idx = (col_idx + 1) * (col_idx != 3);

          lt_idx = (lt_idx + 1) * (lt_idx != lt_w);
          lt_inc++;
        }
      }
      Console::swap_buffers();
      hal::timer::wait_ms(2);
    }
  }

  Console::end_frame();

  /* Hold splash briefly */
  hal::timer::wait_ms(5);
}
#endif
