/*
 * LittleOS Gajah PHP - desktop.cpp
 * Window Manager & Desktop Environment — C++ HAL backend
 * Frontend dikendalikan sepenuhnya oleh PHP
 * Tema: Tailwind CSS color palette
 */

#include "desktop.hpp"
#include "console.hpp"
#include "hal.hpp"
#include "themes.hpp"
#include "ui.hpp"
#include <stdint.h>
#include <string.h>

// extern "C" void *memcpy(void *dest, const void *src, size_t n);

Taskbar taskbar;

namespace Desktop {

Core core;
Core &get_core() { return core; }

DesktopState dstate;
DesktopState &get_state() { return dstate; }

static int next_window_id = 1;
static int32_t wallpaper_cache_w = 0;
static int32_t wallpaper_cache_h = 0;
static uint32_t *wallpaper_cache = nullptr;

void init(int32_t screen_w, int32_t screen_h) {
  dstate.screen_w = screen_w;
  dstate.screen_h = screen_h;
  dstate.window_count = 0;
  dstate.active_window = -1;
  dstate.z_count = 0;
  dstate.running = true;
  dstate.needs_redraw = true;
  dstate.start_menu_open = false;
  dstate.dragging = false;
  dstate.drag_window = -1;
  dstate.cursor_x = screen_w / 2;
  dstate.cursor_y = screen_h / 2;
  dstate.context_menu.visible = false;
  dstate.context_menu.item_count = 0;
  next_window_id = 1;

  for (int i = 0; i < MAX_WINDOWS; i++) {
    dstate.windows[i].id = -1;
    dstate.windows[i].state = WindowState::Closed;
  }

  // TASKBAR INITIATION
  taskbar.height = TASKBAR_HEIGHT;
  taskbar.width = screen_w;
  taskbar.setTaskbarDock(DockEdge::Bottom);
}

int find_slot() {
  for (int i = 0; i < MAX_WINDOWS; i++) {
    if (dstate.windows[i].id == -1 ||
        dstate.windows[i].state == WindowState::Closed)
      return i;
  }
  return -1;
}

int find_window_index(int id) {
  for (int i = 0; i < MAX_WINDOWS; i++) {
    if (dstate.windows[i].id == id &&
        dstate.windows[i].state != WindowState::Closed)
      return i;
  }
  return -1;
}

int create_window(const char *title, int32_t x, int32_t y, int32_t w, int32_t h,
                  const char *app_type) {
  int slot = find_slot();
  if (slot < 0)
    return -1;

  Window &win = dstate.windows[slot];
  win.id = next_window_id++;
  hal::string::strncpy(win.title, title, 127);
  win.title[127] = '\0';
  win.x = x;
  win.y = y;
  win.w = w;
  win.h = h;
  win.saved_x = x;
  win.saved_y = y;
  win.saved_w = w;
  win.saved_h = h;
  win.state = WindowState::Normal;
  win.active = true;
  win.dirty = true;
  win.text_content[0] = '\0';
  win.text_len = 0;
  win.text_scroll = 0;
  win.bg_color = WINDOW_BG;
  win.title_color = WINDOW_TITLE_ACTIVE;
  hal::string::strncpy(win.app_type, app_type ? app_type : "", 63);
  win.app_type[63] = '\0';

  /* Deactivate semua window lain */
  for (int i = 0; i < MAX_WINDOWS; i++) {
    if (i != slot && dstate.windows[i].id >= 0 &&
        dstate.windows[i].state != WindowState::Closed) {
      dstate.windows[i].active = false;
      dstate.windows[i].title_color = WINDOW_TITLE_BG;
    }
  }

  dstate.active_window = win.id;

  /* Tambah ke z-order (paling atas) */
  if (dstate.z_count < MAX_WINDOWS) {
    dstate.z_order[dstate.z_count++] = slot;
  }

  dstate.window_count++;
  dstate.needs_redraw = true;
  return win.id;
}

void close_window(int id) {
  int idx = find_window_index(id);
  if (idx < 0)
    return;

  dstate.windows[idx].state = WindowState::Closed;
  dstate.windows[idx].id = -1;
  dstate.window_count--;

  /* Hapus dari z-order */
  for (int i = 0; i < dstate.z_count; i++) {
    if (dstate.z_order[i] == idx) {
      for (int j = i; j < dstate.z_count - 1; j++)
        dstate.z_order[j] = dstate.z_order[j + 1];
      dstate.z_count--;
      break;
    }
  }

  /* Aktifkan window teratas */
  if (dstate.z_count > 0) {
    int top = dstate.z_order[dstate.z_count - 1];
    dstate.windows[top].active = true;
    dstate.windows[top].title_color = WINDOW_TITLE_ACTIVE;
    dstate.active_window = dstate.windows[top].id;
  } else {
    dstate.active_window = -1;
  }

  dstate.needs_redraw = true;
}

void minimize_window(int id) {
  int idx = find_window_index(id);
  if (idx < 0)
    return;
  dstate.windows[idx].state = WindowState::Minimized;
  dstate.needs_redraw = true;
}

void maximize_window(int id) {
  int idx = find_window_index(id);
  if (idx < 0)
    return;
  Window &win = dstate.windows[idx];
  if (win.state == WindowState::Maximized) {
    restore_window(id);
    return;
  }
  win.saved_x = win.x;
  win.saved_y = win.y;
  win.saved_w = win.w;
  win.saved_h = win.h;
  win.x = 0;
  win.y = 0;
  win.w = dstate.screen_w;
  win.h = dstate.screen_h - TASKBAR_HEIGHT;
  win.state = WindowState::Maximized;
  dstate.needs_redraw = true;
}

void restore_window(int id) {
  int idx = find_window_index(id);
  if (idx < 0)
    return;
  Window &win = dstate.windows[idx];
  win.x = win.saved_x;
  win.y = win.saved_y;
  win.w = win.saved_w;
  win.h = win.saved_h;
  win.state = WindowState::Normal;
  dstate.needs_redraw = true;
}

void set_active_window(int id) {
  for (int i = 0; i < MAX_WINDOWS; i++) {
    if (dstate.windows[i].id >= 0 &&
        dstate.windows[i].state != WindowState::Closed) {
      dstate.windows[i].active = (dstate.windows[i].id == id);
      dstate.windows[i].title_color =
          (dstate.windows[i].id == id) ? WINDOW_TITLE_ACTIVE : WINDOW_TITLE_BG;
    }
  }
  dstate.active_window = id;
  dstate.needs_redraw = true;
}

void set_window_text(int id, const char *text) {
  int idx = find_window_index(id);
  if (idx < 0)
    return;
  hal::string::strncpy(dstate.windows[idx].text_content, text, 8191);
  dstate.windows[idx].text_content[8191] = '\0';
  dstate.windows[idx].text_len =
      (int)hal::string::strlen(dstate.windows[idx].text_content);
  dstate.windows[idx].dirty = true;
  dstate.needs_redraw = true;
}

void append_window_text(int id, const char *text) {
  int idx = find_window_index(id);
  if (idx < 0 || !text)
    return;
  Window &win = dstate.windows[idx];
  int add_len = (int)hal::string::strlen(text);
  if (win.text_len + add_len >= 8191) {
    /* Hapus awal teks untuk membuat ruang */
    int keep = 4096;
    int off = win.text_len - keep;
    if (off < 0)
      off = 0;
    memmove(win.text_content, win.text_content + off, keep);
    win.text_len = keep;
    win.text_content[win.text_len] = '\0';
  }
  hal::string::strncpy(win.text_content + win.text_len, text,
                       8191 - win.text_len);
  win.text_len += add_len;
  if (win.text_len > 8191)
    win.text_len = 8191;
  win.text_content[win.text_len] = '\0';
  win.dirty = true;
  dstate.needs_redraw = true;
}

void clear_window_text(int id) {
  int idx = find_window_index(id);
  if (idx < 0)
    return;
  dstate.windows[idx].text_content[0] = '\0';
  dstate.windows[idx].text_len = 0;
  dstate.windows[idx].text_scroll = 0;
  dstate.windows[idx].dirty = true;
  dstate.needs_redraw = true;
}

Window *get_window(int id) {
  int idx = find_window_index(id);
  if (idx < 0)
    return nullptr;
  return &dstate.windows[idx];
}

void bring_to_front(int id) {
  int idx = find_window_index(id);
  if (idx < 0)
    return;
  /* Hapus dari z-order lalu push ke atas */
  int pos = -1;
  for (int i = 0; i < dstate.z_count; i++) {
    if (dstate.z_order[i] == idx) {
      pos = i;
      break;
    }
  }
  if (pos >= 0) {
    for (int i = pos; i < dstate.z_count - 1; i++)
      dstate.z_order[i] = dstate.z_order[i + 1];
    dstate.z_order[dstate.z_count - 1] = idx;
  }
  set_active_window(id);
}

/* ============================================================
 * RENDERING — Menggambar desktop, window, taskbar
 * ============================================================ */

void render_desktop() {
  int32_t desk_h = dstate.screen_h;

  /* Cache the scaled wallpaper on first render — avoids 750K draw_pixel per
   * frame */
  if (!wallpaper_cache) {
    wallpaper_cache_w = dstate.screen_w;
    wallpaper_cache_h = desk_h;
    wallpaper_cache = (uint32_t *)hal::memory::kmalloc(
        (size_t)wallpaper_cache_w * wallpaper_cache_h * 4);
    if (wallpaper_cache) {
      const uint32_t *wp = hal::assets::wallpaper_pixels();
      int src_w = hal::assets::WALLPAPER_W;
      int src_h = hal::assets::WALLPAPER_H;
      for (int32_t y = 0; y < desk_h; y++) {
        int sy = (y * src_h) / desk_h;
        if (sy >= src_h)
          sy = src_h - 1;
        for (int32_t x = 0; x < wallpaper_cache_w; x++) {
          int sx = (x * src_w) / wallpaper_cache_w;
          if (sx >= src_w)
            sx = src_w - 1;
          wallpaper_cache[y * wallpaper_cache_w + x] = wp[sy * src_w + sx];
        }
      }
    }
  }

  /* Blit cached wallpaper directly to render target — row by row memcpy */
  if (wallpaper_cache) {
    uint32_t *target = Console::get_backbuffer();
    if (!target)
      target = (uint32_t *)Console::get_framebuffer();
    if (target) {
      uint64_t pitch = Console::get_pitch();
      for (int32_t y = 0; y < desk_h; y++) {
        uint32_t *dst_row = (uint32_t *)((uint8_t *)target + y * pitch);
        uint32_t *src_row = wallpaper_cache + y * wallpaper_cache_w;
        memcpy(dst_row, src_row, wallpaper_cache_w * 4);
      }
    }
  } else {
    /* Fallback if cache alloc failed */
    Console::fill_rect(0, 0, dstate.screen_w, desk_h, DESKTOP_BG);
  }
}

/* ---- Pixel art icon mini 8x8 ---- */
static void draw_icon_terminal(int32_t x, int32_t y) {
  Console::fill_rounded_rect(x, y, 16, 16, 2, TW_SLATE_900);
  Console::draw_string(x + 2, y + 4, ">_", TW_GREEN_400);
}

static void draw_icon_folder(int32_t x, int32_t y) {
  Console::fill_rect(x, y + 3, 7, 2, TW_AMBER_500);
  Console::fill_rounded_rect(x, y + 4, 16, 11, 2, TW_AMBER_500);
  Console::fill_rect(x + 1, y + 7, 14, 7, TW_YELLOW_400);
}

static void draw_icon_globe(int32_t x, int32_t y) {
  Console::fill_circle(x + 8, y + 8, 7, TW_BLUE_500);
  Console::fill_circle(x + 8, y + 8, 5, TW_SKY_400);
  Console::draw_line(x + 1, y + 8, x + 15, y + 8, TW_BLUE_600);
  Console::draw_line(x + 8, y + 1, x + 8, y + 15, TW_BLUE_600);
}

static void draw_icon_gear(int32_t x, int32_t y) {
  Console::fill_circle(x + 8, y + 8, 7, TW_SLATE_400);
  Console::fill_circle(x + 8, y + 8, 4, TW_SLATE_600);
  Console::fill_circle(x + 8, y + 8, 2, TW_SLATE_400);
}

static void draw_icon_info(int32_t x, int32_t y) {
  Console::fill_circle(x + 8, y + 8, 7, TW_BLUE_600);
  Console::draw_string(x + 5, y + 4, "i", TW_WHITE);
}

static void draw_icon_chart(int32_t x, int32_t y) {
  Console::fill_rect(x + 2, y + 10, 3, 6, TW_GREEN_500);
  Console::fill_rect(x + 6, y + 6, 3, 10, TW_SKY_400);
  Console::fill_rect(x + 10, y + 3, 3, 13, TW_PURPLE_500);
}

static void draw_icon_clock(int32_t x, int32_t y) {
  Console::fill_circle(x + 8, y + 8, 7, TW_WHITE);
  Console::fill_circle(x + 8, y + 8, 6, TW_SLATE_800);
  Console::draw_line(x + 8, y + 8, x + 8, y + 3, TW_WHITE);
  Console::draw_line(x + 8, y + 8, x + 12, y + 8, TW_SKY_400);
}

/* ---- System tray icons ---- */
static void draw_tray_wifi(int32_t x, int32_t y) {
  /* WiFi signal bars — simple arc approximation */
  /* Bar 1 — bottom dot */
  Console::fill_circle(x + 8, y + 13, 1, TW_SKY_400);
  /* Bar 2 — small arc (3 pixels) */
  Console::draw_pixel(x + 5, y + 10, TW_SKY_400);
  Console::draw_pixel(x + 6, y + 9, TW_SKY_400);
  Console::draw_pixel(x + 7, y + 9, TW_SKY_400);
  Console::draw_pixel(x + 8, y + 9, TW_SKY_400);
  Console::draw_pixel(x + 9, y + 9, TW_SKY_400);
  Console::draw_pixel(x + 10, y + 9, TW_SKY_400);
  Console::draw_pixel(x + 11, y + 10, TW_SKY_400);
  /* Bar 3 — medium arc */
  Console::draw_pixel(x + 3, y + 7, TW_SKY_400);
  Console::draw_pixel(x + 4, y + 6, TW_SKY_400);
  Console::draw_pixel(x + 5, y + 5, TW_SKY_400);
  Console::draw_pixel(x + 6, y + 5, TW_SKY_400);
  Console::draw_pixel(x + 10, y + 5, TW_SKY_400);
  Console::draw_pixel(x + 11, y + 5, TW_SKY_400);
  Console::draw_pixel(x + 12, y + 6, TW_SKY_400);
  Console::draw_pixel(x + 13, y + 7, TW_SKY_400);
}

static void draw_tray_volume(int32_t x, int32_t y) {
  /* Speaker icon */
  Console::fill_rect(x + 3, y + 6, 4, 5, TW_SLATE_300);
  Console::fill_rect(x + 7, y + 4, 2, 9, TW_SLATE_300);
  /* Sound waves */
  Console::draw_pixel(x + 11, y + 6, TW_SLATE_400);
  Console::draw_pixel(x + 11, y + 10, TW_SLATE_400);
  Console::draw_pixel(x + 13, y + 5, TW_SLATE_500);
  Console::draw_pixel(x + 13, y + 11, TW_SLATE_500);
}

static void draw_tray_battery(int32_t x, int32_t y) {
  /* Battery outline */
  Console::draw_rect(x + 2, y + 5, 12, 7, TW_SLATE_300);
  Console::fill_rect(x + 14, y + 7, 2, 3, TW_SLATE_300);
  /* Battery level (full) */
  Console::fill_rect(x + 3, y + 6, 10, 5, TW_GREEN_400);
}

static void draw_tray_notification(int32_t x, int32_t y) {
  /* Bell icon */
  Console::fill_circle(x + 8, y + 5, 4, TW_SLATE_300);
  Console::fill_rect(x + 4, y + 5, 9, 6, TW_SLATE_300);
  Console::fill_rect(x + 3, y + 10, 11, 2, TW_SLATE_300);
  Console::fill_circle(x + 8, y + 13, 2, TW_SLATE_300);
}

void render_taskbar() {
  int32_t ty = taskbar.get('y');

  /* ---- Gradient background taskbar ---- */
  /* Dari slate-950 ke slate-900 — efek glassmorphism */
  for (int32_t row = 0; row < TASKBAR_HEIGHT; row++) {
    /* Blend: atas lebih gelap, bawah sedikit lebih terang */
    uint8_t r = 2 + (row * 6) / TASKBAR_HEIGHT;
    uint8_t g = 6 + (row * 10) / TASKBAR_HEIGHT;
    uint8_t b = 23 + (row * 8) / TASKBAR_HEIGHT;
    uint32_t c = 0xFF000000 | (r << 16) | (g << 8) | b;
    Console::fill_rect(0, ty + row, dstate.screen_w, 1, c);
  }

  /* Garis atas accent — tipis 1px gradient biru */
  for (int32_t col = 0; col < dstate.screen_w; col++) {
    /* Gradient dari blue-600 di tengah ke transparan di tepi */
    int32_t center = dstate.screen_w / 2;
    int32_t dist = col > center ? col - center : center - col;
    int32_t max_d = center;
    int alpha = 255 - (dist * 200) / (max_d > 0 ? max_d : 1);
    if (alpha < 30)
      alpha = 30;
    uint8_t rb = (uint8_t)((0x25 * alpha) / 255);
    uint8_t gb = (uint8_t)((0x63 * alpha) / 255);
    uint8_t bb = (uint8_t)((0xEB * alpha) / 255);
    uint32_t c = 0xFF000000 | (rb << 16) | (gb << 8) | bb;
    Console::draw_pixel(col, ty, c);
  }

  /* ---- Start button — PHP logo styled ---- */
  /* Hover glow effect */
  bool start_hover =
      (dstate.cursor_x >= 4 && dstate.cursor_x < 88 &&
       dstate.cursor_y >= ty + 3 && dstate.cursor_y < ty + TASKBAR_HEIGHT - 3);
  uint32_t start_bg = start_hover ? TW_BLUE_500 : TW_BLUE_600;
  Console::fill_rounded_rect(4, ty + 3, 84, TASKBAR_HEIGHT - 6, 6, start_bg);
  /* Inner subtle highlight */
  Console::fill_rect(6, ty + 4, 80, 1, TW_BLUE_400);
  /* Menu icon centered in start button */
  {
    int32_t icon_x = 4 + (84 - hal::assets::MENU_ICON_W) / 2;
    int32_t icon_y =
        ty + 3 + (TASKBAR_HEIGHT - 6 - hal::assets::MENU_ICON_H) / 2;
    Console::blit_raw(icon_x, icon_y, hal::assets::MENU_ICON_W,
                      hal::assets::MENU_ICON_H,
                      hal::assets::menu_icon_pixels());
  }

  /* ---- Pinned apps (Terminal, File Manager) ---- */
  int px = 96;
  /* Terminal pinned */
  {
    bool hover = (dstate.cursor_x >= px && dstate.cursor_x < px + 36 &&
                  dstate.cursor_y >= ty + 3 &&
                  dstate.cursor_y < ty + TASKBAR_HEIGHT - 3);
    uint32_t bg = hover ? TW_SLATE_700 : TW_SLATE_800;
    Console::fill_rounded_rect(px, ty + 3, 36, TASKBAR_HEIGHT - 6, 6, bg);
    draw_icon_terminal(px + 10, ty + 7);
    px += 40;
  }
  /* File Manager pinned */
  {
    bool hover = (dstate.cursor_x >= px && dstate.cursor_x < px + 36 &&
                  dstate.cursor_y >= ty + 3 &&
                  dstate.cursor_y < ty + TASKBAR_HEIGHT - 3);
    uint32_t bg = hover ? TW_SLATE_700 : TW_SLATE_800;
    Console::fill_rounded_rect(px, ty + 3, 36, TASKBAR_HEIGHT - 6, 6, bg);
    draw_icon_folder(px + 10, ty + 7);
    px += 44;
  }

  /* Separator antara pinned dan window list */
  Console::fill_rect(px, ty + 8, 1, TASKBAR_HEIGHT - 16, TW_SLATE_700);
  px += 6;

  /* ---- Window buttons di taskbar ---- */
  int bx = px;
  for (int i = 0; i < dstate.z_count && bx < dstate.screen_w - 260; i++) {
    int idx = dstate.z_order[i];
    Window &win = dstate.windows[idx];
    if (win.id < 0 || win.state == WindowState::Closed)
      continue;

    /* Hover detection */
    bool hover = (dstate.cursor_x >= bx && dstate.cursor_x < bx + 150 &&
                  dstate.cursor_y >= ty + 3 &&
                  dstate.cursor_y < ty + TASKBAR_HEIGHT - 3);

    uint32_t btn_color, txt_color, indicator_color;
    if (win.active) {
      btn_color = TW_SLATE_700;
      txt_color = TW_WHITE;
      indicator_color = TW_BLUE_500;
    } else if (hover) {
      btn_color = 0xFF2A3548; /* slate-750 */
      txt_color = TW_SLATE_200;
      indicator_color = TW_SLATE_500;
    } else {
      btn_color = TW_SLATE_800;
      txt_color = TW_SLATE_400;
      indicator_color = 0x00000000;
    }

    Console::fill_rounded_rect(bx, ty + 3, 150, TASKBAR_HEIGHT - 6, 6,
                               btn_color);

    /* Active indicator — garis bawah biru */
    if (win.active) {
      Console::fill_rounded_rect(bx + 20, ty + TASKBAR_HEIGHT - 5, 110, 2, 1,
                                 indicator_color);
    }

    /* Icon kecil berdasarkan app_type */
    int icon_x = bx + 6;
    int icon_y = ty + 6;
    if (hal::string::strcmp(win.app_type, "terminal") == 0)
      draw_icon_terminal(icon_x, icon_y);
    else if (hal::string::strcmp(win.app_type, "files") == 0)
      draw_icon_folder(icon_x, icon_y);
    else if (hal::string::strcmp(win.app_type, "browser") == 0)
      draw_icon_globe(icon_x, icon_y);
    else if (hal::string::strcmp(win.app_type, "setting") == 0)
      draw_icon_gear(icon_x, icon_y);
    else if (hal::string::strcmp(win.app_type, "sistem") == 0)
      draw_icon_info(icon_x, icon_y);
    else if (hal::string::strcmp(win.app_type, "tasks") == 0)
      draw_icon_chart(icon_x, icon_y);
    else if (hal::string::strcmp(win.app_type, "clock") == 0)
      draw_icon_clock(icon_x, icon_y);
    else
      draw_icon_info(icon_x, icon_y);

    /* Truncate title */
    char short_title[16];
    hal::string::strncpy(short_title, win.title, 14);
    short_title[14] = '\0';
    Console::draw_string(bx + 24, ty + 10, short_title, txt_color);

    bx += 158;
  }

  /* ---- System tray area (kanan) ---- */
  int tray_x = dstate.screen_w - 240;

  /* Separator vertikal */
  Console::fill_rect(tray_x, ty + 8, 1, TASKBAR_HEIGHT - 16, TW_SLATE_700);
  tray_x += 8;

  /* Keyboard layout indicator */
  Console::fill_rounded_rect(tray_x, ty + 6, 28, 22, 3, TW_SLATE_800);
  Console::draw_string(tray_x + 4, ty + 10, "EN", TW_SLATE_300);
  tray_x += 34;

  /* Tray icons */
  draw_tray_wifi(tray_x, ty + 4);
  tray_x += 22;

  draw_tray_volume(tray_x, ty + 4);
  tray_x += 22;

  draw_tray_battery(tray_x, ty + 4);
  tray_x += 22;

  draw_tray_notification(tray_x, ty + 4);
  tray_x += 26;

  /* Separator sebelum jam */
  Console::fill_rect(tray_x, ty + 8, 1, TASKBAR_HEIGHT - 16, TW_SLATE_700);
  tray_x += 8;

  /* Clock + Date - kanan */
  hal::rtc::DateTime now = hal::rtc::get_time();
  char time_str[6];
  time_str[0] = '0' + (now.hour / 10);
  time_str[1] = '0' + (now.hour % 10);
  time_str[2] = ':';
  time_str[3] = '0' + (now.minute / 10);
  time_str[4] = '0' + (now.minute % 10);
  time_str[5] = '\0';

  char date_str[11];
  date_str[0] = '0' + (now.day / 10);
  date_str[1] = '0' + (now.day % 10);
  date_str[2] = '/';
  date_str[3] = '0' + (now.month / 10);
  date_str[4] = '0' + (now.month % 10);
  date_str[5] = '/';
  date_str[6] = '2';
  date_str[7] = '0';
  date_str[8] = '0' + ((now.year % 100) / 10);
  date_str[9] = '0' + (now.year % 10);
  date_str[10] = '\0';

  Console::draw_string(tray_x, ty + 5, time_str, TW_SLATE_100);
  Console::draw_string(tray_x, ty + 18, date_str, TW_SLATE_400);
}

void render_window(Window *win) {
  if (!win || win->state == WindowState::Minimized ||
      win->state == WindowState::Closed)
    return;

  int32_t x = win->x, y = win->y, w = win->w, h = win->h;

  /* Shadow */
  Console::fill_rect(x + 3, y + 3, w, h, 0x40000000);

  /* Border */
  Console::fill_rounded_rect(x, y, w, h, 6,
                             win->active ? TW_SLATE_600 : TW_SLATE_700);

  /* Window body */
  Console::fill_rect(x + 1, y + TITLE_BAR_H, w - 2, h - TITLE_BAR_H - 1,
                     win->bg_color);

  /* Title bar */
  Console::fill_rounded_rect(x, y, w, TITLE_BAR_H, 6, win->title_color);
  /* Rata bawah title bar */
  Console::fill_rect(x, y + TITLE_BAR_H - 8, w, 8, win->title_color);

  /* Title text */
  Console::draw_string(x + 12, y + 7, win->title, TW_WHITE);

  /* Close button (X) - kanan atas */
  int bx = x + w - 28;
  int by = y + 4;
  Console::fill_rounded_rect(bx, by, 20, 20, 4, BTN_CLOSE_BG);
  Console::draw_char_transparent(bx + 6, by + 6, 'x', TW_WHITE);

  /* Maximize button */
  bx -= 24;
  Console::fill_rounded_rect(bx, by, 20, 20, 4, BTN_MAX_BG);
  Console::draw_char_transparent(bx + 6, by + 6, '+', TW_SLATE_900);

  /* Minimize button */
  bx -= 24;
  Console::fill_rounded_rect(bx, by, 20, 20, 4, BTN_MIN_BG);
  Console::draw_char_transparent(bx + 6, by + 6, '-', TW_SLATE_900);

  /* Render teks konten */
  if (win->text_len > 0) {
    int cx = x + 8;
    int cy = y + TITLE_BAR_H + 6;
    int max_cy = y + h - 12;
    const char *p = win->text_content;
    while (*p && cy < max_cy) {
      if (*p == '\n') {
        cy += 10;
        cx = x + 8;
        p++;
        continue;
      }
      if (cx + 9 > x + w - 8) {
        cy += 10;
        cx = x + 8;
      }
      if (cy < max_cy) {
        Console::draw_char_transparent(cx, cy, *p, WINDOW_TEXT);
      }
      cx += 9;
      p++;
    }
  }
}

void render_start_menu() {
  if (!dstate.start_menu_open)
    return;

  int32_t mx = 4;
  int32_t my = dstate.screen_h - TASKBAR_HEIGHT - 310;
  int32_t mw = 260;
  int32_t mh = 310;

  /* Shadow */
  Console::fill_rounded_rect(mx + 4, my + 4, mw, mh, 10, 0x60000000);

  /* Background — glassmorphism dark */
  Console::fill_rounded_rect(mx, my, mw, mh, 10, 0xF0101828);

  /* Border halus */
  Console::draw_rect(mx, my, mw, mh, TW_SLATE_700);

  /* Header area */
  Console::fill_rounded_rect(mx + 2, my + 2, mw - 4, 50, 8, TW_SLATE_800);
  Console::draw_string(mx + 16, my + 10, "LittleOS Gajah", TW_SKY_400);
  Console::draw_string(mx + 16, my + 26, "PHP 8.2 Runtime", TW_SLATE_500);

  /* Divider */
  Console::fill_rect(mx + 12, my + 56, mw - 24, 1, TW_SLATE_700);

  /* Menu items dengan icon */
  struct MenuItem {
    const char *label;
    void (*draw_icon)(int32_t, int32_t);
  };
  MenuItem items[] = {
      {"Sistem", draw_icon_info},         {"Terminal", draw_icon_terminal},
      {"File Manager", draw_icon_folder}, {"Browser", draw_icon_globe},
      {"Setting", draw_icon_gear},        {"Task Manager", draw_icon_chart},
      {"Clock Date", draw_icon_clock}};
  const int item_count = 7;

  for (int i = 0; i < item_count; i++) {
    int iy = my + 64 + i * 34;
    /* Hover check */
    bool hover = (dstate.cursor_x >= mx + 6 && dstate.cursor_x < mx + mw - 6 &&
                  dstate.cursor_y >= iy && dstate.cursor_y < iy + 32);
    if (hover) {
      Console::fill_rounded_rect(mx + 6, iy, mw - 12, 32, 6, TW_BLUE_600);
    }

    /* Icon */
    items[i].draw_icon(mx + 16, iy + 8);

    /* Label */
    Console::draw_string(mx + 40, iy + 10, items[i].label,
                         hover ? TW_WHITE : TW_SLATE_200);
  }
}
void render_cursor_efficient(int32_t x, int32_t y) {
  // Definisi bentuk cursor (x_offset_hitam, x_offset_putih_start,
  // x_offset_putih_end) Menggunakan static agar tabel ini tidak di-generate
  // ulang setiap frame
  static const struct {
    int8_t border;
    int8_t start;
    int8_t end;
  } shape[] = {{0, 1, 1}, {0, 1, 2},  {0, 1, 3},  {0, 1, 4},
               {0, 1, 5}, {0, 1, 6},  {0, 1, 7},  {0, 1, 8},
               {0, 1, 9}, {0, 1, 10}, {0, 1, 10}, {0, 1, 10},
               {0, 1, 7}, {0, 1, 4},  {0, 1, 1},  {0, 0, 0}};

  for (int i = 0; i < 16; i++) {
    int iy = y + i;

    // 1. Gambar Border Kiri (Selalu di x)
    Console::draw_pixel(x, iy, TW_BLACK);

    // 2. Gambar Bagian Putih (Batch fill per baris)
    for (int j = shape[i].start; j <= shape[i].end; j++) {
      Console::draw_pixel(x + j, iy, TW_WHITE);
    }

    // 3. Gambar Border Kanan/Diagonal
    if (i < 12) {
      int border_x = x + shape[i].end;
      Console::draw_pixel(border_x, iy, TW_BLACK);
    }
  }
}

void render_cursor(int32_t x, int32_t y) {
  /* Border kiri */
  for (int i = 0; i < 16; i++) {
    Console::draw_pixel(x, y + i, TW_BLACK);
  }
  /* Cursor segitiga putih dengan border hitam — lebih halus */
  for (int i = 0; i < 16; i++) {
    int w = (i < 12) ? (i * 10 / 12) : (10 - (i - 12) * 3);
    for (int j = 0; j <= w && j < 11; j++) {
      Console::draw_pixel(x + j, y + i, TW_WHITE);
    }
  }
  /* Border kanan diagonal */
  for (int i = 0; i < 12; i++) {
    int w = i * 10 / 12;
    Console::draw_pixel(x + w, y + i, TW_BLACK);
  }
}

/* ---- Context Menu ---- */
void open_context_menu(int32_t x, int32_t y, const char *context) {
  ContextMenu &cm = dstate.context_menu;
  cm.visible = true;
  cm.x = x;
  cm.y = y;
  cm.item_count = 0;

  auto add_item = [&](const char *label, const char *action) {
    if (cm.item_count >= MAX_CTX_ITEMS)
      return;
    ContextMenuItem &it = cm.items[cm.item_count++];
    hal::string::strncpy(it.label, label, 63);
    it.label[63] = '\0';
    hal::string::strncpy(it.action, action, 63);
    it.action[63] = '\0';
    it.separator = false;
  };

  auto add_sep = [&]() {
    if (cm.item_count >= MAX_CTX_ITEMS)
      return;
    cm.items[cm.item_count].separator = true;
    cm.items[cm.item_count].label[0] = '\0';
    cm.items[cm.item_count].action[0] = '\0';
    cm.item_count++;
  };

  if (hal::string::strcmp(context, "desktop") == 0) {
    add_item("Terminal PHP", "open_terminal");
    add_item("File Manager", "open_files");
    add_item("Browser", "open_browser");
    add_sep();
    add_item("Ubah Wallpaper", "change_wallpaper");
    add_item("Pengaturan Display", "display_settings");
    add_sep();
    add_item("Sistem Info", "open_sistem");
    add_item("Task Manager", "open_tasks");
    add_sep();
    add_item("Refresh Desktop", "refresh");
    add_item("Tentang LittleOS", "about");
  } else if (hal::string::strcmp(context, "taskbar") == 0) {
    add_item("Task Manager", "open_tasks");
    add_item("Pengaturan", "open_setting");
    add_sep();
    add_item("Tampilkan Desktop", "show_desktop");
  } else if (hal::string::strcmp(context, "window") == 0) {
    add_item("Tutup Window", "close_win");
    add_item("Maximize", "maximize_win");
    add_item("Minimize", "minimize_win");
    add_sep();
    add_item("Pindah ke Depan", "bring_front");
  }

  /* Pastikan context menu tidak keluar layar */
  int32_t menu_w = 210;
  int32_t item_h = 28;
  int32_t menu_h = 12; /* padding */
  for (int i = 0; i < cm.item_count; i++) {
    menu_h += cm.items[i].separator ? 9 : item_h;
  }
  menu_h += 8;

  if (cm.x + menu_w > dstate.screen_w)
    cm.x = dstate.screen_w - menu_w - 4;
  if (cm.y + menu_h > dstate.screen_h - TASKBAR_HEIGHT)
    cm.y = dstate.screen_h - TASKBAR_HEIGHT - menu_h - 4;
  if (cm.x < 0)
    cm.x = 4;
  if (cm.y < 0)
    cm.y = 4;

  dstate.needs_redraw = true;
}

void close_context_menu() {
  dstate.context_menu.visible = false;
  dstate.needs_redraw = true;
}

void render_context_menu() {
  ContextMenu &cm = dstate.context_menu;
  if (!cm.visible || cm.item_count == 0)
    return;

  int32_t menu_w = 210;
  int32_t item_h = 28;

  /* Hitung total tinggi */
  int32_t total_h = 12;
  for (int i = 0; i < cm.item_count; i++) {
    total_h += cm.items[i].separator ? 9 : item_h;
  }
  total_h += 8;

  /* Shadow */
  Console::fill_rounded_rect(cm.x + 3, cm.y + 3, menu_w, total_h, 8,
                             0x60000000);

  /* Background */
  Console::fill_rounded_rect(cm.x, cm.y, menu_w, total_h, 8, 0xF0121D2E);

  /* Border */
  Console::draw_rect(cm.x, cm.y, menu_w, total_h, TW_SLATE_600);

  /* Items */
  int32_t iy = cm.y + 6;
  for (int i = 0; i < cm.item_count; i++) {
    if (cm.items[i].separator) {
      iy += 4;
      Console::fill_rect(cm.x + 12, iy, menu_w - 24, 1, TW_SLATE_700);
      iy += 5;
      continue;
    }

    bool hover =
        (hal::mouse::get_x() >= cm.x + 4 &&
         hal::mouse::get_x() < cm.x + menu_w - 4 && hal::mouse::get_y() >= iy &&
         hal::mouse::get_y() < iy + item_h);
    if (hover) {
      Console::fill_rounded_rect(cm.x + 4, iy, menu_w - 8, item_h, 4,
                                 TW_BLUE_600);
    }

    Console::draw_string(cm.x + 16, iy + 8, cm.items[i].label,
                         hover ? TW_WHITE : TW_SLATE_200);
    iy += item_h;
  }
}

// uint64_t TIMEOUT = 1;
// bool HIDE_CURSOR = false;

void render_all() {
  /* Begin double-buffered frame */
  Console::begin_frame();

  render_desktop();

  /* Render windows bottom-to-top */
  for (int i = 0; i < dstate.z_count; i++) {
    int idx = dstate.z_order[i];
    if (dstate.windows[idx].id >= 0)
      render_window(&dstate.windows[idx]);
  }

  render_taskbar();

  if (dstate.start_menu_open) {
    render_start_menu();
  }

  if (dstate.context_menu.visible) {
    render_context_menu();
  }

  // if (!hal::mouse::getState().isMoving) {
  //   if (TIMEOUT > 0 && Desktop::get_core().time.delta.sec != 0)
  //     TIMEOUT--;
  // } else {
  //   TIMEOUT = 1;
  // }
  //
  // if (TIMEOUT <= 0)
  //   HIDE_CURSOR = true;
  // else
  //   HIDE_CURSOR = false;
  //
  // if (!HIDE_CURSOR)
  render_cursor_efficient(hal::mouse::get_x(), hal::mouse::get_y());

  /* End frame: copy backbuffer → framebuffer */
  Console::end_frame();
}

/* ============================================================
 * HIT TESTING
 * ============================================================ */

int hit_test_window(int32_t x, int32_t y) {
  /* Test dari atas ke bawah (z-order terbalik) */
  for (int i = dstate.z_count - 1; i >= 0; i--) {
    int idx = dstate.z_order[i];
    Window &win = dstate.windows[idx];
    if (win.id < 0 || (win.state != WindowState::Normal &&
                       win.state != WindowState::Maximized))
      continue;
    if (x >= win.x && x < win.x + win.w && y >= win.y && y < win.y + win.h)
      return win.id;
  }
  return -1;
}

int hit_test_close_btn(int id, int32_t x, int32_t y) {
  int idx = find_window_index(id);
  if (idx < 0)
    return 0;
  Window &win = dstate.windows[idx];
  int bx = win.x + win.w - 28;
  int by = win.y + 4;
  return (x >= bx && x < bx + 20 && y >= by && y < by + 20) ? 1 : 0;
}

int hit_test_maximize_btn(int id, int32_t x, int32_t y) {
  int idx = find_window_index(id);
  if (idx < 0)
    return 0;
  Window &win = dstate.windows[idx];
  int bx = win.x + win.w - 52;
  int by = win.y + 4;
  return (x >= bx && x < bx + 20 && y >= by && y < by + 20) ? 1 : 0;
}

int hit_test_minimize_btn(int id, int32_t x, int32_t y) {
  int idx = find_window_index(id);
  if (idx < 0)
    return 0;
  Window &win = dstate.windows[idx];
  int bx = win.x + win.w - 76;
  int by = win.y + 4;
  return (x >= bx && x < bx + 20 && y >= by && y < by + 20) ? 1 : 0;
}

} // namespace Desktop
