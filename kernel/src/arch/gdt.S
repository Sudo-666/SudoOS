/* 告诉汇编器这是 64 位代码 */
.code64
.section .text

.global gdt_flush
.global tss_load

/* * void gdt_flush(uint64_t gdt_ptr_addr);
 * RDI 存储了 gdtp 的地址 (System V ABI)
 */
gdt_flush:
    lgdt (%rdi)          /* 加载新 GDT */
    
    /* 刷新数据段寄存器 */
    mov $0x10, %ax       /* 内核数据段选择子 (Index 2 * 8 = 0x10) */
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs
    mov %ax, %ss

    /* 刷新代码段 CS (通过 push/retfq 实现长跳转) */
    pushq $0x08          /* 新的内核代码段选择子 (Index 1 * 8 = 0x08) */
    lea .next(%rip), %rax /* 获取跳转地址 */
    pushq %rax
    retfq                /* 弹出 RIP 和 CS，完成跳转 */
.next:
    ret

/* * void tss_load();
 */
tss_load:
    mov $0x2B, %ax       /* TSS 选择子 (Index 5 * 8 = 0x28 | RPL 3 = 0x2B) */
    ltr %ax              /* 加载任务寄存器 Task Register */
    ret