#pragma once

#include "desktop.hpp"
#include <stdint.h>

enum class DockEdge : uint8_t { Top, Right, Bottom, Left };
class Taskbar {
public:
  int32_t height, width;

  void set(int x, int y) {
    this->x = x;
    this->y = y;
  }
  void set(int32_t index) { this->index = index; }
  void setTaskbarDock(DockEdge edge) {
    int screen_height = Desktop::get_state().screen_h;
    if (screen_height <= 5)
      return;
    if (edge == DockEdge::Bottom)
      y = Desktop::get_state().screen_h - height;
  }

  int64_t get(const char xyz) {
    if (xyz == 'x' || xyz == 'X') {
      return this->x;
    }
    if (xyz == 'y' || xyz == 'Y') {
      return this->y;
    }

    return 0;
  }

private:
  int32_t index;
  int x, y;
};

extern "C" Taskbar taskbar;
