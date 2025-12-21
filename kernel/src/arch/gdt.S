.code64
.section .text
.global gdt_flush
.global tss_load

gdt_flush:
    /* System V AMD64 ABI 规定：第一个参数在 %rdi 中 */
    /* 这里的 %rdi 存储的是 gdt_ptr 的指针 */
    lgdt (%rdi)

    /* 1. 刷新数据段寄存器 (Kernel Data = 0x10) */
    mov $0x10, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs
    mov %ax, %ss

    /* 2. 刷新代码段寄存器 (CS) */
    /* 我们利用 "远返回" (retfq) 机制来强制修改 CS */
    /* 栈布局必须是：[新CS] [新RIP] */
    
    pushq $0x08             /* 压入新的代码段选择子 (Kernel Code) */
    leaq .reload(%rip), %rax 
    pushq %rax              /* 压入跳转的目标地址 (.reload) */
    retfq                   /* 执行远返回：CPU 会弹出 CS 和 RIP，实现跳转 */

.reload:
    /* 此时 CS 已经是 0x08 了，数据段也是 0x10 了 */
    /* 栈顶现在恢复到了调用 gdt_flush 时的状态（即存放着 C 函数的返回地址） */
    ret

tss_load:
    /* 加载 TSS 选择子 (0x28) 到 TR 寄存器 */
    mov $0x28, %ax
    ltr %ax
    ret