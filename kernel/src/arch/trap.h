#pragma once
#include <stdint.h>

typedef struct {
    // 1. Pushed by your assembly `common_stub`
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rbp, rdi, rsi, rdx, rcx, rbx, rax;

    // 2. Interrupt info (pushed by CPU or stub)
    uint64_t int_no;
    uint64_t err_code;

    // 3. Pushed by CPU automatically
    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
    uint64_t rsp;
    uint64_t ss;
} __attribute__((packed)) trap_frame_t;

typedef struct {
    uint64_t r15;
    uint64_t r14;
    uint64_t r13;
    uint64_t r12;
    uint64_t rbx;
    uint64_t rbp;
    uint64_t rip; // 返回地址
} context_t;