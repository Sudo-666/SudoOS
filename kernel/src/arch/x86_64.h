#pragma once
#ifndef __LIBS_X86_H__
#define __LIBS_X86_H__

#include <stdint.h>

// 告诉编译器，不要把这行代码前面的内存访问指令优化（重排）到这行代码后面。
#define barrier() __asm__ __volatile__ ("" ::: "memory")

/* I/O 端口操作通常不需要改变，因为端口地址空间依然是 16 位的 */
static inline uint8_t inb(uint16_t port) __attribute__((always_inline));
static inline uint16_t inw(uint16_t port) __attribute__((always_inline));
static inline uint32_t inl(uint16_t port) __attribute__((always_inline));
static inline void insl(uint32_t port, void *addr, int cnt) __attribute__((always_inline));
static inline void outb(uint16_t port, uint8_t data) __attribute__((always_inline));
static inline void outw(uint16_t port, uint16_t data) __attribute__((always_inline));
static inline void outl(uint16_t port, uint32_t data) __attribute__((always_inline));
static inline void outsl(uint32_t port, const void *addr, int cnt) __attribute__((always_inline));

/* 寄存器操作：EBP -> RBP (栈帧基址) */
static inline uint64_t read_rbp(void) __attribute__((always_inline));

static inline void breakpoint(void) __attribute__((always_inline));

/* 调试寄存器：DR0-DR7 在 64 位模式下是 64 位的 */
static inline uint64_t read_dr(unsigned regnum) __attribute__((always_inline));
static inline void write_dr(unsigned regnum, uint64_t value) __attribute__((always_inline));

/* 描述符结构：Base 变成了 64 位 (uintptr_t 自动适配) */
struct pseudodesc {
    uint16_t pd_lim;        // Limit
    uintptr_t pd_base;      // Base address
} __attribute__ ((packed));

static inline void lidt(struct pseudodesc *pd) __attribute__((always_inline));
static inline void lgdt(struct pseudodesc *pd) __attribute__((always_inline)); // 补充常用
static inline void sti(void) __attribute__((always_inline));
static inline void cli(void) __attribute__((always_inline));
static inline void ltr(uint16_t sel) __attribute__((always_inline));

/* 标志位寄存器：EFLAGS (32位) -> RFLAGS (64位) */
static inline uint64_t read_rflags(void) __attribute__((always_inline));
static inline void write_rflags(uint64_t rflags) __attribute__((always_inline));

/* 控制寄存器：CR0-CR3 变成 64 位 */
static inline void lcr0(uintptr_t cr0) __attribute__((always_inline));
static inline void lcr3(uintptr_t cr3) __attribute__((always_inline));
static inline uintptr_t rcr0(void) __attribute__((always_inline));
static inline uintptr_t rcr2(void) __attribute__((always_inline));
static inline uintptr_t rcr3(void) __attribute__((always_inline));

static inline void invlpg(void *addr) __attribute__((always_inline));

/* 字符串操作优化 */
static inline int __strcmp(const char *s1, const char *s2) __attribute__((always_inline));
static inline char *__strcpy(char *dst, const char *src) __attribute__((always_inline));
static inline void *__memset(void *s, char c, size_t n) __attribute__((always_inline));
static inline void *__memcpy(void *dst, const void *src, size_t n) __attribute__((always_inline));
static inline void *__memmove(void *dst, const void *src, size_t n) __attribute__((always_inline));





/**
 * @brief 从指定端口读取 1 个字 (16-bit)。
 * 
 * @param port 
 * @return uint16_t 
 */
static inline uint16_t inw(uint16_t port) {
    uint16_t data;
    asm volatile ("inw %1, %0" : "=a" (data) : "d" (port));
    return data;
}

/**
 * @brief 向端口读 4 个字节 (32-bit)。
 * 
 * @param port 
 * @return uint32_t 
 */
static inline uint32_t inl(uint16_t port) {
    uint32_t data;
    asm volatile ("inl %1, %0" : "=a" (data) : "d" (port));
    return data;
}

/**
 * @brief 从端口批量读取 cnt 个 32 位双字（double word）到内存 addr 中。
 * 
 * @param port 
 * @param addr 
 * @param cnt 
 */
static inline void insl(uint32_t port, void *addr, int cnt) {
    asm volatile (
        "cld;"
        "repne; insl;"
        : "=D" (addr), "=c" (cnt)
        : "d" (port), "0" (addr), "1" (cnt)
        : "memory", "cc");
}



/**
 * @brief 向指定端口写入 1 个字。
 * 
 * @param port 
 * @param data 
 */
static inline void outw(uint16_t port, uint16_t data) {
    asm volatile ("outw %0, %1" :: "a" (data), "d" (port));
}

/**
 * @brief 从端口读取 4 个字节 (32-bit)。
 * 
 * @param port 
 * @param data 
 */
static inline void outl(uint16_t port, uint32_t data) {
    asm volatile ("outl %0, %1" :: "a" (data), "d" (port));
}

