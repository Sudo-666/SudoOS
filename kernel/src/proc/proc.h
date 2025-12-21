#pragma once
#include "../mm/vmm.h"

#define PROCNAME_LEN 32

typedef enum {
    PROC_RUNNING,
    PROC_READY,
    PROC_BLOCKED,
    PROC_ZOMBIE             // 僵尸（已退出但未被父进程回收）
} proc_state_t;


struct context {
    uint64_t r15;
    uint64_t r14;
    uint64_t r13;
    uint64_t r12;
    uint64_t rbp;
    uint64_t rbx;

    // === 返回地址 ===
    // 执行 call switch_to 时，CPU 自动压入的返回地址 (RIP)
    // 切换回来后，ret 指令会弹出这个值并跳转
    uint64_t rip;
};

struct pcb_t
{
    uint64_t rsp;          // 栈指针
    uint64_t kstack;       // 内核栈的栈底地址
    struct mm_struct* mm;
    
    int pid;
    proc_state_t proc_state;
    char name[PROCNAME_LEN];
    int exit_code;

};




