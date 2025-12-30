#include "pmm.h"
#include "../drivers/console.h"
#include "../arch/x86_64.h"

// 定义HHDM_OFFSET
uint64_t HHDM_OFFSET = 0;

// 定义全局变量
uint8_t* bitmap = NULL;             // 位图数组的虚拟地址
size_t bitmap_size = 0;             // 位图占用的字节数
size_t total_pages = 0;             // 物理内存总页数
size_t free_pages = 0;              // 当前空闲页数（统计用）
size_t last_free_index = 0;         // 优化变量，下次分配时从这里开始扫描
uint64_t kstack_ptr = KERNEL_STACK_BASE;  // 内核栈指针

/**
 * @brief 将bitmap的bit位设置成1
 * 
 * @param bit 
 */
void bit_set(size_t bit) {
    bitmap[bit / 8] |= (1 << (bit % 8));
}


/**
 * @brief 将bitmap的bit位设置成0
 * 
 * @param bit 
 */
void bit_unset(size_t bit) {
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

static list_node_t kheap_list; // 堆块全局管理链表
static uint64_t kheap_top = KERNEL_HEAP_BASE;     // 追踪堆当前的虚拟地址边界




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
        kprintln("ERROR: No free pages available for allocation!");
        return 0;
    }
    
    for(size_t i = last_free_index; i < total_pages; i++) {
        // 发现该位是 0 (Free)
        if(!bit_test(i)) {
            bit_set(i);
            free_pages -= 1;
            last_free_index = i + 1;
            // kprintf("PMM Alloc: %lx\n", pgidx2pa(i));
            return pgidx2pa(i);
        }
    }

    for(size_t i = 0; i < last_free_index; i++) {
        // 发现该位是 0 (Free)
        if(!bit_test(i)) {
            bit_set(i);
            free_pages -= 1;
            last_free_index = i + 1;
            // kprintf("PMM Alloc: %lx\n", pgidx2pa(i));
            return pgidx2pa(i);
        }
    }

    kprintln("ERROR: Failed to allocate page (allocation loop failed)!");
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

extern pg_table_t* kernel_pml4;
bool kheap_expand(size_t pgnum) {
    for(size_t i=0;i<pgnum;i++) {
        uint64_t pa = pmm_alloc_page();
        if(pa==0) return false;
        // 映射在页表中映射内核堆虚拟地址
        vmm_map_page(kernel_pml4,kheap_top,pa,PTE_PRESENT | PTE_RW);
        // 设置空闲内核堆块内存头
        kheap_pghdr_t* pghdr=(kheap_pghdr_t*) kheap_top;
        pghdr->is_free=0;
        pghdr->size = PAGE_SIZE-HEADER_SIZE;
        // 将新页加入循环链表的“末尾”
        list_add_before(&pghdr->node,&kheap_list);

        // 调用 kfree 逻辑会自动将它们合并成一个大的空闲块
        kfree((void*)((uint64_t)pghdr+HEADER_SIZE));

        // 更新堆顶指针
        kheap_top += PAGE_SIZE;
    }
    return true;
} 

static kheap_pghdr_t* first_fit(size_t size) {
    list_node_t* cur;
    for(cur = kheap_list.next;cur!=&kheap_list;cur=cur->next) {
        kheap_pghdr_t *hdr = (kheap_pghdr_t*) cur;
        if(hdr->is_free && hdr->size >= size) {
            return hdr;
        }
    }
    return NULL;
}


void* kmalloc(size_t size) {
    size = ALIGN_UP(size,8);

    kheap_pghdr_t* target = first_fit(size);

    if(!target) {
        // 计算扩充需要多少页
        size_t pg_needed = ALIGN_UP(size+HEADER_SIZE,PAGE_SIZE) / PAGE_SIZE;
        // 至少扩充一页
        if(pg_needed == 0) pg_needed = 1;
        if(kheap_expand(pg_needed)) {
            // 扩充后重新分配
            return kmalloc(size);
        }
        return NULL;
    }

    // 切割空闲内存块
    if(target->size >= size+HEADER_SIZE+MIN_SPLIT) {
        kheap_pghdr_t* rest = (kheap_pghdr_t*)((uint64_t)target+HEADER_SIZE+size);
        rest->is_free = 1;
        rest->size = target->size - size - HEADER_SIZE;
        list_add_after(&rest->node,&target->node);
        target->size =size;
    }

    target->is_free = false;
    return (void*)((uint64_t)target+HEADER_SIZE);
} 

