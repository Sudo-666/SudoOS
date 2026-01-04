#ifndef _SYSCALL_H
#define _SYSCALL_H
#include <stdint.h>

// ============================================================================
//   SudoOS System Call Definitions (Linux x86_64 Compatible)
//   内核实现位置: kernel/src/arch/interrupts.c -> syscall_handler
// ============================================================================

// --- 基础 IO (最优先实现) ---
// 功能: 从文件描述符读取数据
// 参数: rdi=fd, rsi=buf, rdx=count
// 实现: 如果 fd=0 (stdin)，读取键盘缓冲区；如果是文件，调用文件系统读接口
#define SYS_READ    0

// 功能: 向文件描述符写入数据
// 参数: rdi=fd, rsi=buf, rdx=count
// 实现: 如果 fd=1/2 (stdout/stderr)，输出到显卡/串口；如果是文件，写入磁盘
#define SYS_WRITE   1

// 功能: 打开或创建文件
// 参数: rdi=path, rsi=flags, rdx=mode
// 实现: 解析路径，查找 inode，创建 file 结构体，返回新的 fd (整数)
#define SYS_OPEN    2

// 功能: 关闭文件描述符
// 参数: rdi=fd
// 实现: 释放对应的 file 结构体引用计数
#define SYS_CLOSE   3

// --- 文件系统增强 (支持 ls, cp 等命令) ---
// 功能: 获取文件状态 (大小、权限、时间等)
// 参数: rdi=path, rsi=stat_buf
// 实现: 根据路径找到文件，填充 struct stat 结构体返回给用户
#define SYS_STAT    4

// 功能: 获取已打开文件的状态
// 参数: rdi=fd, rsi=stat_buf
// 实现: 同上，但是通过 fd 查找
#define SYS_FSTAT   5

// 功能: 移动文件读写指针
// 参数: rdi=fd, rsi=offset, rdx=whence (SEEK_SET/CUR/END)
// 实现: 修改 file 结构体中的 internal_offset
#define SYS_LSEEK   8

// 功能: 读取目录项 (实现 'ls' 命令的核心)
// 参数: rdi=fd, rsi=dirp (缓冲区), rdx=count
// 实现: 这是一个难点。需要把目录下的文件名列表格式化为 linux_dirent64 结构返回
#define SYS_GETDENTS64 217

// 功能: 获取当前工作目录
// 参数: rdi=buf, rsi=size
// 实现: 返回当前进程 PCB 中的 cwd 路径字符串
#define SYS_GETCWD  79

// 功能: 改变当前工作目录 (实现 'cd' 命令)
// 参数: rdi=path
// 实现: 检查路径是否存在，更新进程 PCB 中的 cwd
#define SYS_CHDIR   80
#define SYS_MKDIR   83  

// --- 内存管理 (核心 - 支撑 malloc) ---
// 功能: 修改数据段大小 (Heap 堆边界)
// 参数: rdi=brk_addr (目标地址)
// 实现: 
//   1. 如果 addr == 0，返回当前堆顶 (current_brk)。
//   2. 如果 addr > current_brk，分配物理页并映射到页表，更新 current_brk。
//   3. 这是 C 库 malloc 的底层支撑。
#define SYS_BRK     12

// 功能: 内存映射 (加载动态库或大文件)
// 参数: rdi=addr, rsi=len, rdx=prot, r10=flags, r8=fd, r9=offset
// 实现: 初期可只实现 "匿名映射" (flags=MAP_ANONYMOUS)，相当于申请一大块内存
#define SYS_MMAP    9

// 功能: 解除内存映射
// 参数: rdi=addr, rsi=len
// 实现: 释放对应的页表映射和物理页
#define SYS_MUNMAP  11

// --- 进程管理 (核心 - 支撑 Shell 运行程序) ---
// 功能: 主动让出 CPU
// 参数: 无
// 实现: 直接调用 schedule()
#define SYS_YIELD   24

// 功能: 创建子进程
// 参数: 无
// 实现: 
//   1. 复制父进程的 PCB。
//   2. 复制页表 (或者实现 Copy-On-Write)。
//   3. 父进程返回子进程 PID，子进程返回 0。
#define SYS_FORK    57

// 功能: 执行新程序
// 参数: rdi=filename, rsi=argv, rdx=envp
// 实现: 
//   1. 读取 ELF 文件。
//   2. 清空当前进程地址空间。
//   3. 加载 ELF 到内存，建立新栈。
//   4. 跳转到 ELF 入口点 (Entry Point)。
#define SYS_EXECVE  59

// 功能: 结束当前进程
// 参数: rdi=exit_code
// 实现: 释放资源，将状态设为 ZOMBIE，唤醒父进程
#define SYS_EXIT    60

// 功能: 等待子进程结束 (回收僵尸进程)
// 参数: rdi=pid, rsi=status, rdx=options, r10=rusage
// 实现: 检查子进程链表，如果有 ZOMBIE 状态的子进程，回收它并返回其 PID
#define SYS_WAIT4   61

// 功能: 获取当前进程 ID
// 参数: 无
// 实现: 返回 current_proc->pid
#define SYS_GETPID  39

// 功能: 获取父进程 ID
// 参数: 无
// 实现: 返回 current_proc->parent->pid
#define SYS_GETPPID 110

// --- 时间与休眠 ---
// 功能: 进程休眠
// 参数: rdi=req (timespec*), rsi=rem
// 实现: 将进程状态设为 SLEEPING，记录唤醒 tick 数，schedule()
#define SYS_NANOSLEEP 35

// 功能: 获取系统时间
// 参数: rdi=tv (timeval*), rsi=tz
// 实现: 读取 RTC 或系统启动后的 tick 数并转换
#define SYS_GETTIMEOFDAY 96


struct linux_dirent64 {
    uint64_t d_ino;
    int64_t  d_off;
    unsigned short d_reclen;
    unsigned char  d_type;
    char           d_name[];
};


// IO
int read(int fd, void *buf, int count);
int write(int fd, const void *buf, int count);
int open(const char *pathname, int flags, int mode);
int close(int fd);

// 文件系统
int getcwd(char *buf, unsigned long size);
int chdir(const char *path);
int getdents64(unsigned int fd, void *dirp, unsigned int count);
int lseek(int fd, long offset, int whence);
int fstat(int fd, void *statbuf);
int stat(const char *filename, void *statbuf);
int mkdir(const char *pathname, int mode);

// 进程
int getpid(void);
int getppid(void);
int fork(void);
int execve(const char *filename, char *const argv[], char *const envp[]);
void exit(int status);
int wait4(int pid, int *status, int options, void *rusage);
int waitpid(int pid, int *status, int options);
void sched_yield(void);

// 内存
void *brk(void *addr);
void *sbrk(intptr_t increment);

// 时间
int nanosleep(const void *req, void *rem);
unsigned int sleep(unsigned int seconds);

#endif