/*
 * LittleOS Gajah PHP - interrupts.cpp
 * IDT, PIC, dan penanganan interupsi (C++)
 * Timer IRQ 0, Keyboard IRQ 1, Mouse IRQ 12
 */

#include "interrupts.hpp"
#include "console.hpp"
#include "hal.hpp"
#include "kernel.hpp"
#include <stdint.h>
#include <sys/types.h>

/* Referensi external untuk mouse packet handler */
extern "C" void mouse_handle_packet(uint8_t data);

namespace hal {

/* ============================================================
 * STRUKTUR IDT x86_64
 * ============================================================ */
struct IDTEntry {
  uint16_t offset_low;
  uint16_t selector;
  uint8_t ist;
  uint8_t type_attr;
  uint16_t offset_mid;
  uint32_t offset_high;
  uint32_t reserved;
} PACKED;

struct IDTR {
  uint16_t limit;
  uint64_t base;
} PACKED;

static IDTEntry idt_entries[256];
static IDTR idtr;

/* ============================================================
 * PIC (Programmable Interrupt Controller)
 * ============================================================ */
#define PIC1_CMD 0x20
#define PIC1_DATA 0x21
#define PIC2_CMD 0xA0
#define PIC2_DATA 0xA1
#define PIC_EOI 0x20
#define ICW1_INIT 0x11
#define ICW4_8086 0x01

static void pic_remap() {
  /* Simpan mask */
  uint8_t mask1 = ports::inb(PIC1_DATA);
  uint8_t mask2 = ports::inb(PIC2_DATA);

  /* Mulai inisialisasi PIC */
  ports::outb(PIC1_CMD, ICW1_INIT);
  ports::io_wait();
  ports::outb(PIC2_CMD, ICW1_INIT);
  ports::io_wait();

  /* Set offset: IRQ 0-7 -> INT 32-39, IRQ 8-15 -> INT 40-47 */
  ports::outb(PIC1_DATA, 0x20);
  ports::io_wait();
  ports::outb(PIC2_DATA, 0x28);
  ports::io_wait();

  /* Hubungkan PIC1 dan PIC2 */
  ports::outb(PIC1_DATA, 0x04);
  ports::io_wait();
  ports::outb(PIC2_DATA, 0x02);
  ports::io_wait();

  /* Mode 8086 */
  ports::outb(PIC1_DATA, ICW4_8086);
  ports::io_wait();
  ports::outb(PIC2_DATA, ICW4_8086);
  ports::io_wait();

  /* Restore mask — aktifkan timer (IRQ0) dan keyboard (IRQ1) */
  UNUSED(mask1);
  UNUSED(mask2);
  ports::outb(PIC1_DATA, 0xF8); /* 11111000 = aktifkan IRQ 0, 1, 2 (cascade) */
  ports::outb(PIC2_DATA, 0xEF); /* 11101111 = aktifkan IRQ 12 (mouse) */
}

static void pic_send_eoi(uint8_t irq) {
  if (irq >= 8) {
    ports::outb(PIC2_CMD, PIC_EOI);
  }
  ports::outb(PIC1_CMD, PIC_EOI);
}

/* ============================================================
 * TIMER STATE
 * ============================================================ */
volatile uint64_t timer_ticks = 0;

/* ============================================================
 * KEYBOARD STATE
 * ============================================================ */
#define KB_BUFFER_SIZE 256
static volatile char kb_buffer[KB_BUFFER_SIZE];
static volatile uint32_t kb_head = 0;
static volatile uint32_t kb_tail = 0;
static volatile uint8_t kb_modifiers = 0;
#define KB_MOD_SHIFT (1 << 0)
#define KB_MOD_CTRL (1 << 1)
#define KB_MOD_CAPS (1 << 2)

/* Scancode ke ASCII (US layout) — lowercase */
static const char scancode_to_ascii[128] = {
    0,    27,   '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-',  '=',
    '\b', '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[',  ']',
    '\n', 0,    'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    0,    '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,    '*',
    0,    ' ',  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,    0,
    0,    0,    0,   0,   '-', 0,   0,   0,   '+', 0,   0,   0,   0,    0,
    0,    0,    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,    0,
    0,    0,    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,    0,
    0,    0,    0,   0,   0,   0,   0};

/* Scancode ke ASCII — uppercase (shift) */
static const char scancode_to_ascii_shift[128] = {
    0,    27,   '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+',
    '\b', '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}',
    '\n', 0,    'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',
    0,    '|',  'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0,   '*',
    0,    ' ',  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,    0,    0,   0,   '-', 0,   0,   0,   '+', 0,   0,   0,   0,   0,
    0,    0,    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,    0,    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,    0,    0,   0,   0,   0,   0};

static void kb_buffer_push(char c) {
  uint32_t next = (kb_head + 1) % KB_BUFFER_SIZE;
  if (next != kb_tail) {
    kb_buffer[kb_head] = c;
    kb_head = next;
  }
}

/* ============================================================
 * ISR HANDLERS
 * ============================================================ */
void timer_isr(InterruptFrame *frame) {
  UNUSED(frame);
  timer_ticks += 1;
}

static void keyboard_isr(InterruptFrame *frame) {

  UNUSED(frame);
  uint8_t scancode = ports::inb(0x60);

  /* Key release (bit 7 set) */
  if (scancode & 0x80) {
    uint8_t release = scancode & 0x7F;
    if (release == 0x2A || release == 0x36) { /* Shift release */
      kb_modifiers &= ~KB_MOD_SHIFT;
    }
    if (release == 0x1D) { /* Ctrl release */
      kb_modifiers &= ~KB_MOD_CTRL;
    }
  } else {
    /* Key press */
    if (scancode == 0x2A || scancode == 0x36) { /* Shift press */
      kb_modifiers |= KB_MOD_SHIFT;
    } else if (scancode == 0x1D) { /* Ctrl press */
      kb_modifiers |= KB_MOD_CTRL;
    } else if (scancode == 0x3A) { /* Caps Lock toggle */
      kb_modifiers ^= KB_MOD_CAPS;
    } else {
      char c = 0;
      bool use_shift = (kb_modifiers & KB_MOD_SHIFT) != 0;
      bool use_caps = (kb_modifiers & KB_MOD_CAPS) != 0;

      if (use_shift) {
        c = scancode_to_ascii_shift[scancode];
      } else {
        c = scancode_to_ascii[scancode];
      }

      /* Caps Lock hanya mempengaruhi huruf */
      if (use_caps && !use_shift && c >= 'a' && c <= 'z') {
        c = c - 'a' + 'A';
      } else if (use_caps && use_shift && c >= 'A' && c <= 'Z') {
        c = c - 'A' + 'a';
      }

      if (c != 0) {
        kb_buffer_push(c);
      }
    }
  }
}

/* Mouse ISR (IRQ 12 = vektor 44) */
static void mouse_isr(InterruptFrame *frame) {
  UNUSED(frame);
  uint8_t data = ports::inb(0x60);
  mouse_handle_packet(data);
}

/* Variabel scancode buffer untuk GUI mode */
static volatile uint8_t scancode_buffer[64];
static volatile uint32_t sc_head = 0;
static volatile uint32_t sc_tail = 0;

/* ============================================================
 * IDT SETUP
 * ============================================================ */
static void idt_set_entry(uint8_t vector, void *handler, uint8_t type_attr) {
  uint64_t addr = (uint64_t)handler;
  idt_entries[vector].offset_low = (uint16_t)(addr & 0xFFFF);
  idt_entries[vector].selector =
      0x28; /* Limine sets GDT entry 5 (0x28) as 64-bit code segment */
  idt_entries[vector].ist = 0;
  idt_entries[vector].type_attr = type_attr;
  idt_entries[vector].offset_mid = (uint16_t)((addr >> 16) & 0xFFFF);
  idt_entries[vector].offset_high = (uint32_t)((addr >> 32) & 0xFFFFFFFF);
  idt_entries[vector].reserved = 0;
}

extern "C" void isr_handler(interrupt_frame *frame) {
  uint8_t n = frame->int_no;

  if (isr_fun_handlers[n]) {
    isr_fun_handlers[n](frame);
  }

  if (n >= 32 && n <= 47) {
    pic_send_eoi(n - 32);
  }
}

void test_isr(InterruptFrame *frame) { UNUSED(frame); }

namespace interrupts {

void init() {
  /* Inisialisasi semua entry IDT dengan default handler */
  for (int i = 0; i < 256; i += 8) {
    idt_set_entry((uint8_t)i, isr_table[i], 0x8E);
    idt_set_entry((uint8_t)i + 1, isr_table[i + 1], 0x8E);
    idt_set_entry((uint8_t)i + 2, isr_table[i + 2], 0x8E);
    idt_set_entry((uint8_t)i + 3, isr_table[i + 3], 0x8E);
    idt_set_entry((uint8_t)i + 4, isr_table[i + 4], 0x8E);
    idt_set_entry((uint8_t)i + 5, isr_table[i + 5], 0x8E);
    idt_set_entry((uint8_t)i + 6, isr_table[i + 6], 0x8E);
    idt_set_entry((uint8_t)i + 7, isr_table[i + 7], 0x8E);
  }

  /* Remap PIC */
  pic_remap();

  /* Set handler timer (IRQ 0 = vektor 32) */
  register_interrupt_handler(32, timer_isr);
  /* Set handler keyboard (IRQ 1 = vektor 33) */
  register_interrupt_handler(33, keyboard_isr);
  // /* Set handler mouse (IRQ 12 = vektor 44) */
  register_interrupt_handler(44, mouse_isr);

  /* Load IDT */
  idtr.limit = sizeof(idt_entries) - 1;
  idtr.base = (uint64_t)&idt_entries;
  asm volatile("lidt %0" : : "m"(idtr));
}

void enable() { asm volatile("sti"); }

void disable() { asm volatile("cli"); }

} // namespace interrupts

/* ============================================================
 * TIMER IMPLEMENTATION
 * ============================================================ */
#define PIT_FREQ 1193182
#define TARGET_FREQ 1000 /* 1000 Hz = 1ms per tick */
#define PIT_CH0_DATA 0x40
#define PIT_CMD 0x43

namespace timer {
static uint64_t tsc_base = 0;
static uint64_t tsc_per_ms = 1;
static uint64_t tsc_per_us = 1;
static uint64_t tsc_to_ns_mul = 0;

inline uint64_t rdtscp() {
  uint32_t lo, hi;
  asm volatile("lfence\n\t"
               "rdtsc\n\t"
               : "=a"(lo), "=d"(hi)
               :
               : "memory");
  return ((uint64_t)hi << 32) | lo;
}

void init() {
  uint16_t divisor = (uint16_t)(PIT_FREQ / TARGET_FREQ);

  ports::outb(PIT_CMD, 0x36); /* Channel 0, lobyte/hibyte, square wave */
  ports::outb(PIT_CH0_DATA, (uint8_t)(divisor & 0xFF));
  ports::outb(PIT_CH0_DATA, (uint8_t)((divisor >> 8) & 0xFF));

  tsc_base = rdtscp();
}

uint64_t get_ticks() { return timer_ticks; }
uint64_t get_ms() { return timer_ticks; } /* 1 tick = 1ms */
uint64_t get_seconds() { return timer_ticks / 1000; }
uint64_t time_ns() {
  uint64_t delta_tsc = rdtscp() - tsc_base;
  return (__uint128_t)delta_tsc * tsc_to_ns_mul >> 32;
}
void calibrate_tsc() {
  uint64_t start_ticks = timer_ticks;

  uint64_t t1 = rdtscp();
  while (timer_ticks - start_ticks < 100) {
    asm volatile("hlt");
  }
  uint64_t t2 = rdtscp();

  uint64_t dt = t2 - t1;
  tsc_per_ms = dt / 100;
  tsc_per_us = tsc_per_ms / 1000;
  tsc_to_ns_mul = (1000000ULL << 32) / tsc_per_ms;

  if (tsc_per_us == 0)
    tsc_per_us = 1;
}

void wait_ms(uint64_t ms) {
  uint64_t target = timer_ticks + ms;
  while (timer_ticks < target) {
    asm volatile("hlt");
  }
}

} // namespace timer

/* ============================================================
 * KEYBOARD IMPLEMENTATION
 * ============================================================ */
namespace keyboard {

void init() {
  /* Keyboard diinisialisasi melalui PIC IRQ 1 */
  /* Flush buffer keyboard hardware */
  while (ports::inb(0x64) & 0x01) {
    ports::inb(0x60);
  }
}

char read_char() {
  while (kb_head == kb_tail) {
    asm volatile("hlt");
  }
  char c = kb_buffer[kb_tail];
  kb_tail = (kb_tail + 1) % KB_BUFFER_SIZE;
  return c;
}

char read_char_nonblocking() {
  if (kb_head == kb_tail)
    return 0;
  char c = kb_buffer[kb_tail];
  kb_tail = (kb_tail + 1) % KB_BUFFER_SIZE;
  return c;
}

bool has_input() { return kb_head != kb_tail; }

void read_line(char *buf, size_t max_len) {
  size_t pos = 0;
  while (pos < max_len - 1) {
    char c = read_char();
    if (c == '\n') {
      Console::putchar('\n');
      break;
    }
    if (c == '\b') {
      if (pos > 0) {
        pos--;
        Console::putchar('\b');
      }
      continue;
    }
    buf[pos++] = c;
    Console::putchar(c);
  }
  buf[pos] = '\0';
}

uint8_t read_scancode_nonblocking() {
  if (sc_head == sc_tail)
    return 0;
  uint8_t sc = scancode_buffer[sc_tail];
  sc_tail = (sc_tail + 1) % 64;
  return sc;
}

bool is_shift_held() { return (kb_modifiers & KB_MOD_SHIFT) != 0; }
bool is_ctrl_held() { return (kb_modifiers & KB_MOD_CTRL) != 0; }

} // namespace keyboard

} // namespace hal