void kfree(void* ptr) {
    if(!ptr) return;
    kheap_pghdr_t * hdr = (kheap_pghdr_t *) ((uint64_t)ptr-HEADER_SIZE);
    hdr->is_free = 1;
    list_node_t * next_node = hdr->node.next;
    list_node_t * prev_node = hdr->node.prev;

    // 向后合并
    if(next_node != &kheap_list) {
        kheap_pghdr_t* next= (kheap_pghdr_t*) next_node;
        if((uint64_t)hdr + hdr->size + HEADER_SIZE == (uint64_t)next) {
            hdr->size += next->size + HEADER_SIZE;
            list_del(next_node);
        }
    }

    // 向前合并
    if(prev_node != &kheap_list) {
        kheap_pghdr_t* prev=(kheap_pghdr_t*) prev_node;
        if((uint64_t)prev + prev->size +HEADER_SIZE == (uint64_t)hdr) {
            prev->size += hdr->size + HEADER_SIZE;
            list_del(&hdr->node);
        }
    }

}

void kheap_init(size_t pgnum) {
    kprintln("Initing kernel heap memory manager...");
    list_init(&kheap_list);
    kheap_top = KERNEL_HEAP_BASE;
    kheap_expand(pgnum);
    kprintf("kernel heap is setted at %lx - %lx !\n",KERNEL_HEAP_BASE,kheap_top);
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
            // 实际上，为了对齐安全，最好把 bitmap_size 向上对齐到 PAGE_SIZE 再扣除
            // 这样保证剩下的内存也是页对齐的。
            size_t bitmap_pages = ALIGN_UP(bitmap_size, PAGE_SIZE) / PAGE_SIZE;
            size_t bitmap_reserved_size = bitmap_pages * PAGE_SIZE;
            // 更新块信息
            if (e->length >= bitmap_reserved_size) {
                bitmap_pa = (uintptr_t)e->base;
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

    // 【修复】：显式地将位图所在的物理页重新标记为占用
    size_t bitmap_pages = ALIGN_UP(bitmap_size, PAGE_SIZE) / PAGE_SIZE;
    pmm_set_busy(bitmap_pa, bitmap_pages);
    kprintf("Reserved bitmap area: %lx (pages: %ld)\n", bitmap_pa, bitmap_pages);
    
    // 保护 0 号物理页 (NULL)
    // 防止 pmm_alloc 返回 0，导致空指针混淆
    pmm_set_busy(0, 1);
    kprintln("===== Init pmm done!!! =====");
}


void* kstack_init(size_t size) {
    kprintln("Initing kernel stack ...");
    // 预留 Guard Page
    kstack_ptr += PAGE_SIZE;

    uint64_t vaddr_bottom = kstack_ptr;
    uint64_t vaddr_top = vaddr_bottom + size;

    // 更新全局指针，为下一次分配做准备
    kstack_ptr = vaddr_top;

    // 循环映射每一页
    for (uint64_t v = vaddr_bottom; v < vaddr_top; v += PAGE_SIZE) {
        uint64_t paddr = pmm_alloc_page();
        if (paddr == 0) {
            kprintln("Error: OOM during kstack allocation!");
            return NULL;
        }

        vmm_map_page(kernel_pml4, v, paddr, PTE_PRESENT | PTE_RW);
    }

    kprintf("kernel stack allocated: %lx - %lx\n", vaddr_bottom, vaddr_top);
    
    // 循环结束后再返回栈顶地址
    return (void*)vaddr_top;
}

void kstack_free(uintptr_t kstack_base) {
    /*
        遍历栈的虚拟地址范围（按页遍历）。
        通过页表找到每一页对应的物理地址（PA）。
        调用 pmm_free_page(pa) 归还给物理内存管理器。
        解除虚拟映射.
    */
    kprintln("Freeing kernel stack ...");
    uintptr_t base = kstack_base;
    uintptr_t top = base + KSTACK_SIZE;

    for(uintptr_t vaddr = base; vaddr < top ; vaddr += PAGE_SIZE)
    {
        pte_t* pte = vmm_get_pte(kernel_pml4, vaddr);
        if(pte && (*pte & PTE_PRESENT)) {
            uint64_t pa = PTE_GET_ADDR(*pte);
            pmm_free_page(pa);
            *pte = 0; // 解除映射
            invlpg((void*) vaddr); // 刷新TLB
        }
    }

    kprintf("Kernel stack freed: %lx - %lx\n", base, top);   
}