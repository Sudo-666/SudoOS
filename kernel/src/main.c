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
    uint64_t totalmm = 0;
    uint64_t usablemm = 0;
    uint64_t unusablemm = 0;
    for(;i<mmap->entry_count;i++) {
        struct limine_memmap_entry *e = mmap->entries[i];
        totalmm+=e->length;
        if(e->type == LIMINE_MEMMAP_USABLE) {
            usablemm+=e->length;
        }
        else {
            unusablemm+=e->length;
        }

        char idx_buf[32];
        char base_buf[64];
        char len_buf[64];

        kprint("Entry ");
        kprint_uint64(i);
        kprint(":");
        kprint(" base=");
        kprint(itoa_uint64(e->base, base_buf, 16));
        kprint(" size=");
        kprint(itoa_uint64(e->length, len_buf, 10));
        kprint(" type=");
        kprintln(memtype2str(e->type));
    }
    char buf[64];
    kprint("total memory: ");
    kprintln(itoa_uint64(totalmm,buf,10));
    kprint("usable memory: ");
    kprintln(itoa_uint64(usablemm,buf,10));
    kprint("unusable memory: ");
    kprintln(itoa(unusablemm,buf,10));
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