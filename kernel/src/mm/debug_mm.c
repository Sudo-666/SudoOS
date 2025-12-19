#include "debug_mm.h"
#include "../drivers/console.h"
#include "../lib/string.h"


/**
 * @brief  根据内存块的类型返回相应的字符串便于调试
 * 
 * @param type 
 * @return const char* 
 */
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
 * @brief 打印调试获取到的memmap信息
 * 
 * @param mmap 
 */
void debug_memmap(struct limine_memmap_response *mmap) {
    kprintln("start debug memmap ... ");
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

        char base_buf[64];
        char len_buf[64];

        kprint("Entry ");
        kprint_uint64(i);
        kprint(":");
        kprint(" base address at: ");
        kprint(itoa_uint64(e->base, base_buf, 16));
        kprint(" size=");
        kprint(itoa_uint64(e->length, len_buf, 10));
        kprint(" bytes");
        kprint(" type=");
        kprintln(memtype2str(e->type));
    }
    char buf[64];
    kprint("total memory: ");
    kprint(itoa_uint64(totalmm,buf,10));
    kprintln(" bytes");
    kprint("usable memory: ");
    kprint(itoa_uint64(usablemm,buf,10));
    kprintln(" bytes");
    kprint("unusable memory: ");
    kprint(itoa_uint64(unusablemm,buf,10));
    kprintln(" bytes");
    kprintln("=== DEBUG MEMORY MAP FINISHED! ===");
}
