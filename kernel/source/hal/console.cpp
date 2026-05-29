/*
 * LittleOS Gajah PHP - console.cpp
 * Framebuffer console + grafis primitif + double buffering
 * Tema Tailwind CSS — ditulis dalam C++
 */

#include "console.hpp"
#include "fonts.hpp"
#include "hal.hpp"
#include "kernel.hpp"
#include "themes.hpp"
#include "utils.hpp"
#include <stdarg.h>
#include <stdint.h>
#include <string.h>

namespace Console {

/* ============================================================
 * STATE CONSOLE
 * ============================================================ */

ConsoleState state;

/* ============================================================
 * IMPLEMENTASI GRAFIS DASAR
 * ============================================================ */
void draw_pixel(int32_t x, int32_t y, uint32_t color) {
  if (x < 0 || y < 0 || (uint64_t)x >= state.fb_width ||
      (uint64_t)y >= state.fb_height)
    return;
  uint32_t *target =
      state.render_target ? state.render_target : state.framebuffer;
  uint32_t *pixel = (uint32_t *)((uint8_t *)target + y * state.fb_pitch) + x;
  *pixel = color;
}

void fill_rect(int32_t x, int32_t y, int32_t w, int32_t h, uint32_t color) {
  uint32_t *target =
      state.render_target ? state.render_target : state.framebuffer;
  for (int32_t row = y; row < y + h; row++) {
    if (row < 0 || (uint64_t)row >= state.fb_height)
      continue;
    for (int32_t col = x; col < x + w; col++) {
      if (col < 0 || (uint64_t)col >= state.fb_width)
        continue;
      uint32_t *pixel =
          (uint32_t *)((uint8_t *)target + row * state.fb_pitch + col * 4);
      *pixel = color;
    }
  }
}

void draw_char(int32_t x, int32_t y, char c, uint32_t fg, uint32_t bg) {
  int idx = (int)c - 32;
  if (idx < 0 || idx >= 95)
    idx = 0; /* karakter tidak dikenal -> spasi */
  const uint8_t *glyph = font8x8[idx];

  for (int row = 0; row < FONT_H; row++) {
    uint8_t bits = glyph[row];
    for (int col = 0; col < FONT_W; col++) {
      uint32_t color = (bits & (1 << col)) ? fg : bg;
      draw_pixel(x + col, y + row, color);
    }
  }
}

/* ============================================================
 * IMPLEMENTASI CONSOLE
 * ============================================================ */
void init(void *fb_addr, uint64_t width, uint64_t height, uint64_t pitch) {
  state.framebuffer = (uint32_t *)fb_addr;
  state.fb_width = width;
  state.fb_height = height;
  state.fb_pitch = pitch;
  state.fg_color = COLOR_FG;
  state.bg_color = COLOR_BG;
  state.cursor_col = 0;
  state.cursor_row = 0;
  state.max_cols = (uint32_t)(width / CELL_W);
  state.max_rows = (uint32_t)(height / CELL_H);
  state.fb_size_bytes = height * pitch;
  state.backbuffer = nullptr;
  state.render_target = nullptr; /* Default: render langsung ke framebuffer */
}

void initc(framebuffer_t &fb) {
  uint64_t width = fb.width, height = fb.height, pitch = fb.pitch;
  state.framebuffer = (unsigned int *)fb.addr;
  state.fb_width = width;
  state.fb_height = height;
  state.fb_pitch = pitch;
  state.fg_color = COLOR_FG;
  state.bg_color = COLOR_BG;
  state.cursor_col = 0;
  state.cursor_row = 0;
  state.max_cols = (uint32_t)(width / CELL_W);
  state.max_rows = (uint32_t)(height / CELL_H);
  state.fb_size_bytes = height * pitch;
  state.backbuffer = nullptr;
  state.render_target = nullptr; /* Default: render langsung ke framebuffer */
}

void clear() {
  fill_rect(0, 0, (int32_t)state.fb_width, (int32_t)state.fb_height,
            state.bg_color);
  state.cursor_col = 0;
  state.cursor_row = 0;
}

void scroll_up() {
  /* Salin semua baris ke atas 1 baris */
  uint64_t line_bytes = CELL_H * state.fb_pitch;
  uint8_t *fb = (uint8_t *)state.framebuffer;

  for (uint32_t row = 1; row < state.max_rows; row++) {
    uint8_t *dst = fb + (row - 1) * line_bytes;
    uint8_t *src = fb + row * line_bytes;
    memcpy(dst, src, line_bytes);
  }

  /* Bersihkan baris paling bawah */
  uint8_t *last_line = fb + (state.max_rows - 1) * line_bytes;
  for (uint64_t i = 0; i < line_bytes / 4; i++) {
    ((uint32_t *)last_line)[i] = state.bg_color;
  }
}

void newline() {
  state.cursor_col = 0;
  state.cursor_row++;
  if (state.cursor_row >= state.max_rows) {
    scroll_up();
    state.cursor_row = state.max_rows - 1;
  }
}

void putchar(char c) {
  if (c == '\n') {
    newline();
    return;
  }
  if (c == '\r') {
    state.cursor_col = 0;
    return;
  }
  if (c == '\t') {
    uint32_t next_tab = (state.cursor_col + 4) & ~3u;
    while (state.cursor_col < next_tab && state.cursor_col < state.max_cols) {
      draw_char(state.cursor_col * CELL_W, state.cursor_row * CELL_H, ' ',
                state.fg_color, state.bg_color);
      state.cursor_col++;
    }
    if (state.cursor_col >= state.max_cols)
      newline();
    return;
  }
  if (c == '\b') {
    if (state.cursor_col > 0) {
      state.cursor_col--;
      draw_char(state.cursor_col * CELL_W, state.cursor_row * CELL_H, ' ',
                state.fg_color, state.bg_color);
    }
    return;
  }

  if (state.cursor_col >= state.max_cols) {
    newline();
  }

  draw_char(state.cursor_col * CELL_W, state.cursor_row * CELL_H, c,
            state.fg_color, state.bg_color);
  state.cursor_col++;
}

void putchar_transparent(char c) {
  if (c == '\n') {
    newline();
    return;
  }
  if (c == '\r') {
    state.cursor_col = 0;
    return;
  }
  if (c == '\t') {
    uint32_t next_tab = (state.cursor_col + 4) & ~3u;
    while (state.cursor_col < next_tab && state.cursor_col < state.max_cols) {
      draw_char(state.cursor_col * CELL_W, state.cursor_row * CELL_H, ' ',
                state.fg_color, state.bg_color);
      state.cursor_col++;
    }
    if (state.cursor_col >= state.max_cols)
      newline();
    return;
  }

  if (c == '\b') {
    if (state.cursor_col > 0) {
      state.cursor_col--;
      draw_char(state.cursor_col * CELL_W, state.cursor_row * CELL_H, ' ',
                state.fg_color, state.bg_color);
    }
    return;
  }

  if (state.cursor_col >= state.max_cols) {
    newline();
  }

  draw_char_transparent(state.cursor_col * CELL_W, state.cursor_row * CELL_H, c,
                        state.fg_color);
  state.cursor_col++;
}

void puts(const char *s) {
  if (!s)
    return;
  while (*s) {
    putchar_transparent(*s);
    s++;
  }
}

void puts_colored(const char *s, uint32_t color) {
  uint32_t old_fg = state.fg_color;
  state.fg_color = color;
  puts(s);
  state.fg_color = old_fg;
}

void put_number(int64_t n) {
  char buf[32];
  hal::string::itoa(n, buf);
  puts(buf);
}

void put_hex(uint64_t n) {
  char buf[32];
  hal::string::htoa(n, buf);
  puts("0x");
  puts(buf);
}

uint32_t get_fg() { return state.fg_color; }
uint32_t get_bg() { return state.bg_color; }
void set_fg(uint32_t color) { state.fg_color = color; }
void set_bg(uint32_t color) { state.bg_color = color; }

void set_cursor(uint32_t col, uint32_t row) {
  state.cursor_col = col;
  state.cursor_row = row;
}

uint32_t get_width() { return state.max_cols; }
uint32_t get_height() { return state.max_rows; }
uint64_t get_fb_width() { return state.fb_width; }
uint64_t get_fb_height() { return state.fb_height; }

/* ============================================================
 * PRINTF SEDERHANA
 * ============================================================ */
void printf(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);

