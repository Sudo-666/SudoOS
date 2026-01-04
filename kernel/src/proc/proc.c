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
  new_pcb->cwd_inode = 0;
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

  // 【修复】检查节点是否连接在链表上再删除
  // 只有当 prev 或 next 不为 NULL (或不指向自己，取决于你的 list 实现) 时才删除
  // 假设你的 list_init 将 prev/next 设为 &node 自身，或者 alloc 时是 0
  // 最安全的做法是检查它是否被初始化过。
  // 由于 alloc_new_pcb 做了 memset 0，如果没加链表，它们是 NULL。
  
  if (proc->proc_list_node.prev != NULL && proc->proc_list_node.next != NULL) {
      list_del(&proc->proc_list_node);
  }

  // 同理检查调度队列节点
  if (proc->sched_node.prev != NULL && proc->sched_node.next != NULL) {
      list_del(&proc->sched_node);
  }

  // 释放内核栈
  if (proc->kstack_base) {
      kstack_free(proc->kstack_base);
  }
  // 释放内存空间（如果有）
  if(proc->mm) {
    mm_free(proc->mm); // 假设有 mmfree 函数
  }
  // 释放 PCB 结构体
  kfree(proc);

}

void user_entry() {
  uint64_t entry_point;
  uint64_t user_stack_top;

  // 从寄存器取出参数 (这些寄存器是 switch_to 从内核栈 ctx 里恢复的)
  asm volatile("mov %%r12, %0" : "=r"(entry_point));
  asm volatile("mov %%r13, %0" : "=r"(user_stack_top));

  // 2. 确保 TSS 栈指针正确 (用于下次从 Ring3 中断回来)
  // 此时 current_proc 已经是我们了
  extern pcb_t* current_proc;
  set_tss_stack(current_proc->kstack_base + KSTACK_SIZE);
  
  kprintf("Jumping to Ring 3...\n");
  enter_user_mode(entry_point, user_stack_top);

  // 不应该运行到这里
  while(1);
}


uint64_t load_elf(pcb_t *proc,const char *elf_data)
{
  /*
  实现逻辑核心：
    解析头部：检查 Magic Number。
    切换页表 (关键步骤)：
    我们即将往用户虚拟地址（如 0x400000）写入数据。
    当前 CPU 使用的是内核页表（或父进程页表），里面并没有映射 0x400000。
    因此，必须临时执行 lcr3(proc->mm->pml4_pa) 切换到目标进程的页表。
    为什么安全？ 因为你的 mm_alloc 已经复制了内核的高半核映射，所以切换后，
    正在执行的内核代码（load_elf 本身）依然存在且地址不变。
    加载段 (Segment)：遍历 Program Headers，找到 PT_LOAD 类型的段，申请内存并拷贝数据。
    恢复页表：操作完成后切回原来的 CR3。
  */
 kprintln("Start Loading ELF ... ");

  Elf64_Ehdr* ehdr = (Elf64_Ehdr*)elf_data;

  // 检查 ELF Magic Number
  if (ehdr->e_ident[0] != 0x7F || ehdr->e_ident[1] != 'E' ||
      ehdr->e_ident[2] != 'L' || ehdr->e_ident[3] != 'F') {
    kprintf("Error: Invalid ELF Magic Number for process PID %d\n", proc->pid);
    kprintf("Magic: %02x %c %c %c\n", 
    ehdr->e_ident[0], ehdr->e_ident[1], ehdr->e_ident[2], ehdr->e_ident[3]);
    return 0;
  }

  // 保存当前 CR3
  uint64_t old_cr3 = rcr3();
  // 切换到目标进程的页表
  lcr3(proc->mm->pml4_pa);

  // 遍历 Program Headers
  Elf64_Phdr* phdr = (Elf64_Phdr*)(elf_data + ehdr->e_phoff);
  for(int i=0;i<ehdr->e_phnum;i++)
  {
    if(phdr[i].p_type == PT_LOAD) {
      // 权限确定 
      uint64_t vm_flags = VM_READ;
      
      // 【修正方案】：在加载阶段，必须给予写权限 (VM_WRITE)
      // 否则 memcpy 无法将数据写入页面。
      // 哪怕它是只读代码段，内核加载它时也需要写。
      vm_flags |= VM_WRITE; 
      if(phdr[i].p_flags & PF_X) vm_flags |= VM_EXEC;

      // 映射内存
      if(!mm_map_range(proc->mm,phdr[i].p_vaddr,phdr[i].p_memsz,vm_flags)) {
        kprintf("Error: Failed to map segment at %lx\n", phdr[i].p_vaddr);
        lcr3(old_cr3); // 恢复 CR3
        return 0;
      }

      // 拷贝数据段内容
      memset((void*)phdr[i].p_vaddr,0,phdr[i].p_memsz); // 先清零
      memcpy((void*)phdr[i].p_vaddr,(void*)(elf_data + phdr[i].p_offset),phdr[i].p_filesz);
    }
  }
  // 恢复原 CR3
  lcr3(old_cr3);
  return ehdr->e_entry; // 返回入口点
}

