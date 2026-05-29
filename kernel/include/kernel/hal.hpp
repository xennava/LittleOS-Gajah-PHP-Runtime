/*
 * LittleOS Gajah PHP - hal.hpp
 * Hardware Abstraction Layer (C++ untuk komunikasi hardware)
 * Semua akses hardware dilakukan melalui C++ — bukan C
 *
 * Versi Desktop: mendukung GUI, mouse, window manager
 * Tema warna: Tailwind CSS color palette
 */

#pragma once

#include "limine.h"
#include <stddef.h>
#include <stdint.h>

namespace hal {

/* ============================================================
 * PORT I/O — akses langsung ke port hardware x86
 * ============================================================ */
namespace ports {

static inline __attribute__((always_inline)) uint8_t inb(uint16_t port) {
  uint8_t r;
  asm volatile("inb %1, %0" : "=a"(r) : "Nd"(port));
  return r;
}

static inline void outb(uint16_t port, uint8_t val) {
  asm volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint16_t inw(uint16_t port) {
  uint16_t r;
  asm volatile("inw %1, %0" : "=a"(r) : "Nd"(port));
  return r;
}

static inline void outw(uint16_t port, uint16_t val) {
  asm volatile("outw %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint32_t ind(uint16_t port) {
  uint32_t r;
  asm volatile("inl %1, %0" : "=a"(r) : "Nd"(port));
  return r;
}

static inline void outd(uint16_t port, uint32_t val) {
  asm volatile("outl %0, %1" : : "a"(val), "Nd"(port));
}

static inline void io_wait() { asm volatile("outb %%al, $0x80" : : "a"(0)); }

} // namespace ports

/* ============================================================
 * STRING — fungsi manipulasi string
 * ============================================================ */
namespace string {

size_t strlen(const char *s);
char *strcpy(char *dst, const char *src);
char *strncpy(char *dst, const char *src, size_t n);
int strcmp(const char *a, const char *b);
int strncmp(const char *a, const char *b, size_t n);
char *strcat(char *dst, const char *src);
char *strchr(const char *s, int c);
char *strstr(const char *hay, const char *needle);
void itoa(int64_t num, char *buf);
void utoa(uint64_t num, char *buf);
void htoa(uint64_t num, char *buf);
int64_t atoi(const char *s);
char to_upper(char c);
char to_lower(char c);
bool is_digit(char c);
bool is_alpha(char c);
bool is_space(char c);
bool is_alnum(char c);

} // namespace string

/* ============================================================
 * MEMORY — manajemen memori fisik dan heap
 * ============================================================ */
namespace memory {

void init(struct limine_memmap_response *memmap, uint64_t hhdm_offset);
void *alloc_page();
void free_page(void *addr);
uint64_t get_total();
uint64_t get_free();
uint64_t get_used();

/* Heap dinamis */
void heap_init();
void *kmalloc(size_t size);
void kfree(void *ptr);
void *kcalloc(size_t count, size_t size);
void *krealloc(void *ptr, size_t new_size);

} // namespace memory

/* ============================================================
 * INTERRUPTS — IDT dan penanganan interupsi
 * ============================================================ */
namespace interrupts {

void init();
void enable();
void disable();

} // namespace interrupts

/* ============================================================
 * TIMER — PIT timer untuk pencatat waktu
 * ============================================================ */
namespace timer {

inline uint64_t rdtscp();
void init();
uint64_t get_ticks();
uint64_t get_ms();
uint64_t get_seconds();
uint64_t time_ns();
void wait_ms(uint64_t ms);
void calibrate_tsc();

} // namespace timer

/* ============================================================
 * KEYBOARD — driver keyboard PS/2
 * ============================================================ */
namespace keyboard {

enum KeyAction : uint8_t { KEY_DOWN, KEY_UP };

struct KeyEvent {
  uint16_t key;
  KeyAction action;
  bool repeated;
  uint8_t modifiers;
};

extern bool key_down[];

void init();
char read_char();
char read_char_nonblocking();
bool has_input();
void read_line(char *buf, size_t max_len);
uint8_t read_scancode_nonblocking();
bool is_shift_held();
bool is_ctrl_held();

} // namespace keyboard

/* ============================================================
 * MOUSE — driver mouse PS/2
 * ============================================================ */
namespace mouse {
constexpr uint16_t DEFAULT_PACKET_SIZE_BUF = 512;

template <uint32_t SIZE = DEFAULT_PACKET_SIZE_BUF> class PACKET {
private:
  uint8_t packet_data[SIZE];
  uint32_t head = 0, tail = 0;
  uint32_t count = 0;

public:
  bool push(uint8_t v) {
    if (count >= SIZE)
      return false;

    packet_data[head] = v;
    head = (head + 1) % SIZE;
    count++;
    return true;
  }

  bool pop(uint8_t &out) {
    if (count == 0)
      return false;
    count--;
    out = packet_data[tail];
    tail = (tail + 1) % SIZE;

    return true;
  }

  bool empty() const { return count == 0; }

  constexpr uint32_t size() const { return SIZE; }
};

extern PACKET<> &get_packet();

/* State mouse */
struct MState {
  int32_t x, y;
  int32_t prev_x, prev_y;
  int32_t screen_w, screen_h;
  /* Packet buffer */
  int packet_idx;
  uint8_t packet[4];
  bool prev_left, prev_right;
  bool left, right, middle;
  bool has_wheel;
  /* Event queue */
  bool event_pending;
  bool isMoving;
};

void init();
int32_t get_x();
int32_t get_y();
bool is_left_pressed();
bool is_right_pressed();
bool has_event();
MState &getState();

struct MouseEvent {
  int32_t x, y;
  int32_t dx, dy;
  bool left, right, middle;
  bool clicked;
  bool released;
  bool right_clicked;
};

MouseEvent poll_event();

} // namespace mouse

/* ============================================================
 * RTC — Real-Time Clock
 * ============================================================ */
namespace rtc {

void init();

struct DateTime {
  uint8_t second;
  uint8_t minute;
  uint8_t hour;
  uint8_t day;
  uint8_t month;
  uint16_t year;
  uint8_t weekday;
};

DateTime get_time();

} // namespace rtc

/* ============================================================
 * EMBEDDED ASSETS — gambar yang di-embed ke kernel binary
 * ============================================================ */
namespace assets {

/* Mascot 96x96 ARGB */
static const int MASCOT_W = 96;
static const int MASCOT_H = 96;

/* Wallpaper 640x400 ARGB */
static const int WALLPAPER_W = 640;
static const int WALLPAPER_H = 400;

/* Symbols dari objcopy (extern "C" karena dari binary blob) */

extern "C" const uint8_t _binary_kernel_assets_mascot_bin_start[];
extern "C" const uint8_t _binary_kernel_assets_mascot_bin_end[];
extern "C" const uint8_t _binary_kernel_assets_wallpaper_bin_start[];
extern "C" const uint8_t _binary_kernel_assets_wallpaper_bin_end[];
extern "C" const uint8_t _binary_kernel_assets_menu_icon_bin_start[];
extern "C" const uint8_t _binary_kernel_assets_menu_icon_bin_end[];

inline const uint32_t *mascot_pixels() {
  return (const uint32_t *)_binary_kernel_assets_mascot_bin_start;
}
inline const uint32_t *wallpaper_pixels() {
  return (const uint32_t *)_binary_kernel_assets_wallpaper_bin_start;
}

static const int MENU_ICON_W = 32;
static const int MENU_ICON_H = 32;
inline const uint32_t *menu_icon_pixels() {
  return (const uint32_t *)_binary_kernel_assets_menu_icon_bin_start;
}

} // namespace assets

} // namespace hal
