#include "syscall.h"
#include <stdint.h>

// 通用的 syscall 触发器
static inline int64_t syscall(int64_t num, int64_t a1, int64_t a2, int64_t a3) {
    int64_t ret;
    // 内联汇编
    // rax = num, rdi = a1, rsi = a2, rdx = a3
    // int 0x80 触发中断
    // 返回值存入 rax
    __asm__ volatile (
        "int $0x80"
        : "=a" (ret)            // 输出: rax -> ret
        : "a" (num),            // 输入: num -> rax
          "D" (a1),             // a1 -> rdi
          "S" (a2),             // a2 -> rsi
          "d" (a3)              // a3 -> rdx
        : "rcx", "r11", "memory" // Clobbers: 寄存器会被修改
    );
    return ret;
}

void usrprint(const char* str) {
    int len = 0;
    while(str[len]) len++;
    syscall(1, 1, (uint64_t)str, len);
}