pcb_t* create_user_process(const char *name, void *elf_data,uint64_t size)
{
  kprintf("Start creating user process %s \n",name);
  // 分配 PCB
  pcb_t *proc = alloc_new_pcb();
  if (proc == NULL)
    return NULL;
  // 设置信息
  set_proc_name(proc, name);

  // 创建内存空间
  proc->mm = mm_alloc();
  if(proc->mm == NULL) {
    kfree(proc);
    return NULL;
  }

  // 加载 ELF 文件
  uint64_t entry_point = load_elf(proc, (const char *)elf_data);
  if (entry_point == 0)
  {
    kprintln("Error: Failed to load ELF");
    // 清理
    if (proc->mm) {
        mm_free(proc->mm);
        proc->mm = NULL; // 防止 free_proc 里再次释放（如果那里也有逻辑的话）
    }
    free_proc(proc);
    return NULL;
  }

  // 设置用户栈
 
  uint64_t old_cr3 = rcr3();
  lcr3(proc->mm->pml4_pa);

  // 映射用户栈
  uint64_t user_stack_base = USER_STACK_TOP - USER_STACK_SIZE;
  if(!mm_map_range(proc->mm,user_stack_base,USER_STACK_SIZE,VM_READ | VM_WRITE| VM_STACK)) {
    kprintln("Error: Failed to map user stack");
    lcr3(old_cr3);
    free_proc(proc);
    return NULL;
  }
  memset((void*)user_stack_base,0,USER_STACK_SIZE); // 清零栈空间

  lcr3(old_cr3);

  // 分配内核栈
  void *kstack_top = kstack_init(KSTACK_SIZE);
  if(!kstack_top) {
    free_proc(proc);
    kprintln("Error: Failed to alloc kernel stack");
    return NULL;
  }
  proc->rsp = (uint64_t)kstack_top; 
  proc->kstack_base = (uint64_t)kstack_top - KSTACK_SIZE;

  // 构造上下文
  proc->rsp -= sizeof(context_t);
  context_t *context = (context_t *)proc->rsp;
  memset(context, 0, sizeof(context_t));

  // 设置 switch_to 返回后的目标地址 -> 跳板函数
  context->rip = (uint64_t)user_entry; // 用户程序入口点
  context->r12 = entry_point; // 传递用户程序入口点
  context->r13 = USER_STACK_TOP; // 传递用户栈顶地址
  proc->context = context;

  // 初始化其他字段
  proc->trap_frame = NULL; // 初始为 NULL，因为还没发生过中断
  proc->proc_state = PROC_READY;

  // 加入调度队列
  list_add_after(&proc->proc_list_node, &proc_list);
  list_add_before(&proc->sched_node, &ready_queue);

  kprintf("Process '%s' created. Entry: %lx, Stack: %lx\n", name, entry_point, USER_STACK_TOP);
  return proc;
  
}

int sys_fork()
{
  pcb_t* parent = current_proc;
  pcb_t* child = alloc_new_pcb();
  child->cwd_inode = parent->cwd_inode;
  if(child == NULL) {
    return -1; // 分配失败
  }

  set_proc_name(child, parent->name);
  child->parent = parent;

  // 复制内存空间
  child->mm = mm_alloc();
  if(!mm_copy(child->mm,parent->mm)) {
    free_proc(child);
    return -1;
  }

  // 分配内核栈
  void *kstack_top = kstack_init(KSTACK_SIZE);
  if(!kstack_top) {
    free_proc(child);
    return -1;
  }
  child->rsp = (uint64_t)kstack_top;
  child->kstack_base = (uint64_t)kstack_top - KSTACK_SIZE;

  // 复制 TrapFrame (中断现场)
  trap_frame_t* parent_tf = parent->trap_frame;

  // 在子进程栈顶预留 TrapFrame 空间
  child->rsp -= sizeof(trap_frame_t);
  trap_frame_t* child_tf = (trap_frame_t*) child->rsp;
  // 拷贝内容
  *child_tf = *parent_tf;

  // 修改子进程返回值：fork 在子进程返回 0
  child_tf->rax = 0;
  child->trap_frame = child_tf;

  // 构造上下文 (Context) 用于 switch_to
  child->rsp -= sizeof(context_t);
  context_t* context = (context_t*) child->rsp;
  
  // 子进程被调度时，直接“返回”到用户态。
  // 一种简单的做法是：让 switch_to 返回到一个“跳板函数”，
  // 跳板函数里执行 iretq (使用栈顶的 child_tf)。
  extern void fork_ret_entry(); // 需要在汇编实现
  context->rip = (uint64_t)fork_ret_entry;
  child->context = context;

  // 加入调度
  child->proc_state = PROC_READY;
  list_add_after(&child->proc_list_node, &proc_list);
  list_add_before(&child->sched_node, &ready_queue);

  return child->pid; // 父进程返回子进程 PID

}

void init_userproc(struct limine_file* init_file)
{
  pcb_t* init_proc = create_user_process("init", init_file->address, init_file->size);
  if (!init_proc) {
    kprintln("Failed to create init process!");
    while (1) {
      __asm__ volatile ("hlt");
    }
  }
  
}