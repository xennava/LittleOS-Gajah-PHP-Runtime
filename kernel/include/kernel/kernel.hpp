/*
 * LittleOS Gajah PHP - kernel.hpp
 * Definisi tipe dasar dan makro kernel
 * Kernel murni PHP 8 Runtime — bukan UNIX, bukan Linux, bukan DOS
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

/* ============================================================
 * KONSTANTA UKURAN
 * ============================================================ */
#define PAGE_SIZE 4096ULL
#define KILOBYTE 1024ULL
#define MEGABYTE (1024ULL * KILOBYTE)
#define GIGABYTE (1024ULL * MEGABYTE)

/* ============================================================
 * MAKRO UTILITAS
 * ============================================================ */
#define ALIGN_UP(v, a) (((v) + (a) - 1) & ~((a) - 1))
#define ALIGN_DOWN(v, a) ((v) & ~((a) - 1))
#define IS_ALIGNED(v, a) (((v) & ((a) - 1)) == 0)
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))
#define UNUSED(x) (void)(x)

/* ============================================================
 * ATRIBUT COMPILER
 * ============================================================ */
#define PACKED __attribute__((packed))
#define NORETURN __attribute__((noreturn))
#define ALIGNED(n) __attribute__((aligned(n)))
#define USED_ATTR __attribute__((used))

/* ============================================================
 * C++ PLACEMENT NEW (diperlukan tanpa libstdc++)
 * ============================================================ */
inline void *operator new(size_t, void *p) noexcept { return p; }
inline void *operator new[](size_t, void *p) noexcept { return p; }
inline void operator delete(void *, void *) noexcept {}
inline void operator delete[](void *, void *) noexcept {}

/* ============================================================
 * FUNGSI MEMORI DASAR (extern "C" agar kompatibel)
 * ============================================================ */
// extern "C" {
//     void* memcpy(void* dest, const void* src, size_t n);
//     void* memset(void* dest, int val, size_t n);
//     void* memmove(void* dest, const void* src, size_t n);
//     int   memcmp(const void* a, const void* b, size_t n);
// }

struct framebuffer_t {
  void *addr;
  uint64_t width, height, pitch;
};
