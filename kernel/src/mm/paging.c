#include "paging.h"
#include "../drivers/console.h"
#include "../lib/string.h"
#include "../arch/x86_64.h"

// 将物理地址转换成虚拟地址
#define pa2kva(pa) (pa+HHDM_OFFSET)

// 全局内核 PML4 指针
pg_table_t* kernel_pml4 = NULL;
/**
 * @brief 获取下一级页表指针。如果 allocate=true 且不存在，则创建之。
 * 
 * @param pgtable 
 * @param index 
 * @param allocate 
 * @return pg_table_t* 
 */
pg_table_t* get_next_table(pg_table_t* pgtable, uint64_t index, bool allocate) {
    // 获取对应的页表项
    pte_t* entry = &pgtable->entries[index];

    // 检查是否存在
    if(*entry & PTE_PRESENT) {
        uintptr_t next_pa = PTE_GET_ADDR(*entry);
        return (pg_table_t*)pa2kva(next_pa);
    }

    if(allocate) {
        uintptr_t newpg_pa = pmm_alloc_page();
        // 分配失败
        if(newpg_pa == 0) {

            kprintln("Panic:OOM when allocating page table!");
            return NULL;
        }

        // 转换成虚拟地址，并初始化为0
        pg_table_t* newpg_va = (pg_table_t*)pa2kva(newpg_pa);
        memset(newpg_va, 0, PAGE_SIZE);

        // 设置页表项
        *entry = newpg_pa | PTE_PRESENT | PTE_RW;
        // 返回
        return newpg_va;
    }
    
    // 如果不存在且不需要分配，返回 NULL
    return NULL;
}

/**
 * @brief 映射虚拟地址到物理地址
 * @param pml4 顶级页表虚拟地址
 * @param virt 虚拟地址
 * @param phys 物理地址
 * @param flags 标志位 (PTE_RW, PTE_USER 等)
 */
void vmm_map_page(pg_table_t* pml4, uintptr_t va, uintptr_t pa, uint64_t flags) 
{
    // 获取索引
    uint64_t idx4 = PML4_IDX(va);
    uint64_t idx3 = PDPT_IDX(va);
    uint64_t idx2 = PD_IDX(va);
    uint64_t idx1 = PT_IDX(va);

    // 获取页表项
    pg_table_t* pdpt = get_next_table(pml4,idx4,1);
    if (pdpt == NULL) return; // 检查分配是否失败
    pg_table_t* pd = get_next_table(pdpt,idx3,1);
    if (pd == NULL) return; // 检查分配是否失败
    pg_table_t* pt = get_next_table(pd,idx2,1);
    if (pt == NULL) return; // 检查分配是否失败

    // 设置最后一级 PTE
    pt->entries[idx1] = pa|flags|PTE_PRESENT;
    
    // 刷新 TLB 使新的页表映射生效
    invlpg((void*)va);

}

// 获取内核（可执行文件）在物理内存和虚拟内存中的加载基地址。
__attribute__((used, section(".limine_requests"))) 
static volatile struct limine_executable_address_request kernel_addr_request = {
    .id = LIMINE_EXECUTABLE_ADDRESS_REQUEST_ID,
    .revision = 0
};




void paging_init(struct limine_memmap_response* mmap) {
    kprintln("===== Start init PAGING... =====");

    // 分配一个新的 PML4 作为内核的基础页表
    uint64_t pml4_pa = pmm_alloc_page();
    if(pml4_pa == 0) {
        kprintln("Panic: Failed to allocate PML4!");
        for(;;) __asm__("hlt");
    }
    kernel_pml4 = (pg_table_t*) pa2kva(pml4_pa);
    memset(kernel_pml4, 0, PAGE_SIZE);

    kprintf("Kernel PML4 allocated at PA: %lx, VA: %lx\n", pml4_pa, (uintptr_t)kernel_pml4);

    // 获取内核空间
    uintptr_t k_start_va = (uintptr_t)__kernel_start;
    uintptr_t k_end_va   = (uintptr_t)__kernel_end;
    size_t kernel_size = k_end_va - k_start_va;
    kernel_size = ALIGN_UP(kernel_size, PAGE_SIZE);
    kprintf("Kernel Range: %lx - %lx (Size: %ld bytes)\n", k_start_va, k_end_va, kernel_size);

    // 映射内核本身
    // 获取内核空间基地址
    struct limine_executable_address_response* kaddr = kernel_addr_request.response;
    if(!kaddr) {
        kprintln("Panic: Failed to get kernel address!");
        for(;;) __asm__("hlt");
    }
    uintptr_t kva_base = kaddr->virtual_base;
    uint64_t kpa_base = kaddr->physical_base;
    
    // 映射内核空间（映射整个加载区域）
    for(uint64_t i = 0; i < kernel_size; i += PAGE_SIZE) {
        vmm_map_page(kernel_pml4, kva_base + i, kpa_base + i, PTE_PRESENT | PTE_RW);
    }
    kprintln("Kernel mapped...");

    // 映射HHDM空间访问物理内存
    uintptr_t maxpa = total_pages * PAGE_SIZE;
    kprintf("Mapping HHDM: 0x0 - 0x%lx (total_pages=%ld)\n", maxpa, total_pages);

    for (uint64_t i = 0; i < mmap->entry_count; i++) {
        struct limine_memmap_entry *entry = mmap->entries[i];

        // 仅映射可用的 RAM 和内核相关的区域
        // 跳过保留区域、坏内存或空洞
        if (entry->type == LIMINE_MEMMAP_USABLE ||
            entry->type == LIMINE_MEMMAP_EXECUTABLE_AND_MODULES ||
            entry->type == LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE ||
            entry->type == LIMINE_MEMMAP_FRAMEBUFFER) {
            
            uintptr_t start = ALIGN_DOWN(entry->base, PAGE_SIZE);
            uintptr_t end = ALIGN_UP(entry->base + entry->length, PAGE_SIZE);

            kprintf("Mapping region: %lx - %lx\n", start, end);

            for (uintptr_t pa = start; pa < end; pa += PAGE_SIZE) {
                // 使用 4KB 映射
                vmm_map_page(kernel_pml4, pa + HHDM_OFFSET, pa, PTE_PRESENT | PTE_RW);
            }
        }
    }
    
    kprintln("HHDM mapped...");

    // 加载到cr3
    lcr3(pml4_pa);
    kprintln("===== PAGING Init Done! CR3 switched. =====");
}

