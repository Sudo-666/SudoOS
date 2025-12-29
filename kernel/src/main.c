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

    // 初始化内存管理子系统
    mm_init();
    kprintln("Switched to new kernel stack done!");

    proc_init();
}


void thread_a(void* arg) {
    while(1) {
        kprintf("A");
        // 延时，否则屏幕刷太快看不清
        for(int i=0; i<1000000; i++) __asm__("pause");
    }
}

void thread_b(void* arg) {
    while(1) {
        kprintf("B");
        for(int i=0; i<1000000; i++) __asm__("pause");
    }
}


/**
 * @brief 入口
 * 
 */



void kmain(void) {
    
    kernel_init();
    // // 2. 获取模块响应
    // struct limine_module_response *module_response = module_request.response;
    // if (module_response == NULL || module_response->module_count < 1) {
    //     kprintln("Error: No modules loaded! Check limine.cfg");
    //     hcf();
    // }
    // struct limine_file *user_file = module_response->modules[0]; // 获取第一个模块
    // kprintf("Module found at: %lx, Size: %ld\n", user_file->address, user_file->size);

    // kprintln("Preparing to run usrmain in User Mode...");

    // // 3. 映射并拷贝用户代码
    // uint64_t user_code_va = 0x1000000;

    // size_t file_pages = (user_file->size + PAGE_SIZE - 1) / PAGE_SIZE;
    
    // for(size_t i=0; i < file_pages; i++) {
    //     uint64_t pg_pa = pmm_alloc_page();
        
    //     // 映射
    //     vmm_map_page(kernel_pml4, user_code_va + i*PAGE_SIZE, pg_pa, PTE_PRESENT | PTE_RW | PTE_USER);
        
    //     // 拷贝 (通过 HHDM 计算目标地址)
    //     extern uint64_t HHDM_OFFSET;
    //     void* dst = (void*)(pg_pa + HHDM_OFFSET);
        
    //     // 计算本页拷贝大小
    //     size_t cp_len = PAGE_SIZE;
    //     if(i == file_pages -1) cp_len = user_file->size % PAGE_SIZE;
    //     if(cp_len == 0) cp_len = PAGE_SIZE;
        
    //     // 从模块地址拷贝 (Limine HHDM地址可以直接读)
    //     memcpy(dst, (void*)(user_file->address + i*PAGE_SIZE), cp_len);
    // }
    
    // kprintln("User program loaded at 0x1000000");

    // // 4. 映射栈 (保持不变)
    // uint64_t user_stack_va = 0x80000000; 
    // uint64_t stack_pa = pmm_alloc_page();
    // vmm_map_page(kernel_pml4, user_stack_va, stack_pa, PTE_PRESENT | PTE_RW | PTE_USER);

    // // 5. 跳转
    // kprintln("Switching to Ring 3...");
    // enter_user_mode(user_code_va, user_stack_va + PAGE_SIZE);

    // // 如果成功，代码不会运行到这里
    // kprintln("ERROR: Switch failed!");
    init_timer(20); // 设置频率为 20Hz (每秒切换20次)
    extern pcb_t* current_proc;
    kthread_create(current_proc,"thread_a",thread_a, NULL);
    kthread_create(current_proc,"thread_b",thread_b, NULL);

    hcf();
 
}