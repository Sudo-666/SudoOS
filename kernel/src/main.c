#include "limine.h"
#include "drivers/drivers.h"
#include "lib/std.h"
#include "arch/idt.h"
#include "arch/gdt.h"
#include "mm/debug_mm.h"
#include "mm/pmm.h"
#include "mm/paging.h"
#include "arch/x86_64.h"
#include "proc/proc.h"
#include "arch/timer.h"
#include "fs/ramfs.h"

extern pg_table_t *kernel_pml4;

/**
 * @brief 声明limine版本号
 *
 */
__attribute__((used, section(".limine_requests"))) 
static volatile uint64_t limine_base_revision[] = LIMINE_BASE_REVISION(4);

/**
 * @brief 请求framebuffer显存
 *
 */
__attribute__((used, section(".limine_requests"))) 
static volatile struct limine_framebuffer_request framebuffer_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST_ID,
    .revision = 0
};


/**
 * @brief 请求memmap，获取物理内存探测表
 * 
 */
__attribute__((used,section(".limine_requests")))
static volatile struct limine_memmap_request memmap_request = {
    .id = LIMINE_MEMMAP_REQUEST_ID,
    .revision = 0
};


/**
 * @brief 请求HHDM，获取 pa->va 的偏移量
 * 
 */
__attribute__((used,section(".limine_requests")))
static volatile struct limine_hhdm_request hhdm_request = {
    .id = LIMINE_HHDM_REQUEST_ID,
    .revision = 0
};

// 
__attribute__((used, section(".limine_requests")))
static volatile struct limine_module_request module_request = {
    .id = LIMINE_MODULE_REQUEST_ID,
    .revision = 0
};

/**
 * @brief  声明limine请求区头尾，使得limine在内核运行前处理请求区的所有请求
 *
 */

__attribute__((used, section(".limine_requests_start"))) 
static volatile uint64_t limine_requests_start_marker[] = LIMINE_REQUESTS_START_MARKER;

__attribute__((used, section(".limine_requests_end"))) 
static volatile uint64_t limine_requests_end_marker[] = LIMINE_REQUESTS_END_MARKER;

/**
 * @brief  让 CPU 完全停止
 * 
 */
static void hcf(void)
{
    for(;;) {
        asm("hlt");
    }
}

#define MSR_EFER 0xC0000080
#define EFER_NXE (1 << 11)
void enable_nx() {
    uint64_t efer = rdmsr(MSR_EFER);
    // 开启 No-Execute Enable 位
    wrmsr(MSR_EFER, efer | EFER_NXE);
}

void mm_init() {
    kprintln("Initializing Memory Management Subsystem...");
    // 获取memmap
    struct limine_memmap_response *mmap = memmap_request.response;
    // 获取hhdm_response
    if (hhdm_request.response == NULL) {
        hcf();
    }
    struct limine_hhdm_response *hhdm_res = hhdm_request.response;
    HHDM_OFFSET = hhdm_res->offset;
    // 初始化控制台

    debug_memmap(mmap);
    kprintf("HHDM OFFSET: %lx\n",HHDM_OFFSET);
    pmm_init(mmap);
    paging_init(mmap);
    kheap_init(4);

    // 内核栈相关
    void* ksptr = kstack_init(4*PAGE_SIZE);
    if (ksptr == NULL) hcf();

    set_tss_stack((uint64_t)ksptr);
    kprintf("Switching stack to %lx\n", ksptr);

    return;
}

void kernel_init() {
    // 确保版本正确
    if (LIMINE_BASE_REVISION_SUPPORTED(limine_base_revision) == false) {
        hcf();
    }

    // 确保获取到显存
    if (framebuffer_request.response == NULL
     || framebuffer_request.response->framebuffer_count < 1) {
        hcf();
    }

    // 初始化终端
    // 获取到第一个显存信息
    struct limine_framebuffer *framebuffer = framebuffer_request.response->framebuffers[0];
    console_init(framebuffer);
    // 初始化 GDT 和 IDT
    kprintln("Initializing GDT and IDT...");
    gdt_init();
    kprintln("GDT initialized.");
    idt_init();
    kprintln("IDT initialized.");
    init_keyboard();

    //在 CPU 的 EFER 寄存器中启用 NX 功能 (EFER.NXE)。
    enable_nx();
    // 初始化内存管理子系统
    mm_init();
    kprintln("Switched to new kernel stack done!");

    proc_init();
    // 开启时钟中断，启动调度
    init_timer(20);

    ramfs_init(0,0);
}


void thread_a(void* arg) {
    for(int i=1;i<=10000;i++) {
        kprintf("A");
        for(int j=0;j<1000000;j++); // 简单延时
    }
}

void thread_b(void* arg) {
    for(int i=1;i<=10000;i++) {
        kprintf("B");
        for(int j=0;j<1000000;j++); // 简单延时
    }
}

void debug_proc() {
    extern pcb_t* current_proc;
    extern list_node_t proc_list;
    extern list_node_t ready_queue;
    kprintf("Current Process PID: %d, Name: %s\n", current_proc->pid, current_proc->name);
    kprintln("All Processes:\n");
    list_node_t* node = proc_list.next;
    while(node != &proc_list) {
        pcb_t* proc = container_of(node, pcb_t, proc_list_node);
        kprintf("PID: %d, Name: %s, State: %d\n", proc->pid, proc->name, proc->proc_state);
        node = node->next;
    }
    kprintln("Ready Queue:\n");
    node = ready_queue.next;
    while(node != &ready_queue) {
        pcb_t* proc = container_of(node, pcb_t, sched_node);
        kprintf("PID: %d, Name: %s\n", proc->pid, proc->name);
        node = node->next;
    }
}

/**
 * @brief 入口
 * 
 */
void kmain(void) {
    
    kernel_init();
    
    struct limine_file* init_file = module_request.response->modules[0];
    
    // 创建第一个进程
    init_userproc(init_file);
    __asm__ volatile ("sti");

    schedule(); 

    // 如果 schedule() 返回了，说明没有其他进程可运行，或者被抢占回来了
    kprintln("Back in kmain loop");
    while (1) {
        __asm__ volatile ("hlt");
    }
    hcf();
 
}