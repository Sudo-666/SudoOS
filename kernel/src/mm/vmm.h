#pragma once
#include <stdint.h>
#include "../lib/list.h"
#include "paging.h"

// VMA 权限标志
#define VM_READ     (1 << 0)
#define VM_WRITE    (1 << 1)
#define VM_EXEC     (1 << 2)
#define VM_SHARED   (1 << 3) // 是否多进程共享
#define VM_STACK    (1 << 4) // 栈（通常向下生长）
#define VM_HEAP     (1 << 5) // 堆


struct mm_struct;

struct vma_struct {
    list_node_t list_node; 
    struct mm_struct * mm; // 指向所属的地址空间
    uint64_t vm_start;
    uint64_t vm_end;
    uint64_t vm_flags;
};


// 代表了一个进程完整的虚拟地址空间。
struct mm_struct {
    pg_table_t* pml4;       // 该进程的顶级页表指针
    list_node_t vma_list;   // 串联了该进程拥有的所有 VMA (虚拟内存区域)。
    int map_count;          // vma的数量
    int ref_count;          // 记录有多少个PCB正在引用这个mm
    struct vma_struct* mmap_cache; // 最近一次成功查找到的那个 VMA 结构体。

    uint64_t start_code, end_code; // 代码段边界
    uint64_t start_data, end_data; // 数据段边界
    uint64_t start_heap, heap;       // 堆边界
    uint64_t start_stack;          // 栈起始位置
};

/**
 * 虚拟地址空间 (mm_struct 描述)
    +-----------------------+ <--- 0xFFFFFF...
    |                       |
    |      Kernel Space     | (高半核，所有 mm 共享)
    |                       |
    +-----------------------+
    |      User Stack       | <--- start_stack (向下增长)
    |          |            |
    |          v            | (自动扩展)
    |                       |
    +-----------------------+ 
    |          ^            |
    |          |            | (通过 brk() 系统调用向上增长)
    |      User Heap        | <--- brk (当前堆顶)
    |                       | <--- start_brk
    +-----------------------+
    |      User Data        | <--- start_data / end_data
    +-----------------------+
    |      User Text        | <--- start_code / end_code
    +-----------------------+ <--- 0x000000... 
 * 
 */

// 代表了地址空间中一段连续的区域（如代码段、栈、堆）
