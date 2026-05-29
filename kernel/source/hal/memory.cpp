/*
 * LittleOS Gajah PHP - memory.cpp
 * Manajemen memori fisik (bitmap) dan heap (linked-list first-fit)
 * C++ untuk komunikasi langsung dengan hardware memori
 */

#include "hal.hpp"
#include "kernel.hpp"
#include <string.h>

namespace hal {
namespace memory {

/* ============================================================
 * BITMAP ALLOCATOR — memori fisik
 * ============================================================ */
#define MAX_PAGES (512 * 1024) /* Maks 2GB RAM dengan 4KB pages */
#define BITMAP_SIZE (MAX_PAGES / 8)

static uint8_t page_bitmap[BITMAP_SIZE];
static uint64_t total_memory = 0;
static uint64_t usable_memory = 0;
static uint64_t used_pages = 0;
static uint64_t total_pages = 0;
static uint64_t hhdm = 0;

static void bitmap_set(uint64_t page) {
  page_bitmap[page / 8] |= (1 << (page % 8));
}

static void bitmap_clear(uint64_t page) {
  page_bitmap[page / 8] &= ~(1 << (page % 8));
}

static bool bitmap_test(uint64_t page) {
  return (page_bitmap[page / 8] & (1 << (page % 8))) != 0;
}

void init(struct limine_memmap_response *memmap, uint64_t hhdm_offset) {
  hhdm = hhdm_offset;

  /* Tandai semua halaman sebagai terpakai */
  memset(page_bitmap, 0xFF, BITMAP_SIZE);

  if (!memmap)
    return;

  /* Proses setiap entry dari memory map Limine */
  for (uint64_t i = 0; i < memmap->entry_count; i++) {
    struct limine_memmap_entry *entry = memmap->entries[i];
    total_memory += entry->length;

    if (entry->type == LIMINE_MEMMAP_USABLE) {
      usable_memory += entry->length;

      /* Bebaskan halaman yang bisa dipakai */
      uint64_t base = ALIGN_UP(entry->base, PAGE_SIZE);
      uint64_t end = ALIGN_DOWN(entry->base + entry->length, PAGE_SIZE);

      for (uint64_t addr = base; addr < end; addr += PAGE_SIZE) {
        uint64_t page = addr / PAGE_SIZE;
        if (page < MAX_PAGES) {
          bitmap_clear(page);
          total_pages++;
        }
      }
    }
  }
}

void *alloc_page() {
  for (uint64_t i = 0; i < MAX_PAGES; i++) {
    if (!bitmap_test(i)) {
      bitmap_set(i);
      used_pages++;
      uint64_t phys = i * PAGE_SIZE;
      void *virt = (void *)(phys + hhdm);
      memset(virt, 0, PAGE_SIZE);
      return virt;
    }
  }
  return nullptr; /* Kehabisan memori */
}

void free_page(void *addr) {
  uint64_t virt = (uint64_t)addr;
  uint64_t phys = virt - hhdm;
  uint64_t page = phys / PAGE_SIZE;
  if (page < MAX_PAGES && bitmap_test(page)) {
    bitmap_clear(page);
    if (used_pages > 0)
      used_pages--;
  }
}

/* Alokasi N halaman berurutan (contiguous) */
static void *alloc_pages_contiguous(size_t count) {
  for (uint64_t start = 0; start + count <= (uint64_t)MAX_PAGES; start++) {
    bool found = true;
    for (uint64_t j = 0; j < count; j++) {
      if (bitmap_test(start + j)) {
        start += j; /* Loncat ke setelah halaman yang terpakai */
        found = false;
        break;
      }
    }
    if (found) {
      for (uint64_t j = 0; j < count; j++) {
        bitmap_set(start + j);
        used_pages++;
      }
      uint64_t phys = start * PAGE_SIZE;
      void *virt = (void *)(phys + hhdm);
      memset(virt, 0, count * PAGE_SIZE);
      return virt;
    }
  }
  return nullptr;
}

uint64_t get_total() { return total_memory; }
uint64_t get_free() { return (total_pages - used_pages) * PAGE_SIZE; }
uint64_t get_used() { return used_pages * PAGE_SIZE; }

/* ============================================================
 * HEAP ALLOCATOR — first-fit linked list
 * Digunakan oleh PHP Runtime untuk alokasi dinamis
 * ============================================================ */
struct HeapBlock {
  size_t size;
  bool free;
  HeapBlock *next;
  HeapBlock *prev;
};

static const size_t HEAP_INITIAL_PAGES = 2048; /* 2048 * 4KB = 8MB heap */
static HeapBlock *heap_start = nullptr;
static size_t heap_total = 0;

/* Alignment untuk semua alokasi */
static const size_t HEAP_ALIGN = 16;
static const size_t BLOCK_HEADER_SIZE = ALIGN_UP(sizeof(HeapBlock), HEAP_ALIGN);

void heap_init() {
  /* Alokasikan halaman berurutan sekaligus untuk heap */
  uint8_t *base = (uint8_t *)alloc_pages_contiguous(HEAP_INITIAL_PAGES);
  if (!base)
    return;

  heap_total = HEAP_INITIAL_PAGES * PAGE_SIZE;

  /* Inisialisasi block pertama */
  heap_start = (HeapBlock *)base;
  heap_start->size = heap_total - BLOCK_HEADER_SIZE;
  heap_start->free = true;
  heap_start->next = nullptr;
  heap_start->prev = nullptr;
}

/* Cari block yang cukup besar (first-fit) */
static HeapBlock *find_free_block(size_t size) {
  HeapBlock *block = heap_start;
  while (block) {
    if (block->free && block->size >= size) {
      return block;
    }
    block = block->next;
  }
  return nullptr;
}

/* Pecah block jika terlalu besar */
static void split_block(HeapBlock *block, size_t size) {
  if (block->size >= size + BLOCK_HEADER_SIZE + HEAP_ALIGN) {
    HeapBlock *new_block =
        (HeapBlock *)((uint8_t *)block + BLOCK_HEADER_SIZE + size);
    new_block->size = block->size - size - BLOCK_HEADER_SIZE;
    new_block->free = true;
    new_block->next = block->next;
    new_block->prev = block;

    if (block->next)
      block->next->prev = new_block;
    block->next = new_block;
    block->size = size;
  }
}

/* Gabungkan block yang bersebelahan dan sama-sama free */
static void merge_blocks(HeapBlock *block) {
  /* Gabung dengan block sesudahnya */
  if (block->prev && block->prev->free) {
    block->prev->size += BLOCK_HEADER_SIZE + block->size;
    block->prev->next = block->next;
    if (block->next)
      block->next->prev = block->prev;
    block = block->prev;
  }
  /* Gabung dengan block sebelumnya */
  if (block->next && block->next->free) {
    block->size += BLOCK_HEADER_SIZE + block->next->size;
    block->next = block->next->next;
    if (block->next)
      block->next->prev = block;
  }
}

void *kmalloc(size_t size) {
  if (size == 0)
    return nullptr;

  /* Align ukuran */
  size = ALIGN_UP(size, HEAP_ALIGN);

  HeapBlock *block = find_free_block(size);
  if (!block)
    return nullptr; /* Kehabisan memori heap */

  split_block(block, size);
  block->free = false;

  return (void *)((uint8_t *)block + BLOCK_HEADER_SIZE);
}

void kfree(void *ptr) {
  if (!ptr)
    return;

  HeapBlock *block = (HeapBlock *)((uint8_t *)ptr - BLOCK_HEADER_SIZE);
  block->free = true;

  merge_blocks(block);
}

void *kcalloc(size_t count, size_t size) {
  size_t total = count * size;
  void *ptr = kmalloc(total);
  if (ptr)
    memset(ptr, 0, total);
  return ptr;
}

void *krealloc(void *ptr, size_t new_size) {
  if (!ptr)
    return kmalloc(new_size);
  if (new_size == 0) {
    kfree(ptr);
    return nullptr;
  }

  HeapBlock *block = (HeapBlock *)((uint8_t *)ptr - BLOCK_HEADER_SIZE);
  if (block->size >= new_size)
    return ptr; /* Cukup besar */

  /* Alokasi baru, salin, bebaskan yang lama */
  void *new_ptr = kmalloc(new_size);
  if (new_ptr) {
    memcpy(new_ptr, ptr, block->size);
    kfree(ptr);
  }
  return new_ptr;
}

} // namespace memory
} // namespace hal
