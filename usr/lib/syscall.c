#include "syscall.h"
#include <stdint.h>

// ============================================================================
// 1. 底层接口：适配 SudoOS 的 int 0x80 中断体系
// ============================================================================

/**
 * @brief 核心系统调用触发函数 (Int 0x80 版本)
 * * 对应内核 syscall_handler 的接收规则：
 * - RAX: 系统调用号
 * - RDI: 第1个参数
 * - RSI: 第2个参数
 * - RDX: 第3个参数
 * - R10: 第4个参数 (预留，保持 ABI 通用性)
 * - R8 : 第5个参数
 * * 触发方式：int $0x80 (中断号 128)
 */
static inline int64_t syscall(uint64_t n, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5) {
    int64_t ret;
    
    // 将参数放入指定寄存器
    // 注意：虽然 int 0x80 不像 syscall 指令那样破坏 rcx/r11，
    // 但为了代码的健壮性和 ABI 一致性，我们依然使用 r10 传递第4个参数。
    register uint64_t r10 __asm__("r10") = a4;
    register uint64_t r8  __asm__("r8")  = a5;

    __asm__ volatile (
        "int $0x80"             // <--- 关键修改：使用中断触发，而不是 syscall 指令
        : "=a"(ret)             // 输出：RAX (返回值)
        : "a"(n),               // 输入：RAX (调用号)
          "D"(a1),              // RDI (参数1)
          "S"(a2),              // RSI (参数2)
          "d"(a3),              // RDX (参数3)
          "r"(r10),             // R10 (参数4)
          "r"(r8)               // R8  (参数5)
        : "memory"              // 内存屏障，防止编译器乱序
    );
    return ret;
}

// 辅助宏：简化调用参数
#define SYSCALL0(n)             syscall(n, 0, 0, 0, 0, 0)
#define SYSCALL1(n, a1)         syscall(n, (uint64_t)a1, 0, 0, 0, 0)
#define SYSCALL2(n, a1, a2)     syscall(n, (uint64_t)a1, (uint64_t)a2, 0, 0, 0)
#define SYSCALL3(n, a1, a2, a3) syscall(n, (uint64_t)a1, (uint64_t)a2, (uint64_t)a3, 0, 0)
#define SYSCALL4(n, a1, a2, a3, a4) syscall(n, (uint64_t)a1, (uint64_t)a2, (uint64_t)a3, (uint64_t)a4, 0)

// ============================================================================
// 2. 基础 IO 函数封装
// ============================================================================

int read(int fd, void *buf, int count) {
    return (int)SYSCALL3(SYS_READ, fd, buf, count);
}

int write(int fd, const void *buf, int count) {
    return (int)SYSCALL3(SYS_WRITE, fd, buf, count);
}

int open(const char *pathname, int flags, int mode) {
    return (int)SYSCALL3(SYS_OPEN, pathname, flags, mode);
}

int close(int fd) {
    return (int)SYSCALL1(SYS_CLOSE, fd);
}

// ============================================================================
// 3. 文件系统增强
// ============================================================================

int getcwd(char *buf, unsigned long size) {
    return (int)SYSCALL2(SYS_GETCWD, buf, size);
}

int chdir(const char *path) {
    return (int)SYSCALL1(SYS_CHDIR, path);
}

int getdents64(unsigned int fd, void *dirp, unsigned int count) {
    return (int)SYSCALL3(SYS_GETDENTS64, fd, dirp, count);
}

int lseek(int fd, long offset, int whence) {
    return (int)SYSCALL3(SYS_LSEEK, fd, offset, whence);
}

int fstat(int fd, void *statbuf) {
    return (int)SYSCALL2(SYS_FSTAT, fd, statbuf);
}

int stat(const char *filename, void *statbuf) {
    return (int)SYSCALL2(SYS_STAT, filename, statbuf);
}

// ============================================================================
// 4. 进程管理
// ============================================================================

int getpid() {
    return (int)SYSCALL0(SYS_GETPID);
}

int getppid() {
    return (int)SYSCALL0(SYS_GETPPID);
}

int fork() {
    return (int)SYSCALL0(SYS_FORK);
}

int execve(const char *filename, char *const argv[], char *const envp[]) {
    return (int)SYSCALL3(SYS_EXECVE, filename, argv, envp);
}

void exit(int status) {
    SYSCALL1(SYS_EXIT, status);
    while(1) { __asm__ volatile("hlt"); } // 应该永远不会运行到这
}

int wait4(int pid, int *status, int options, void *rusage) {
    return (int)SYSCALL4(SYS_WAIT4, pid, status, options, rusage);
}

int waitpid(int pid, int *status, int options) {
    return wait4(pid, status, options, 0);
}

void sched_yield() {
    SYSCALL0(SYS_YIELD);
}

// ============================================================================
// 5. 内存管理
// ============================================================================

void *brk(void *addr) {
    return (void *)SYSCALL1(SYS_BRK, addr);
}

void *sbrk(intptr_t increment) {
    void *current_brk = brk(0);
    if (current_brk == (void *)-1) return (void *)-1;

    if (increment == 0) return current_brk;

    void *new_brk = (void *)((char *)current_brk + increment);
    void *ret = brk(new_brk);

    if ((char *)ret < (char *)new_brk) return (void *)-1;

    return current_brk;
}

// ============================================================================
// 6. 时间函数
// ============================================================================

int nanosleep(const void *req, void *rem) {
    return (int)SYSCALL2(SYS_NANOSLEEP, req, rem);
}

unsigned int sleep(unsigned int seconds) {
    long req[2] = { (long)seconds, 0 }; 
    long rem[2] = { 0, 0 };
    nanosleep(req, rem);
    return rem[0]; // 返回剩余时间
}