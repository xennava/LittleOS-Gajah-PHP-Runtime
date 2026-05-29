#pragma once
#include "kernel.hpp"
#include <stdint.h>

namespace Console {
constexpr int FONT_W = 8;
constexpr int FONT_H = 8;
constexpr int CHAR_SPACING = 1;
constexpr int LINE_SPACING = 2;
constexpr int CELL_W = FONT_W + CHAR_SPACING;
constexpr int CELL_H = FONT_H + LINE_SPACING;

struct ConsoleState {
  uint32_t *framebuffer;
  uint32_t *backbuffer;
  uint32_t *render_target; /* Drawing target: framebuffer or backbuffer */
  uint64_t fb_width;
  uint64_t fb_height;
  uint64_t fb_pitch;
  uint32_t fg_color;
  uint32_t bg_color;
  uint32_t cursor_col;
  uint32_t cursor_row;
  uint32_t max_cols;
  uint32_t max_rows;
  uint64_t fb_size_bytes;
}

extern state;

void init(void *fb_addr, uint64_t width, uint64_t height, uint64_t pitch);
void initc(framebuffer_t &fb_t);
void putchar(char c);
void puts(const char *s);
void puts_colored(const char *s, uint32_t color);
void put_number(int64_t n);
void put_hex(uint64_t n);
void set_fg(uint32_t color);
void set_bg(uint32_t color);
void clear();
void newline();
void scroll_up();
void set_cursor(uint32_t col, uint32_t row);
uint32_t get_width();
uint32_t get_height();
uint64_t get_fb_width();
uint64_t get_fb_height();
uint32_t get_fg();
uint32_t get_bg();

void printf(const char *fmt, ...);

/* Grafis primitif */
void draw_pixel(int32_t x, int32_t y, uint32_t color);
void fill_rect(int32_t x, int32_t y, int32_t w, int32_t h, uint32_t color);
void draw_rect(int32_t x, int32_t y, int32_t w, int32_t h, uint32_t color);
void draw_line(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint32_t color);
void fill_circle(int32_t cx, int32_t cy, int32_t r, uint32_t color);
void fill_rounded_rect(int32_t x, int32_t y, int32_t w, int32_t h, int32_t r,
                       uint32_t color);
void draw_char(int32_t x, int32_t y, char c, uint32_t fg, uint32_t bg);
void draw_char_transparent(int32_t x, int32_t y, char c, uint32_t fg);
void draw_string(int32_t x, int32_t y, const char *s, uint32_t fg);
void draw_string_bg(int32_t x, int32_t y, const char *s, uint32_t fg,
                    uint32_t bg);
int string_pixel_width(const char *s);

/* Double buffering */
void swap_buffers();
void begin_frame(); /* Set render target ke backbuffer */
void end_frame(); /* Copy backbuffer → framebuffer, kembali ke direct render */
uint32_t *get_backbuffer();
void *get_framebuffer();
uint64_t get_pitch();

/* Image blitting */
void blit_raw(int32_t dx, int32_t dy, int32_t sw, int32_t sh,
              const uint32_t *pixels);
} // namespace Console
