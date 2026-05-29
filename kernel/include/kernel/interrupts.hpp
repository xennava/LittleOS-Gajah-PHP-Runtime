#pragma once

#include <stdint.h>

/* ============================================================
 * INTERRUPT FRAME (di-push oleh CPU pada x86_64)
 * ============================================================ */

extern "C" void *isr_table[];

extern "C" struct interrupt_frame {
  uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
  uint64_t rsi, rdi, rbp, rdx, rcx, rbx, rax;

  uint64_t int_no;
  uint64_t err_code;

  uint64_t rip;
  uint64_t cs;
  uint64_t rflags;
  uint64_t rsp;
  uint64_t ss;
};

using isr_t = void (*)(interrupt_frame *);

static isr_t isr_fun_handlers[256] = {0};

extern "C" void isr_handler(interrupt_frame *frame);
// register a function to isr_fun_handlers array
inline void register_interrupt_handler(int n, isr_t fun) {
  isr_fun_handlers[n] = fun;
}

typedef interrupt_frame InterruptFrame;

// TIMER
extern "C" volatile uint64_t timer_ticks;
