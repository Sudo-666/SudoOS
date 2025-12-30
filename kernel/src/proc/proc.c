#include "proc.h"
#include "../arch/x86_64.h"
#include "../arch/switch.h"
#include "sche.h"

list_node_t proc_list;
pcb_t *current_proc = NULL;
pcb_t *idle_proc = NULL;
list_node_t ready_queue;
int next_pid = 0;

extern void kernel_thread_entry();

int get_next_pid() { return next_pid++; }

void set_proc_name(pcb_t *proc, const char *name) {
  strncpy(proc->name, name, PROCNAME_LEN);
  proc->name[PROCNAME_LEN] = '\0'; // 确保以空结尾
}

pcb_t *alloc_new_pcb() {
  pcb_t *new_pcb = (pcb_t *)kmalloc(sizeof(pcb_t));
  memset(new_pcb, 0, sizeof(pcb_t));
  new_pcb->pid = get_next_pid();
  new_pcb->time_slice = TIME_SLICE_DEFAULT;
  new_pcb->rsp = 0;
  new_pcb->kstack_base = 0;
  new_pcb->context = NULL;
  new_pcb->trap_frame = NULL;
  new_pcb->mm = NULL;
  new_pcb->parent = NULL;
  new_pcb->proc_state = PROC_READY;
  new_pcb->exit_code = 0;
  new_pcb->total_runtime = 0;
  return new_pcb;
}

void proc_init() {
  kprintln(" === Initializing process management === ");
  list_init(&proc_list);
  list_init(&ready_queue);

  pcb_t *idle = alloc_new_pcb();
  kprintf("Idle PCB address: %lx\n", (uint64_t)idle);
  if (idle == NULL) {
    kprintln("Panic: kmalloc returned NULL for idle process!");
    // 死循环挂起，方便查看日志
    for(;;) asm("hlt");
  }
  set_proc_name(idle, "idle");
  idle->proc_state = PROC_RUNNING;
  current_proc = idle;
  list_add_after(&idle->proc_list_node, &proc_list);
  idle_proc = idle;
  kprintln("Idle process (PID 0) created.");
}

pcb_t *kthread_create(pcb_t *parent, const char *name,
                      void (*kthread_func)(void *), void *arg) {
  // 分配 PCB
  pcb_t *proc = alloc_new_pcb();
  if (proc == NULL)
    return NULL;

  // 设置信息
  set_proc_name(proc, name);
  proc->parent = parent;
  proc->proc_state = PROC_READY;
  proc->mm = parent->mm; // 共享内存空间

  // 分配内核栈
  void *kstack_top = kstack_init(KSTACK_SIZE);
  if (kstack_top == NULL) {
    kfree(proc);
    return NULL;
  }
  proc->rsp = (uint64_t)kstack_top;
  proc->kstack_base = (uint64_t)kstack_top - KSTACK_SIZE;

  // 初始化上下文
  context_t *context = (context_t *)(proc->rsp - sizeof(context_t));
  memset(context, 0, sizeof(context_t));
  proc->context = context;
  proc->rsp = (uint64_t)context; // 更新 rsp 指向 context

  // 设置返回地址为线程函数入口
  context->rip = (uint64_t)kernel_thread_entry;
  // 设置第一个参数（rdi）为 arg
  context->rdi = (uint64_t)arg;
  // 将线程函数地址放在 rbx 寄存器中，供 kernel_thread_entry 使用
  context->rbx = (uint64_t)kthread_func;

  // 将进程加入就绪队列
  list_add_after(&proc->proc_list_node, &proc_list);
  list_add_before(&proc->sched_node, &ready_queue);
  kprintf("Kernel thread '%s' (PID %d) created.\n", name, proc->pid);
  return proc;
}

void kthread_exit(int exit_code) {
  cli(); // 关中断
  pcb_t* proc = current_proc;
  // 标记为僵尸状态
  proc->proc_state = PROC_ZOMBIE;
  // 设置退出码
  proc->exit_code = exit_code;
  kprintf("Thread (PID %d) exited with code %d.\n", current_proc->pid, exit_code);
  
  schedule(); // 切换到其他进程
  // 永远不会返回这里
}

void free_proc(pcb_t * proc) 
{
  if(proc == NULL) return;
  if(proc->proc_state != PROC_ZOMBIE) {
    kprintf("Warning: free_proc called on non-zombie process PID %d\n", proc->pid);
  }

  // 从进程列表中移除
  list_del(&proc->proc_list_node);

  // 从就绪队列中移除（如果在队列中）
  if(proc->sched_node.next != &proc->sched_node) {
    list_del(&proc->sched_node);
  }

  // 释放内核栈
  kstack_free(proc->kstack_base);
  // 释放内存空间（如果有）
  if(proc->mm) {
    // mmfree(proc->mm); // 假设有 mmfree 函数
  }
  // 释放 PCB 结构体
  kfree(proc);

}