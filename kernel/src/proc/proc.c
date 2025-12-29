#include "proc.h"

list_node_t proc_list;
pcb_t *current_proc = NULL;
list_node_t ready_queue;
int next_pid = 0;

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
  set_proc_name(idle, "idle");
  idle->proc_state = PROC_RUNNING;
  current_proc = idle;
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
  proc->pid = get_next_pid();
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
  context->rip = (uint64_t)kthread_func;
  // 设置第一个参数（rdi）为 arg
  context->rdi = (uint64_t)arg;

  // 将进程加入就绪队列
  list_add_after(&ready_queue, &proc->proc_list_node);
  kprintf("Kernel thread '%s' (PID %d) created.", name, proc->pid);
  return proc;
}