/**
 * @brief 将内存 addr 中的 cnt 个 32 位双字批量写入端口。
 * 
 * @param port 
 * @param addr 
 * @param cnt 
 */
static inline void outsl(uint32_t port, const void *addr, int cnt) {
    asm volatile (
        "cld;"
        "repne; outsl;"
        : "=S" (addr), "=c" (cnt)
        : "d" (port), "0" (addr), "1" (cnt)
        : "memory", "cc");
}

/* 修改：EBP -> RBP, 类型改为 uint64_t */

/**
 * @brief 读取当前的栈帧基址指针 (Frame Pointer)。
 * 
 * @return uint64_t 
 */
static inline uint64_t read_rbp(void) {
    uint64_t rbp;
    asm volatile ("movq %%rbp, %0" : "=r" (rbp));
    return rbp;
}


/**
 * @brief 触发软件断点。
 * 
 */
static inline void breakpoint(void) {
    asm volatile ("int $3");
}

/**
 * @brief 读取/写入调试寄存器 (Debug Registers, DR0-DR7)。
 * 
 * @param regnum 
 * @return uint64_t 
 */
static inline uint64_t read_dr(unsigned regnum) {
    uint64_t value = 0;
    switch (regnum) {
    case 0: asm volatile ("movq %%db0, %0" : "=r" (value)); break;
    case 1: asm volatile ("movq %%db1, %0" : "=r" (value)); break;
    case 2: asm volatile ("movq %%db2, %0" : "=r" (value)); break;
    case 3: asm volatile ("movq %%db3, %0" : "=r" (value)); break;
    case 6: asm volatile ("movq %%db6, %0" : "=r" (value)); break;
    case 7: asm volatile ("movq %%db7, %0" : "=r" (value)); break;
    }
    return value;
}

static void write_dr(unsigned regnum, uint64_t value) {
    switch (regnum) {
    case 0: asm volatile ("movq %0, %%db0" :: "r" (value)); break;
    case 1: asm volatile ("movq %0, %%db1" :: "r" (value)); break;
    case 2: asm volatile ("movq %0, %%db2" :: "r" (value)); break;
    case 3: asm volatile ("movq %0, %%db3" :: "r" (value)); break;
    case 6: asm volatile ("movq %0, %%db6" :: "r" (value)); break;
    case 7: asm volatile ("movq %0, %%db7" :: "r" (value)); break;
    }
}

/**
 * @brief 加载中断描述符表寄存器 (IDTR)。
 * 
 * @param pd 
 */
static inline void lidt(struct pseudodesc *pd) {
    asm volatile ("lidt (%0)" :: "r" (pd) : "memory");
}

/**
 * @brief 加载 GDTR (全局描述符表寄存器)。
 * 
 * @param pd 
 */
static inline void lgdt(struct pseudodesc *pd) {
    asm volatile ("lgdt (%0)" :: "r" (pd) : "memory");
}

/**
 * @brief 开启中断 (Set Interrupt Flag)。允许 CPU 响应硬件中断。
 * 
 */
static inline void sti(void) {
    asm volatile ("sti");
}

/**
 * @brief 关闭中断 (Clear Interrupt Flag)。禁止 CPU 响应硬件中断
 * 
 */
static inline void cli(void) {
    asm volatile ("cli" ::: "memory");
}

/**
 * @brief 加载任务寄存器 (Load Task Register)。
 * 
 * @param sel 
 */
static inline void ltr(uint16_t sel) {
    asm volatile ("ltr %0" :: "r" (sel) : "memory");
}

/**
 * @brief 读 RFLAGS (标志寄存器)。
 * 
 * @return uint64_t 
 */
static inline uint64_t read_rflags(void) {
    uint64_t rflags;
    asm volatile ("pushfq; popq %0" : "=r" (rflags));
    return rflags;
}

/**
 * @brief 写 RFLAGS (标志寄存器)。
 * 
 * @param rflags 
 */
static inline void write_rflags(uint64_t rflags) {
    asm volatile ("pushq %0; popfq" :: "r" (rflags));
}

/**
 * @brief 加载 CR0。
 * 
 * @param cr0 
 */
static inline void lcr0(uintptr_t cr0) {
    asm volatile ("mov %0, %%cr0" :: "r" (cr0) : "memory");
}

/**
 * @brief 加载 CR3。
 * 
 * @param cr3 
 */
static inline void lcr3(uintptr_t cr3) {
    asm volatile ("mov %0, %%cr3" :: "r" (cr3) : "memory");
}

/**
 * @brief 读取 CR0。
 * 
 * @return uintptr_t 
 */
static inline uintptr_t rcr0(void) {
    uintptr_t cr0;
    asm volatile ("mov %%cr0, %0" : "=r" (cr0) :: "memory");
    return cr0;
}

/**
 * @brief 读取 CR2。当发生 Page Fault (#PF) 时，CPU 会把引起错误的那个虚拟地址自动填入 CR2。
 * 
 * @return uintptr_t 
 */
