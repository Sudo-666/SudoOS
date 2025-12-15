#pragma once
#include <stdint.h>

typedef struct {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rbp, rdi, rsi, rdx, rcx, rbx, rax;
    uint64_t int_no;
    uint64_t err_code;
    uint64_t rip, cs, rflags, rsp, ss;
} __attribute__((packed)) registers_t;

// 定义中断处理函数指针类型
typedef void (*isr_t)(registers_t*);

// 初始化函数原型
void idt_init();
void register_interrupt_handler(uint8_t n, isr_t handler);
