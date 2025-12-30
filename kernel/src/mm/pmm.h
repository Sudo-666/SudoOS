#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "../limine.h"
#include "../lib/string.h"
#include "../lib/list.h"
#include "paging.h"

//****************************************** */
//**                内核LAYOUT               */
//****************************************** */
// 1. 直接映射区 (HHDM Area)
// 用于通过偏移量直接访问所有物理内存。Limine 默认通常映射在此处。
// 预留空间：取决于物理内存总量（通常可支持到 TB 级）。
#define KERNEL_HHDM_BASE         0xFFFF800000000000

// 2. 内核堆区 (Kernel Heap)
// 用于 kmalloc 动态分配。给堆预留 512GB 甚至更多，完全不用担心够不够用。
#define KERNEL_HEAP_BASE         0xFFFF900000000000

// 3. Vmalloc / MMIO 区
// 用于映射硬件寄存器（如显存、APIC）或分配不连续的物理页。
#define KERNEL_MMIO_BASE         0xFFFFA00000000000

// 4. 内核栈区 (Kernel Stacks)
// 每个进程/线程的内核栈起始位置。
// 建议每个栈 16KB + 4KB 保护页，这里预留数 GB 空间可支持海量线程。
#define KERNEL_STACK_BASE        0xFFFFB00000000000

// 5. 内核镜像区 (Kernel Image)
// 存放链接脚本定义的 .text, .rodata, .data, .bss。
// 必须在 0xFFFFFFFF80000000，这是 -mcmodel=kernel 的硬性要求。
#define KERNEL_IMAGE_BASE        0xFFFFFFFF80000000

// 设置页面大小
#define PAGE_SIZE 4096
#define KSTACK_SIZE PAGE_SIZE * 4 // 每个内核栈大小：16KB

// 向上取整：(x + 4095) & ~4095
#define ALIGN_UP(addr, align)   (((addr) + (align) - 1) & ~((align) - 1))

// 向下取整：x & ~4095
#define ALIGN_DOWN(addr, align) ((addr) & ~((align) - 1))

#define pa2pgidx(pa) (pa/PAGE_SIZE)

#define pgidx2pa(pgidx) ( (uint64_t)(pgidx)*PAGE_SIZE)

// hhdm偏移量
extern uint64_t HHDM_OFFSET;

// bitmap-pmm管理器
// 全局变量
extern uint8_t* bitmap;             // 位图数组的虚拟地址
extern size_t bitmap_size;          // 位图占用的字节数
extern size_t total_pages;          // 物理内存总页数
extern size_t free_pages;           // 当前空闲页数（统计用）

// 对bitmap的位操作
extern bool bit_test(size_t bit);

// 这是一个优化变量，下次分配时从这里开始扫描，避免每次都从头扫
extern size_t last_free_index;

// 初始化bitmap
void pmm_init(struct limine_memmap_response* mmap);

void pmm_set_free(uintptr_t pa,size_t pgnum);
void pmm_set_busy(uintptr_t pa,size_t pgnum);

// 内存分配接口

/**
 * @brief next_fit算法分配空闲物理页框
 * 
 * @return uint64_t 
 */
uint64_t pmm_alloc_page();


/**
 * @brief 释放物理地址pa对应的物理页框
 * 
 * @param pa 
 */
void pmm_free_page(uint64_t pa);



/**
 * @brief 内核堆内存块头
 * 
 */
typedef struct {
    list_node_t node;
    uint64_t size; // 空闲区大小
    bool is_free;
} kheap_pghdr_t;


#define HEADER_SIZE sizeof(kheap_pghdr_t)
#define MIN_SPLIT 16

bool kheap_expand(size_t pgnum);

void kheap_init(size_t init_pages);

/**
 * @brief 首次适配算法分配内核堆内存块
 * @param size 内存块大小
 * @return void* 内存块指针
 */
void* kmalloc(size_t size);

void kfree(void* ptr);

//************************************************** */
//*************        kstack             ********** */
//************************************************** */

extern uint64_t kstack_ptr;

/**
 * @brief 分配内核栈
 * @param size 栈大小
 * @return void* 栈顶地址
 */
void* kstack_init(size_t size);

void kstack_free(uintptr_t kstack_base);