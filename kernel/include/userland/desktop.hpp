#pragma once

/* ============================================================
 * DESKTOP — Window Manager (C++ backend)
 * ============================================================ */
#include <stdint.h>

namespace Desktop {

static const int MAX_WINDOWS = 32;
static const int TITLE_BAR_H = 28;
static const int BORDER_W = 1;

enum class WindowState : uint8_t { Normal, Minimized, Maximized, Closed };

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

struct Core {
  TimeKeeper time;
};

extern Core &get_core();

struct Window {
  char text_content[8192];
  char title[128];
  char app_type[64];
  int32_t x, y, w, h;
  int32_t saved_x, saved_y, saved_w, saved_h;
  int32_t id;
  uint32_t bg_color;
  uint32_t title_color;
  int text_len;
  int text_scroll;
  bool active;
  bool dirty;
  WindowState state;
};

/* Context menu item */
static const int MAX_CTX_ITEMS = 12;
struct ContextMenuItem {
  char label[64];
  char action[64];
  bool separator; /* true = garis pemisah, bukan item */
};

struct ContextMenu {
  ContextMenuItem items[MAX_CTX_ITEMS];
  int32_t x, y;
  int item_count;
  bool visible;
};

struct DesktopState {
  Window windows[MAX_WINDOWS];
  ContextMenu context_menu;
  int32_t cursor_x, cursor_y;
  int32_t drag_offset_x, drag_offset_y;
  int32_t screen_w, screen_h;
  int drag_window;
  int window_count;
  int active_window;
  int z_order[MAX_WINDOWS];
  int z_count;
  bool start_menu_open;
  bool dragging;
  bool needs_redraw;
  bool running;
};

void init(int32_t screen_w, int32_t screen_h);
DesktopState &get_state();

int create_window(const char *title, int32_t x, int32_t y, int32_t w, int32_t h,
                  const char *app_type);
void close_window(int id);
void minimize_window(int id);
void maximize_window(int id);
void restore_window(int id);
void set_active_window(int id);
void set_window_text(int id, const char *text);
void append_window_text(int id, const char *text);
void clear_window_text(int id);
Window *get_window(int id);
void bring_to_front(int id);

void render_desktop();
void render_taskbar();
void render_window(Window *win);
void render_start_menu();
void render_context_menu();
void render_cursor(int32_t x, int32_t y);
void render_all();

void open_context_menu(int32_t x, int32_t y, const char *context);
void close_context_menu();

int hit_test_window(int32_t x, int32_t y);
int hit_test_close_btn(int id, int32_t x, int32_t y);
int hit_test_maximize_btn(int id, int32_t x, int32_t y);
int hit_test_minimize_btn(int id, int32_t x, int32_t y);

} // namespace Desktop
