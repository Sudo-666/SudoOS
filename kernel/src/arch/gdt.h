#pragma once
#include <stdint.h>

// 64位 GDT 条目
struct gdt_entry {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_middle;
    uint8_t  access;
    uint8_t  granularity;
    uint8_t  base_high;
} __attribute__((packed));

// 64位 TSS 结构体 (Task State Segment)
// 只有 RSP0 是我们最关心的，用来存内核栈顶地址
struct tss_entry {
    uint32_t reserved0;
    uint64_t rsp0;       // 中断时切换到这个栈
    uint64_t rsp1;
    uint64_t rsp2;
    uint64_t reserved1;
    uint64_t ist[7];     // 中断栈表
    uint64_t reserved2;
    uint16_t reserved3;
    uint16_t iomap_base;
} __attribute__((packed));

// 加载到 GDTR 寄存器的指针
struct gdt_ptr {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

void gdt_init();
void set_tss_stack(uint64_t kernel_stack);