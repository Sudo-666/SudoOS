#include "vmm.h"

mm_struct_t* mm_alloc() {
    // 分配一个 mm_struct 结构体
    mm_struct_t* mm  = (mm_struct_t*)kmalloc(sizeof(mm_struct_t));
    if (mm == NULL) return NULL;
    memset(mm, 0, sizeof(mm_struct_t));

    // 分配 PML4 页表
    uint64_t pml4_pa = pmm_alloc_page();
    if (pml4_pa == 0) {
        kfree(mm);
        return NULL;    
    }
    mm->pml4_pa = pml4_pa;
    mm->pml4 = (pg_table_t*)(pml4_pa + HHDM_OFFSET);
    memset(mm->pml4, 0, sizeof(pg_table_t));

    // 映射高半内核空间
    extern pg_table_t* kernel_pml4;
    for (size_t i = 256; i < 512; i++) {
        mm->pml4->entries[i] = kernel_pml4->entries[i];
    }

    // 初始化 VMA 列表
    list_init(&mm->vma_list);
    mm->map_count = 0;
    mm->ref_count = 0;
    mm->mmap_cache = NULL;

    mm->start_code = mm->end_code = 0;
    mm->start_data = mm->end_data = 0;
    mm->start_heap = mm->heap = 0;
    mm->start_stack = 0;

    return mm;
}

void mm_free(mm_struct_t* mm) {
    if (mm == NULL) return;

    // 释放所有 VMA 结构体
    list_node_t* node = mm->vma_list.next;
    while (node != &mm->vma_list) {
        vma_struct_t* vma = container_of(node, vma_struct_t, list_node);
        node = node->next;
        kfree(vma);
    }

    // 递归释放用户页表
    if(mm->pml4)
    {
        user_pgtable_free_recursive(mm->pml4, 4);
        // 释放 PML4 页表对应的物理页结构
        pmm_free_page(mm->pml4_pa);
    }
    // 释放 mm_struct 本身
    kfree(mm);
}

bool mm_map_range(mm_struct_t* mm,uintptr_t va,uintptr_t size,uint64_t vm_flags) {
    if(mm==NULL || size == 0) return false;

    uint64_t start = ALIGN_DOWN(va,PAGE_SIZE);
    uint64_t end = ALIGN_UP(va+size,PAGE_SIZE);
    uint64_t pages = (end - start) / PAGE_SIZE;

    // 创建 VMA 结构体并加入链表
    vma_struct_t* vma = (vma_struct_t*)kmalloc(sizeof(vma_struct_t));
    if(vma==NULL) return false;
    vma->vm_start = start;
    vma->vm_end = end;
    vma->vm_flags = vm_flags;
    vma->mm = mm;
    list_add_before(&vma->list_node,&mm->vma_list);
    mm->map_count++;

    // 转换 vm_flags 到 pte_flags
    uint64_t pte_flags = PTE_PRESENT | PTE_USER;
    if(vm_flags & VM_WRITE) pte_flags |= PTE_RW;
    //if(!(vm_flags & VM_EXEC)) pte_flags |= PTE_NX;

    // 映射每一页
    for(uint64_t i=0;i<pages;i++) {
        uint64_t va = start + i * PAGE_SIZE;
        uint64_t pa = pmm_alloc_page();
        if(pa==0) {
            // 映射失败，回滚已映射的页
            for(uint64_t j=0;j<i;j++) {
                uint64_t rollback_va = start + j * PAGE_SIZE;
                pte_t* pte = vmm_get_pte(mm->pml4,rollback_va);
                if(pte && (*pte & PTE_PRESENT)) {
                    uintptr_t pa = PTE_GET_ADDR(*pte);
                    pmm_free_page(pa);
                    *pte = 0;
                }
            }
            kfree(vma);
            return false;
        }
        vmm_map_page(mm->pml4, va, pa, pte_flags);
    }

    return true;
}

bool mm_copy(mm_struct_t* dst, mm_struct_t* src) {
    if(dst == NULL || src == NULL) return false;

    // 遍历父进程的 VMA 列表
    list_node_t* node = src->vma_list.next;
    while(node != &src->vma_list) {
        vma_struct_t* src_vma = container_of(node, vma_struct_t, list_node);
        node = node->next;
        // 在子进程中映射相同的虚拟地址范围
        if(!mm_map_range(dst, src_vma->vm_start, src_vma->vm_end - src_vma->vm_start, src_vma->vm_flags)) {
            // 映射失败，释放已映射的部分
            mm_free(dst);
            return false;
        }
        // 物理内存深拷贝 (Deep Copy)
        for(uint64_t vaddr = src_vma->vm_start; vaddr < src_vma->vm_end; vaddr += PAGE_SIZE) {
            pte_t* src_pte = vmm_get_pte(src->pml4, vaddr);
            pte_t* dst_pte = vmm_get_pte(dst->pml4, vaddr);
            if(src_pte == NULL || dst_pte == NULL || !(*src_pte & PTE_PRESENT)) {
                continue; // 源页表项不存在或未映射
            }
            // 
            uintptr_t src_pa = PTE_GET_ADDR(*src_pte);
            uintptr_t dst_pa = PTE_GET_ADDR(*dst_pte);
            if(dst_pa == 0) {
                // 分配失败，释放已分配的部分
                mm_free(dst);
                return false;
            }
            // 拷贝数据
            memcpy((void*)(dst_pa + HHDM_OFFSET), (void*)(src_pa + HHDM_OFFSET), PAGE_SIZE);
            // 更新子进程页表项
            *dst_pte = (dst_pa & 0x000FFFFFFFFFF000) | (*src_pte & 0xFFF); // 保留标志位
        }
    }
    return true;
}