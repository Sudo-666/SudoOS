#include "limine.h"
#include "drivers/drivers.h"
#include "lib/std.h"
#include "../../usr/usrTest.c"
#include "arch/idt.h"
#include "arch/gdt.h"
#include "mm/debug_mm.h"
#include "mm/pmm.h"
#include "mm/paging.h"


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


/**
 * @brief 入口
 * 
 */

uint8_t kernel_stack[16384];

void kmain(void) {
    
    gdt_init();

    set_tss_stack((uint64_t)kernel_stack + sizeof(kernel_stack));

    idt_init();

    // 确保版本正确
    if (LIMINE_BASE_REVISION_SUPPORTED(limine_base_revision) == false) {
        hcf();
    }

    // 确保获取到显存
    if (framebuffer_request.response == NULL
     || framebuffer_request.response->framebuffer_count < 1) {
        hcf();
    }

    // 获取到第一个显存信息
    struct limine_framebuffer *framebuffer = framebuffer_request.response->framebuffers[0];
    // 获取memmap
    struct limine_memmap_response *mmap = memmap_request.response;
    // 获取hhdm_response
    if (hhdm_request.response == NULL) {
        hcf();
    }
    struct limine_hhdm_response *hhdm_res = hhdm_request.response;
    HHDM_OFFSET = hhdm_res->offset;
    // 初始化控制台
    console_init(framebuffer);
    debug_memmap(mmap);
    kprintf("HHDM OFFSET: %lx\n",HHDM_OFFSET);
    pmm_init(mmap);
    paging_init(mmap);
    kheap_init(4);
    void* ksptr = kstack_init(4*PAGE_SIZE);
    if (ksptr == NULL) hcf();

    set_tss_stack((uint64_t)ksptr);

    kprintf("Switching stack to %lx\n", ksptr);

    // 只需要切换栈指针
    asm volatile (
        "mov %0, %%rsp \n\t"  // 更新栈指针
        "xor %%rbp, %%rbp \n\t" // 清空栈帧指针（可选，为了调试好看）
        : : "r" (ksptr) : "memory"
    );

    // 此时你应该能看到这句话了
    kprintf("Stack switched successfully!\n");

    hcf();

 
}