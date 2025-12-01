#pragma once
#include <stdint.h>

// 寄存器快照 (按照压栈顺序定义)
// 注意：x86_64 的栈是向下生长的，所以最后压入的在结构体最前面
struct TrapFrame {
    // 1. 我们手动 push 的通用寄存器
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rbp, rdi, rsi, rdx, rcx, rbx, rax;

    // 2. 中断号和错误码 (由汇编宏 push)
    uint64_t trap_num;   // 中断号 (0-255)
    uint64_t error_code; // 错误码 (有的异常 CPU 自动压，有的我们需要手动补0)

    // 3. CPU 自动压入的上下文 (iretq 恢复时用)
    uint64_t rip;        // 程序计数器
    uint64_t cs;         // 代码段
    uint64_t rflags;     // 标志寄存器
    uint64_t rsp;        // 栈指针 (用户态切进来时才有)
    uint64_t ss;         // 栈段 (用户态切进来时才有)
} __attribute__((packed));