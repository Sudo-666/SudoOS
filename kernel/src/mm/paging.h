#pragma once

#include <stdint.h>
#include "pmm.h"

// 获取内核逻辑空间地址
extern char __kernel_start[];
extern char __kernel_end[];

typedef uint64_t pte_t;
typedef uint64_t pde_t;
typedef uint64_t pdpte_t;
typedef uint64_t pml4e_t;

#define PAGE_SIZE 4096
// ==========================================
// 页表项标志位 (Page Table Entry Flags)
// ==========================================

// Bit 0: 存在位 (Present)
// 1 = 映射有效，0 = 无效 (访问触发 #PF 缺页异常)
#define PTE_PRESENT   (1ull << 0)

// Bit 1: 读写权限 (Read/Write)
// 1 = 可读可写，0 = 只读
#define PTE_RW        (1ull << 1)

// Bit 2: 用户/内核权限 (User/Supervisor)
// 1 = 用户态(Ring3)可访问，0 = 仅内核态(Ring0)可访问
#define PTE_USER      (1ull << 2)

// Bit 3: 写入通透 (Page-Level Write-Through)
// 控制缓存策略，通常设为 0 (Write-Back)
#define PTE_PWT       (1ull << 3)

// Bit 4: 缓存禁止 (Page-Level Cache Disable)
// 1 = 禁止缓存该页 (用于 MMIO)，0 = 允许缓存 (用于内存)
#define PTE_PCD       (1ull << 4)

// Bit 5: 已访问 (Accessed)
// CPU 会在访问该页后自动置 1 (软件可读，用于页面置换算法)
#define PTE_ACCESSED  (1ull << 5)

// Bit 6: 已修改 (Dirty) - 仅在最后一级 PT 有效
// CPU 会在写入该页后自动置 1
#define PTE_DIRTY     (1ull << 6)

// Bit 7: 大页标志 (Page Size) - 仅在 PDPT 或 PD 有效
// 1 = 这是个大页 (2MB 或 1GB)，不再指向下一级页表
// 0 = 这是个普通目录，指向下一级页表
#define PTE_HUGE      (1ull << 7)

// Bit 8: 全局页 (Global)
// 1 = 刷新 TLB (CR3 切换) 时不清除此页 (用于内核共享区域)
// 需要在 CR4 寄存器开启 PGE 位才生效
#define PTE_GLOBAL    (1ull << 8)

// Bit 63: 禁止执行 (No Execute)
// 1 = 禁止在此页取指令 (防止缓冲区溢出攻击)
// 需要在 EFER MSR 寄存器开启 NXE 位才生效，否则触发 #GP
#define PTE_NX        (1ull << 63)


// ==========================================
//  辅助宏 / Helper Functions
// ==========================================

// 掩码：用于从 PTE 中提取物理地址
// x86_64 物理地址实际有效位通常是 12~51 位 (0x000FFFFFFFFFF000)
#define PTE_ADDR_MASK   0x000FFFFFFFFFF000

// 宏：从页表项中获取物理地址
#define PTE_GET_ADDR(pte) ((pte) & PTE_ADDR_MASK)

// 宏：从页表项中获取标志位
#define PTE_GET_FLAGS(pte) ((pte) & ~PTE_ADDR_MASK)

// 宏：从虚拟地址中提取各级索引 (9位一组)

// PML4 Index: Bits 39-47
#define PML4_IDX(va)  (((va) >> 39) & 0x1FF)
// PDPT Index: Bits 30-38
#define PDPT_IDX(va)  (((va) >> 30) & 0x1FF)
// PD Index:   Bits 21-29
#define PD_IDX(va)    (((va) >> 21) & 0x1FF)
// PT Index:   Bits 12-20
#define PT_IDX(va)    (((va) >> 12) & 0x1FF)
// offset：    Bits 0-11
#define OFFSET(va) (va & 0xFFF)

// 一页4096B，一个指针64b=8B，一页共4096/8=512项页表项
typedef struct {
    pte_t entries[512];
}__attribute__((aligned(PAGE_SIZE))) pg_table_t;

/**
 * @brief 获取下一级页表指针。如果 allocate=true 且不存在，则创建之。
 * 
 * @param pgtable 
 * @param index 
 * @param allocate 
 * @return pg_table_t* 
 */
pg_table_t* get_next_table(pg_table_t* pgtable, uint64_t index, bool allocate,uint64_t flags);


/**
 * @brief 映射虚拟地址到物理地址
 * @param pml4 顶级页表虚拟地址
 * @param virt 虚拟地址
 * @param phys 物理地址
 * @param flags 标志位 (PTE_RW, PTE_USER 等)
 */
void vmm_map_page(pg_table_t* pml4, uintptr_t va, uintptr_t pa, uint64_t flags);

void paging_init(struct limine_memmap_response* mmap);

