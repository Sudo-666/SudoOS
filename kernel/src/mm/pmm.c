#include "pmm.h"
#include "../drivers/console.h"

// 定义HHDM_OFFSET
uint64_t HHDM_OFFSET = 0;

/**
 * @brief 将bitmap的bit位设置成1
 * 
 * @param bit 
 */
static inline void bit_set(size_t bit) {
    bitmap[bit / 8] |= (1 << (bit % 8));
}


/**
 * @brief 将bitmap的bit位设置成0
 * 
 * @param bit 
 */
static inline void bit_unset(size_t bit) {
    bitmap[bit / 8] &= ~(1 << (bit % 8));
}

/**
 * @brief 测试bitmap的bit位是否为1
 * 
 * @param bit 
 */
static inline bool bit_test(size_t bit) {
    return bitmap[bit / 8] & (1 << (bit % 8));
}

/**
 * @brief 将bitmap中从pa开始的pgnum个物理块标志成空闲
 * 
 * @param pa 
 * @param pgnum 
 */
void pmm_set_free(uintptr_t pa,size_t pgnum) {
    size_t pgidx = pa2pg_idx(pa);
    // 越界
    if(pgidx+pgnum>=total_pages) {
        return;
    }

    for(size_t i=0;i<pgnum;i++) {
        if(bit_test(pgidx+i)) {
            bit_unset(pgidx+i);
            free_pages++;
        }
    }
}
/**
 * @brief 将bitmap中从pa开始的pgnum个物理块标志成预留
 * 
 * @param pa 
 * @param pgnum 
 */
void pmm_set_busy(uintptr_t pa,size_t pgnum) {
    size_t pgidx = pa2pg_idx(pa);
    // 越界
    if(pgidx+pgnum>=total_pages) {
        return;
    }

    for(size_t i=0;i<pgnum;i++) {
        if(!bit_test(pgidx+i)) {
            bit_set(pgidx+i);
            free_pages--;
        }
    }
}


void pmm_init(struct limine_memmap_response* mmap) {
    kprintln("Start init pmm...");

    uintptr_t highest_pa = 0;
    // 获取物理内存最高地址
    for(uint64_t i=0;i<mmap->entry_count;i++) {
        struct limine_memmap_entry *e = mmap->entries[i];
        uintptr_t end = e->base + e->length;
        if(end > highest_pa) {
            highest_pa = end;
        }
    }
    kprint("Get the highest address: ");
    kprint_uint64(highest_pa);
    kprintln("");
    // 计算分页数以及bitmap大小
    total_pages = highest_pa/PAGE_SIZE;
    bitmap_size = total_pages/8;
    if(total_pages%8) bitmap_size++;
    kprint("Tolal pages num is ");
    kprint_uint64(total_pages);
    kprintln("");
    kprint("Bitmap size is ");
    kprint_uint64(bitmap_size);
    kprintln("");

    // 寻找区域存放bitmap
    uintptr_t bitmap_pa = 0;
    for(uint64_t i=0;i<mmap->entry_count;i++) {
        struct limine_memmap_entry *e = mmap->entries[i];
        if(e->type == LIMINE_MEMMAP_USABLE && e->length >= bitmap_size) {
            bitmap_pa = (uintptr_t)e->base;

            // 实际上，为了对齐安全，最好把 bitmap_size 向上对齐到 PAGE_SIZE 再扣除
            // 这样保证剩下的内存也是页对齐的。
            size_t bitmap_pages = ALIGN_UP(bitmap_size, PAGE_SIZE) / PAGE_SIZE;
            size_t bitmap_reserved_size = bitmap_pages * PAGE_SIZE;
            // 更新块信息
            if (e->length >= bitmap_reserved_size) {
                 e->base += bitmap_reserved_size;
                 e->length -= bitmap_reserved_size;
                 break;
            }
        }
    }

    if (!bitmap_pa) {
        // 恐慌：内存太小，连位图都放不下
        for(;;) __asm__("hlt"); 
    }

    kprint("Bitmap is setted pa at: ");
    kprint_uint64(bitmap_pa);
    kprintln("");

    // 利用 Limine HHDM 获取位图的虚拟地址
    bitmap = (uint8_t*)(bitmap_pa + HHDM_OFFSET);

    kprint("Bitmap is set VA at: ");
    kprint_uint64((uintptr_t)bitmap);
    kprintln("");

    // 初始化位图全为1
    memset(bitmap,0xFF,bitmap_size);

    for(uint64_t i=0;i<mmap->entry_count;i++) {
        struct limine_memmap_entry *e = mmap->entries[i];
        // 更新位图空闲信息
        if(e->type == LIMINE_MEMMAP_USABLE) {
            uintptr_t start = ALIGN_UP(e->base,PAGE_SIZE);
            uintptr_t end = ALIGN_DOWN(e->base+e->length,PAGE_SIZE);
            // 无效页框
            if(start >= end) {
                continue;
            }
            size_t pg_num = (end-start)/PAGE_SIZE;
            pmm_set_free(start,pg_num);

            kprint("free area start at : ");
            kprint_uint64(start);
            kprint(" and end at : ");
            kprint_uint64(end);
            kprint(" is divided into ");
            kprint_uint64(pg_num);
            kprintln(" pages");
        }
    }

    // 保护 0 号物理页 (NULL)
    // 防止 pmm_alloc 返回 0，导致空指针混淆
    pmm_set_busy(0, 1);
    kprintln("Init pmm done...");
}