static inline uintptr_t rcr2(void) {
    uintptr_t cr2;
    asm volatile ("mov %%cr2, %0" : "=r" (cr2) :: "memory");
    return cr2;
}

/**
 * @brief 读取 CR3。
 * 
 * @return uintptr_t 
 */
static inline uintptr_t rcr3(void) {
    uintptr_t cr3;
    asm volatile ("mov %%cr3, %0" : "=r" (cr3) :: "memory");
    return cr3;
}

/**
 * @brief 刷新 TLB (Translation Lookaside Buffer)。
 * 
 * @param addr 
 */
static inline void invlpg(void *addr) {
    asm volatile ("invlpg (%0)" :: "r" (addr) : "memory");
}

/* 字符串函数优化 */

#ifndef __HAVE_ARCH_STRCMP
#define __HAVE_ARCH_STRCMP
static inline int __strcmp(const char *s1, const char *s2) {
    int d0, d1, ret;
    asm volatile (
        "1: lodsb;"
        "scasb;"
        "jne 2f;"
        "testb %%al, %%al;"
        "jne 1b;"
        "xorl %%eax, %%eax;"
        "jmp 3f;"
        "2: sbbl %%eax, %%eax;"
        "orb $1, %%al;"
        "3:"
        : "=a" (ret), "=&S" (d0), "=&D" (d1)
        : "1" (s1), "2" (s2)
        : "memory");
    return ret;
}
#endif

#ifndef __HAVE_ARCH_STRCPY
#define __HAVE_ARCH_STRCPY
static inline char * __strcpy(char *dst, const char *src) {
    int d0, d1, d2;
    asm volatile (
        "1: lodsb;"
        "stosb;"
        "testb %%al, %%al;"
        "jne 1b;"
        : "=&S" (d0), "=&D" (d1), "=&a" (d2)
        : "0" (src), "1" (dst) : "memory");
    return dst;
}
#endif

#ifndef __HAVE_ARCH_MEMSET
#define __HAVE_ARCH_MEMSET
static inline void * __memset(void *s, char c, size_t n) {
    long d0, d1;
    asm volatile (
        "rep; stosb;"
        : "=&c" (d0), "=&D" (d1)
        : "0" (n), "a" (c), "1" (s)
        : "memory");
    return s;
}
#endif

#ifndef __HAVE_ARCH_MEMMOVE
#define __HAVE_ARCH_MEMMOVE
static inline void * __memmove(void *dst, const void *src, size_t n) {
    if (dst < src) {
        return __memcpy(dst, src, n);
    }
    long d0, d1, d2;
    asm volatile (
        "std;"
        "rep; movsb;" // 64位下内存通常很大，简单用 movsb 即可，或者手动优化为 movsq
        "cld;"
        : "=&c" (d0), "=&S" (d1), "=&D" (d2)
        : "0" (n), "1" (n - 1 + (const char *)src), "2" (n - 1 + (char *)dst)
        : "memory");
    return dst;
}
#endif

#ifndef __HAVE_ARCH_MEMCPY
#define __HAVE_ARCH_MEMCPY
/* 修改：使用 movsq (8字节) 进行拷贝 */
static inline void * __memcpy(void *dst, const void *src, size_t n) {
    long d0, d1, d2;
    asm volatile (
        "rep; movsq;"        /* 每次搬运 8 字节 */
        "movq %4, %%rcx;"    /* 恢复剩下的字节数 */
        "andq $7, %%rcx;"    /* 剩下的字节数 (n % 8) */
        "jz 1f;"
        "rep; movsb;"        /* 搬运剩余字节 */
        "1:"
        : "=&c" (d0), "=&D" (d1), "=&S" (d2)
        : "0" (n / 8), "g" (n), "1" (dst), "2" (src)
        : "memory");
    return dst;
}
#endif

#endif /* !__LIBS_X86_H__ */

static inline void enter_user_mode(uint64_t entry_point, uint64_t user_stack) {
   asm volatile(
    "cli \n\t"
    "mov $0x1B, %%ax \n\t"    // User Data Selector (0x18 | 3)
    "mov %%ax, %%ds \n\t"
    "mov %%ax, %%es \n\t"
    "mov %%ax, %%fs \n\t"
    "mov %%ax, %%gs \n\t"

    "pushq $0x1B \n\t"        // SS
    "pushq %0 \n\t"           // RSP (Operand %0)
    "pushfq \n\t"             // RFLAGS
    "popq %%rax \n\t"
    "orq $0x200, %%rax \n\t"  // Enable Interrupts flag in RFLAGS
    "pushq %%rax \n\t"
    "pushq $0x23 \n\t"        // CS (User Code Selector 0x20 | 3)
    "pushq %1 \n\t"           // RIP (Operand %1)
    "iretq \n\t"
    : : "r"(user_stack), "r"(entry_point) : "rax", "memory"
);
}