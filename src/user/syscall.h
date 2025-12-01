#pragma once
#include <stdint.h>

// 1号调用: 写 (Write)
static inline void sys_write(const char* str) {
    __asm__ volatile (
        "mov $1, %%rax \n" // rax = 1
        "mov %0, %%rbx \n" // rbx = str
        "int $0x80     \n" 
        : : "r"(str) : "rax", "rbx"
    );
}

// 2号调用: 读 (Read)
static inline void sys_read(char* buffer, int max_len) {
    __asm__ volatile (
        "mov $2, %%rax \n"      // 系统调用号 2
        "mov %0, %%rbx \n"      // 参数1: buffer
        "mov %1, %%rcx \n"      // 参数2: len
        "int $0x80     \n"      // 触发中断
        : 
        : "r"(buffer), "r"((uint64_t)max_len)  // <--- 关键修改：加了 (uint64_t) 强转
        : "rax", "rbx", "rcx", "memory"
    );
}