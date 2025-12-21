#pragma once
#include "../mm/vmm.h"
#include "../arch/tr"

#define PROCNAME_LEN 32

typedef enum {
    PROC_RUNNING,
    PROC_READY,
    PROC_BLOCKED,
    PROC_ZOMBIE             // 僵尸（已退出但未被父进程回收）
} proc_state_t;



struct pcb_t
{
    uint64_t rsp;          // 栈指针
    uint64_t kstack;       // 内核栈的栈底地址
    struct mm_struct* mm;
    
    int pid;
    proc_state_t proc_state;
    char name[PROCNAME_LEN+1];
    int exit_code;

};




