#pragma once
#include "../arch/trap.h"
#include "../drivers/console.h"
#include "../mm/vmm.h"
#define PROCNAME_LEN 32
#define KSTACK_SIZE 0x4000    // 16KB


typedef enum {
  PROC_RUNNING,
  PROC_READY,
  PROC_BLOCKED,
  PROC_ZOMBIE // 僵尸（已退出但未被父进程回收）
} proc_state_t;

typedef struct pcb_t {
  // ===  硬件上下文与栈 ===
  uint64_t rsp;         // 栈指针
  uint64_t kstack_base; // 内核栈的栈底地址

  context_t *context;
  trap_frame_t *trap_frame;

  // ===  内存空间 ===
  struct mm_struct *mm;

  list_node_t proc_list_node;
  list_node_t sched_node; // 调度队列节点


  // === 状态信息 ===
  uint64_t total_runtime; // 运行时间
  int time_slice;         // 时间片剩余时间

  int pid;
  struct pcb_t *parent;
  proc_state_t proc_state;
  char name[PROCNAME_LEN + 1];
  int exit_code; // 退出码

} pcb_t;

void proc_init();

void set_proc_name(pcb_t *proc, const char *name);

int get_next_pid();

/**
 * @brief 分配一个新的 PCB，并初始化其字段
 * @return pcb_t* 指向新分配的 PCB 结构体
 */
pcb_t *alloc_new_pcb();

/**
 * @brief 创建一个内核线程
 * @param parent 父进程 PCB 指针
 * @param name 线程名称
 * @param kthread_func 线程函数入口
 * @param arg 传递给线程函数的参数
 * @return pcb_t* 指向新创建的内核线程的 PCB 结构体
 */
pcb_t *kthread_create(pcb_t *parent, const char *name,
                      void (*kthread_func)(void *), void *arg);

void kthread_exit(int exit_code);

/**
 * @brief 释放一个 PCB 及其相关资源
 * @param proc 指向要释放的 PCB 结构体
 */
void free_proc(pcb_t *proc);