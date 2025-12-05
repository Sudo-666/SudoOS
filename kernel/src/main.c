#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "limine.h"
#include "drivers/console.h"
#include "drivers/keyboard.h"
#include "lib/string.h"


/**
 * @brief 声明limine版本号
 *
 */
__attribute__((used, section(".limine_requests"))) static volatile uint64_t limine_base_revision[] = LIMINE_BASE_REVISION(4);

/**
 * @brief 请求framebuffer显存
 *
 */

__attribute__((used, section(".limine_requests"))) static volatile struct limine_framebuffer_request framebuffer_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST_ID,
    .revision = 0
};


/**
 * @brief 请求memmap，获取物理内存探测表
 * 
 */
__attribute__((section(".limine_requests")))
static volatile struct limine_memmap_request memmap_request = {
    .id = LIMINE_MEMMAP_REQUEST_ID,
    .revision = 0
};



/**
 * @brief  声明limine请求区头尾，使得limine在内核运行前处理请求区的所有请求
 *
 */

__attribute__((used, section(".limine_requests_start"))) static volatile uint64_t limine_requests_start_marker[] = LIMINE_REQUESTS_START_MARKER;

__attribute__((used, section(".limine_requests_end"))) static volatile uint64_t limine_requests_end_marker[] = LIMINE_REQUESTS_END_MARKER;

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


const char* memtype2str(uint64_t type) {
    switch (type)
    {
    case LIMINE_MEMMAP_USABLE:
    return "USABLE";
        break;
    case LIMINE_MEMMAP_RESERVED:
    return "RESERVED";
        break;
    case LIMINE_MEMMAP_ACPI_RECLAIMABLE:
    return "ACPI_RECLAIMABLE";
        break;
    case LIMINE_MEMMAP_ACPI_NVS:
    return "ACPI_NVS";
        break;
    case LIMINE_MEMMAP_BAD_MEMORY:
    return "BAD_MEMORY";
        break;
    case LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE:
    return "BOOTLOADER_RECLAIMABLE";
        break;
    case LIMINE_MEMMAP_EXECUTABLE_AND_MODULES:
    return "EXECUTABLE_AND_MODULES";
        break;
    case LIMINE_MEMMAP_FRAMEBUFFER:
    return "MEMMAP_FRAMEBUFFER";
        break;
    case LIMINE_MEMMAP_ACPI_TABLES:
    return "ACPI_TABLES";
        break;
    }
    return "UNKNOWN";
}


/**
 * @brief 
 * 
 * @param mmap 
 */
void debug_memmap(struct limine_memmap_response *mmap) {
    if (!mmap) {
        kprintln("Memmap request failed!");
        return;
    }

    kprintln("=== LIMINE MEMORY MAP ===");
    uint64_t i = 0;
    for(;i<mmap->entry_count;i++) {
        struct limine_memmap_entry *e = mmap->entries[i];

        char idx_buf[32];
        char base_buf[32];
        char len_buf[32];

        kprint("Entry ");
        kprint(itoa((int)i,idx_buf,10));
        kprint(":");
        kprint(" base=");
        kprint(itoa((int)e->base, base_buf, 16));
        kprint(" size=");
        kprint(itoa((int)e->length, len_buf, 10));
        kprint(" type=");
        kprintln(memtype2str(e->type));
    }
}


/**
 * @brief 入口
 * 
 */
void kmain(void) {
    
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

    // 初始化控制台
    console_init(framebuffer);
    
    // 获取memmap
    struct limine_memmap_response *mmap = memmap_request.response;

    debug_memmap(mmap);


    hcf();

 
}