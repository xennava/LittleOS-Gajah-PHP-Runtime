#include "utils.hpp"
#include "console.hpp"
#include "pow10.hpp"
#include <stdint.h>

void numstream(int64_t u, int x, int y) {
  if (x >= 0)
    Console::state.cursor_col = x;
  if (y < 0)
    y = Console::state.cursor_row;
  else
    Console::state.cursor_row = y;

  if (Console::state.cursor_col > Console::state.max_cols)
    Console::newline();

  y *= Console::CELL_H;

  if (u == 0) {
    Console::draw_char_transparent(Console::state.cursor_col * Console::CELL_W,
                                   y, '0', Console::state.fg_color);
    Console::state.cursor_col++;
    return;
  }

  if (u < 0) {
    Console::draw_char_transparent(Console::state.cursor_col * Console::CELL_W,
                                   y, '-', Console::state.fg_color);
    Console::state.cursor_col++;
    u = -u;
  }

  int8_t big = 0;

  int64_t digit = 10;
  while (digit <= u) {
    big++;
    digit *= 10;
  }

  uint64_t rem = u;
  while (big >= 0) {
    digit = rem / num::pow10[big];
    Console::draw_char_transparent(Console::state.cursor_col * Console::CELL_W,
                                   y, '0' + digit, Console::state.fg_color);
    Console::state.cursor_col++;
    if (Console::state.cursor_col >= Console::state.max_cols)
      Console::newline();
    rem -= digit * num::pow10[big];
    big--;
  }
}
