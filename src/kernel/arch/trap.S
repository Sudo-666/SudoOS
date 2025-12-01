.section .text
.global alltraps

# 宏：没有 Error Code 的中断 (手动压入 0)
.macro ISR_NOERRCODE num
    .global vector\num
    vector\num:
        push $0          # 填充 error_code
        push $\num       # 压入中断号
        jmp alltraps     # 跳到公共处理逻辑
.endm

# 宏：有 Error Code 的中断 (CPU 已经压了，不用管)
.macro ISR_ERRCODE num
    .global vector\num
    vector\num:
        push $\num       # 压入中断号
        jmp alltraps     # 跳到公共处理逻辑
.endm

# --- 1. 定义前 32 个异常入口 ---
ISR_NOERRCODE 0  # Divide by zero
ISR_NOERRCODE 1  # Debug
ISR_NOERRCODE 2  # NMI
ISR_NOERRCODE 3  # Breakpoint
ISR_NOERRCODE 4  # Overflow
ISR_NOERRCODE 5  # Bound Range Exceeded
ISR_NOERRCODE 6  # Invalid Opcode
ISR_NOERRCODE 7  # Device Not Available
ISR_ERRCODE   8  # Double Fault (有错误码)
ISR_NOERRCODE 9  # Coprocessor Segment Overrun
ISR_ERRCODE   10 # Invalid TSS (有错误码)
ISR_ERRCODE   11 # Segment Not Present (有错误码)
ISR_ERRCODE   12 # Stack-Segment Fault (有错误码)
ISR_ERRCODE   13 # General Protection Fault (有错误码)
ISR_ERRCODE   14 # Page Fault (有错误码!!!)
ISR_NOERRCODE 15 # Reserved
ISR_NOERRCODE 16 # x87 FPU Exception
ISR_ERRCODE   17 # Alignment Check
ISR_NOERRCODE 18 # Machine Check
ISR_NOERRCODE 19 # SIMD Floating-Point Exception
# 20-31 Reserved
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
ISR_NOERRCODE 30
ISR_NOERRCODE 31

# --- 2. 定义系统调用入口 (0x80) ---
ISR_NOERRCODE 128 # 0x80

# --- 3. 公共处理逻辑 (alltraps) ---
.extern trap_handler # C++ 里的处理函数
alltraps:
    # 保存所有通用寄存器
    push %rax
    push %rbx
    push %rcx
    push %rdx
    push %rsi
    push %rdi
    push %rbp
    push %r8
    push %r9
    push %r10
    push %r11
    push %r12
    push %r13
    push %r14
    push %r15

    # 此时栈顶指针 %rsp 正好指向 TrapFrame 的开头
    # 我们把它作为参数传递给 trap_handler(TrapFrame* tf)
    # x86_64 ABI: 第一个参数放在 rdi
    mov %rsp, %rdi

    # 调用 C++ 处理函数
    call trap_handler

    # 恢复上下文
    pop %r15
    pop %r14
    pop %r13
    pop %r12
    pop %r11
    pop %r10
    pop %r9
    pop %r8
    pop %rbp
    pop %rdi
    pop %rsi
    pop %rdx
    pop %rcx
    pop %rbx
    pop %rax

    # 清理掉 trap_num 和 error_code (共16字节)
    add $16, %rsp

    # 返回用户态
    iretq