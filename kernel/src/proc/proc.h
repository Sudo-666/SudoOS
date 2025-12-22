#pragma once
#include "../mm/vmm.h"
#include "../arch/trap.h"

#define PROCNAME_LEN 32

typedef enum {
    PROC_RUNNING,
    PROC_READY,
    PROC_BLOCKED,
    PROC_ZOMBIE             // 僵尸（已退出但未被父进程回收）
} proc_state_t;


struct pcb_t
{
    // ===  硬件上下文与栈 ===
    uint64_t rsp;                    // 栈指针
    uint64_t kstack;                 // 内核栈的栈底地址
    context_t * context;
    trap_frame_t* trap_frame;

    // ===  内存空间 ===
    struct mm_struct* mm;

    list_node_t proc_list_node;

    // === 状态信息 ===
    uint64_t total_runtime;          // 运行时间


    int pid;
    struct pcb_t *parent;
    proc_state_t proc_state;
    char name[PROCNAME_LEN+1];
    int exit_code;                   // 退出码

};