  while (*fmt) {
    if (*fmt == '%') {
      fmt++;
      switch (*fmt) {
      case 's': {
        const char *s = va_arg(args, const char *);
        puts(s ? s : "(null)");
        break;
      }
      case 'd': {
        int64_t n = va_arg(args, int64_t);
        numstream(n);
        break;
      }
      case 'u': {
        uint64_t n = va_arg(args, uint64_t);
        numstream(n);
        break;
      }
      case 'x': {
        uint64_t n = va_arg(args, uint64_t);
        put_hex(n);
        break;
      }
      case 'c': {
        char c = (char)va_arg(args, int);
        putchar_transparent(c);
        break;
      }
      case '%': {
        putchar_transparent('%');
        break;
      }
      default:
        putchar_transparent('%');
        putchar_transparent(*fmt);
        break;
      }
    } else {
      putchar_transparent(*fmt);
    }
    fmt++;
  }

  va_end(args);
}

/* ============================================================
 * GRAFIS PRIMITIF TAMBAHAN
 * ============================================================ */
void draw_rect(int32_t x, int32_t y, int32_t w, int32_t h, uint32_t color) {
  fill_rect(x, y, w, 1, color);         /* top */
  fill_rect(x, y + h - 1, w, 1, color); /* bottom */
  fill_rect(x, y, 1, h, color);         /* left */
  fill_rect(x + w - 1, y, 1, h, color); /* right */
}

