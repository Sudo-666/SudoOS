/* 告诉汇编器这是 64 位代码 */
.code64
.section .text

/* 声明 C 语言中的处理函数 */
.extern isr_handler

/* === 宏定义：没有错误码的异常 === */
/* 相当于 NASM 的 %macro ISR_NOERRCODE 1 */
.macro ISR_NOERRCODE num
    .global isr\num
    isr\num:
        pushq $0        /* 手动压入假错误码，保持栈平衡 */
        pushq $\num     /* 压入中断号 */
        jmp isr_common_stub
.endm

/* === 宏定义：有错误码的异常 === */
.macro ISR_ERRCODE num
    .global isr\num
    isr\num:
        /* CPU 已经压入了错误码，这里只压中断号 */
        pushq $\num
        jmp isr_common_stub
.endm

/* === 批量定义异常入口 === */
ISR_NOERRCODE 0
ISR_NOERRCODE 1
ISR_NOERRCODE 2
ISR_NOERRCODE 3
ISR_NOERRCODE 4
ISR_NOERRCODE 5
ISR_NOERRCODE 6
ISR_NOERRCODE 7
ISR_ERRCODE   8
ISR_NOERRCODE 9
ISR_ERRCODE   10
ISR_ERRCODE   11
ISR_ERRCODE   12
ISR_ERRCODE   13
ISR_ERRCODE   14
ISR_NOERRCODE 15
ISR_NOERRCODE 16
ISR_ERRCODE   17
ISR_NOERRCODE 18
ISR_NOERRCODE 19
ISR_NOERRCODE 20
ISR_NOERRCODE 21
ISR_NOERRCODE 22
ISR_NOERRCODE 23
ISR_NOERRCODE 24
ISR_NOERRCODE 25
ISR_NOERRCODE 26
ISR_NOERRCODE 27
ISR_NOERRCODE 28
ISR_NOERRCODE 29
ISR_ERRCODE   30
ISR_NOERRCODE 31
ISR_NOERRCODE 32
ISR_NOERRCODE 33
ISR_NOERRCODE 128

/* === 通用中断处理桩 === */
isr_common_stub:
    /* 1. 保存所有通用寄存器 (Context) */
    /* 注意：必须按照 registers_t 结构体的逆序压栈 */
    pushq %rax
    pushq %rbx
    pushq %rcx
    pushq %rdx
    pushq %rsi
    pushq %rdi
    pushq %rbp
    pushq %r8
    pushq %r9
    pushq %r10
    pushq %r11
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15

    /* 2. 准备调用 C 函数 */
    /* void isr_handler(registers_t* regs); */
    /* System V ABI 规定第一个参数放在 rdi 寄存器 */
    movq %rsp, %rdi  

    /* 3. 调用 C 语言处理函数 */
    call isr_handler

    /* 4. 恢复上下文 */
    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %r11
    popq %r10
    popq %r9
    popq %r8
    popq %rbp
    popq %rdi
    popq %rsi
    popq %rdx
    popq %rcx
    popq %rbx
    popq %rax

    /* 5. 清理栈上的中断号(8字节)和错误码(8字节) -> 共16字节 */
    addq $16, %rsp

    /* 6. 返回用户态 */
    iretq