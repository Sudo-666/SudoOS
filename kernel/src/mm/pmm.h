#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "../limine.h"
#include "../lib/string.h"

// 设置页面大小
#define PAGE_SIZE 4096

// 向上取整：(x + 4095) & ~4095
#define ALIGN_UP(addr, align)   (((addr) + (align) - 1) & ~((align) - 1))

// 向下取整：x & ~4095
#define ALIGN_DOWN(addr, align) ((addr) & ~((align) - 1))

#define pa2pg_idx(pa) (pa/PAGE_SIZE)

// hhdm偏移量
extern uint64_t HHDM_OFFSET;

// bitmap-pmm管理器
// 全局变量
static uint8_t* bitmap = NULL;      // 位图数组的虚拟地址
static size_t bitmap_size = 0;      // 位图占用的字节数
static size_t total_pages = 0;      // 物理内存总页数
static size_t free_pages = 0;       // 当前空闲页数（统计用）


static inline void bit_set(size_t bit);

static inline void bit_unset(size_t bit);

static inline bool bit_test(size_t bit);




// 这是一个优化变量，下次分配时从这里开始扫描，避免每次都从头扫
static size_t last_free_index = 0;

// 初始化bitmap
void pmm_init(struct limine_memmap_response* mmap);

void pmm_set_free(uintptr_t pa,size_t pgnum);

void pmm_set_busy(uintptr_t pa,size_t pgnum);