void draw_line(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint32_t color) {
  int32_t dx = x1 - x0;
  int32_t dy = y1 - y0;
  if (dx < 0)
    dx = -dx;
  if (dy < 0)
    dy = -dy;
  int32_t sx = x0 < x1 ? 1 : -1;
  int32_t sy = y0 < y1 ? 1 : -1;
  int32_t err = dx - dy;
  while (true) {
    draw_pixel(x0, y0, color);
    if (x0 == x1 && y0 == y1)
      break;
    int32_t e2 = 2 * err;
    if (e2 > -dy) {
      err -= dy;
      x0 += sx;
    }
    if (e2 < dx) {
      err += dx;
      y0 += sy;
    }
  }
}

void fill_circle(int32_t cx, int32_t cy, int32_t r, uint32_t color) {
  for (int32_t dy = -r; dy <= r; dy++) {
    for (int32_t dx = -r; dx <= r; dx++) {
      if (dx * dx + dy * dy <= r * r) {
        draw_pixel(cx + dx, cy + dy, color);
      }
    }
  }
}

void fill_rounded_rect(int32_t x, int32_t y, int32_t w, int32_t h, int32_t r,
                       uint32_t color) {
  if (r <= 0) {
    fill_rect(x, y, w, h, color);
    return;
  }
  if (r > h / 2)
    r = h / 2;
  if (r > w / 2)
    r = w / 2;
  /* Isi tengah */
  fill_rect(x + r, y, w - 2 * r, h, color);
  fill_rect(x, y + r, r, h - 2 * r, color);
  fill_rect(x + w - r, y + r, r, h - 2 * r, color);
  /* Sudut bulat */
  for (int32_t dy = -r; dy <= 0; dy++) {
    for (int32_t dx = -r; dx <= 0; dx++) {
      if (dx * dx + dy * dy <= r * r) {
        draw_pixel(x + r + dx, y + r + dy, color);         /* kiri atas */
        draw_pixel(x + w - 1 - r - dx, y + r + dy, color); /* kanan atas */
        draw_pixel(x + r + dx, y + h - 1 - r - dy, color); /* kiri bawah */
        draw_pixel(x + w - 1 - r - dx, y + h - 1 - r - dy,
                   color); /* kanan bawah */
      }
    }
  }
}

