#include "pmm.h"
#include "../drivers/console.h"

// 定义HHDM_OFFSET
uint64_t HHDM_OFFSET = 0;

// 定义全局变量
uint8_t* bitmap = NULL;             // 位图数组的虚拟地址
size_t bitmap_size = 0;             // 位图占用的字节数
size_t total_pages = 0;             // 物理内存总页数
size_t free_pages = 0;              // 当前空闲页数（统计用）

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
bool bit_test(size_t bit) {
    return bitmap[bit / 8] & (1 << (bit % 8));
}

/**
 * @brief 将bitmap中从pa开始的pgnum个物理块标志成空闲
 * 
 * @param pa 
 * @param pgnum 
 */
void pmm_set_free(uintptr_t pa,size_t pgnum) {
    size_t pgidx = pa2pgidx(pa);
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
    size_t pgidx = pa2pgidx(pa);
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
    kprintln("===== Start init pmm... =====");

    uintptr_t highest_pa = 0;
    // 获取物理内存最高地址
    for(uint64_t i=0;i<mmap->entry_count;i++) {
        struct limine_memmap_entry *e = mmap->entries[i];
        uintptr_t end = e->base + e->length;
        if(end > highest_pa) {
            highest_pa = end;
        }
    }
    kprintf("Get the highest address: %ld\n",highest_pa);

    // 计算分页数以及bitmap大小
    total_pages = highest_pa/PAGE_SIZE;
    bitmap_size = total_pages/8;
    if(total_pages%8) bitmap_size++;
    kprintf("Total pages num is %ld\n",total_pages);
    kprintf("Bitmap size is %ld\n",bitmap_size);

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

    kprintf("Bitmap is set PA at: %lx\n",bitmap_pa);

    // 利用 Limine HHDM 获取位图的虚拟地址
    bitmap = (uint8_t*)(bitmap_pa + HHDM_OFFSET);

    kprintf("Bitmap is set VA at: %lx\n",(uintptr_t)(bitmap));

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

            kprintf("Free area from %lx to %lx (%ld pages)\n",start,end,pg_num);
        }
    }

    // 保护 0 号物理页 (NULL)
    // 防止 pmm_alloc 返回 0，导致空指针混淆
    pmm_set_busy(0, 1);
    kprintln("===== Init pmm done!!! =====");
}


/**
 * @brief next_fit算法分配空闲物理页框
 * 
 * @return uint64_t 
 */
uint64_t pmm_alloc_page() {
    // 0. 如果没有空闲页，直接返回，避免无效遍历
    // 1. 第一轮搜索：从上次的位置往后找
    // 2. 第二轮搜索：如果后面满了，回绕从头(0)找
    // 3. 真的找不到（虽然理论上前面判断了 free_pages > 0 不会进这里）
    
    if(free_pages == 0) {
        return;
    }
    uint64_t ret_pa = 0;
    
    for(size_t i = last_free_index;i<total_pages;i++) {
        // 发现该位是 0 (Free)
        if(!bit_test(i)) {
            bit_set(i);
            free_pages -= 1;
            last_free_index = i + 1;
            return pgidx2pa(i);
        }
    }

    for(size_t i = 0;i<last_free_index;i++) {
        // 发现该位是 0 (Free)
        if(!bit_test(i)) {
            bit_set(i);
            free_pages -= 1;
            last_free_index = i + 1;
            return pgidx2pa(i);
        }
    }

    return 0;
}

/**
 * @brief 释放物理地址pa对应的物理页框
 * 
 * @param pa 
 */
void pmm_free_page(uint64_t pa) {
    // 1. 越界检查
    // 2. 双重释放检查 (Optional, but good for debug)
    // 3. 执行释放
    // 4. 游标回退优化（next_fit）
    size_t pgidx = pa2pgidx(pa);
    if(pgidx >= total_pages) {
        kprintln("Trying to free invalid address(pg>totalpages)");
        return;
    }

    if(!bit_test(pgidx)) {
        kprintln("double free detected");
        return;
    }

    bit_unset(pgidx);
    free_pages += 1;

    if(pgidx < last_free_index)
        last_free_index = pgidx;

    return;

}