void draw_char_transparent(int32_t x, int32_t y, char c, uint32_t fg) {
  int idx = (int)c - 32;
  if (idx < 0 || idx >= 95)
    idx = 0;
  const uint8_t *glyph = font8x8[idx];
  for (int row = 0; row < FONT_H; row++) {
    uint8_t bits = glyph[row];
    for (int col = 0; col < FONT_W; col++) {
      if (bits & (1 << col)) {
        draw_pixel(x + col, y + row, fg);
      }
    }
  }
}

void draw_string(int32_t x, int32_t y, const char *s, uint32_t fg) {
  if (!s)
    return;
  int32_t cx = x;
  while (*s) {
    if (*s == '\n') {
      y += CELL_H;
      cx = x;
      s++;
      continue;
    }
    draw_char_transparent(cx, y, *s, fg);
    cx += CELL_W;
    s++;
  }
}

void draw_string_bg(int32_t x, int32_t y, const char *s, uint32_t fg,
                    uint32_t bg) {
  if (!s)
    return;
  int32_t cx = x;
  while (*s) {
    if (*s == '\n') {
      y += CELL_H;
      cx = x;
      s++;
      continue;
    }
    draw_char(cx, y, *s, fg, bg);
    cx += CELL_W;
    s++;
  }
}

int string_pixel_width(const char *s) {
  if (!s)
    return 0;
  int w = 0;
  while (*s) {
    w += CELL_W;
    s++;
  }
  return w;
}

/* Double buffering — render ke backbuffer, lalu flip ke framebuffer */
void swap_buffers() {
  if (!state.backbuffer) {
    state.backbuffer = (uint32_t *)hal::memory::kmalloc(state.fb_size_bytes);
    if (!state.backbuffer)
      return;
  }
  /* Copy backbuffer ke layar */
  memcpy(state.framebuffer, state.backbuffer, state.fb_size_bytes);
}

void begin_frame() {
  if (!state.backbuffer) {
    state.backbuffer = (uint32_t *)hal::memory::kmalloc(state.fb_size_bytes);
  }
  if (state.backbuffer) {
    state.render_target = state.backbuffer;
  }
}

void end_frame() {
  if (state.backbuffer) {
    memcpy(state.framebuffer, state.backbuffer, state.fb_size_bytes);
  }
  state.render_target = nullptr; /* Kembali ke framebuffer langsung */
}

uint32_t *get_backbuffer() {
  if (!state.backbuffer) {
    state.backbuffer = (uint32_t *)hal::memory::kmalloc(state.fb_size_bytes);
  }
  return state.backbuffer;
}

void *get_framebuffer() { return state.framebuffer; }
uint64_t get_pitch() { return state.fb_pitch; }

/* Blit raw ARGB pixel data ke layar */
void blit_raw(int32_t dx, int32_t dy, int32_t sw, int32_t sh,
              const uint32_t *pixels) {
  if (!pixels)
    return;
  uint32_t *target =
      state.render_target ? state.render_target : state.framebuffer;
  for (int32_t row = 0; row < sh; row++) {
    int32_t sy = dy + row;
    if (sy < 0 || (uint64_t)sy >= state.fb_height)
      continue;
    for (int32_t col = 0; col < sw; col++) {
      int32_t sx = dx + col;
      if (sx < 0 || (uint64_t)sx >= state.fb_width)
        continue;
      uint32_t c = pixels[row * sw + col];
      /* Skip fully transparent pixels */
      if ((c >> 24) == 0 && (c & 0x00FFFFFF) == 0)
        continue;
      uint32_t *pixel =
          (uint32_t *)((uint8_t *)target + sy * state.fb_pitch + sx * 4);
      *pixel = c;
    }
  }
}

namespace general {

template <auto Action> inline void resolve_overflow(int current, int limit) {
  if (current >= limit)
    Action();
}
} // namespace general

void resolve_newline(int current, int limit, int lines_to_skip = 1) {
  if (current >= limit)
    while (lines_to_skip-- > 0) {
      Console::newline();
    }
}

} // namespace Console
