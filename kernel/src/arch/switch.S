.global switch_to
# void switch_to(struct context **prev, struct context *next);
# RDI = &prev->context (这是指向指针的指针，因为我们要修改 prev 指向栈顶)
# RSI = next->context  (这是下一个进程的栈顶指针)

switch_to:
    # 1. 保存当前进程的上下文 (Callee-saved)
    push %rdi
    push %rbp
    push %rbx
    push %r12
    push %r13
    push %r14
    push %r15

    # 2. 关键：保存当前栈顶 (RSP) 到 prev 结构体中
    # 假设 proc 结构体里有一个 context* 类型的字段
    mov %rsp, (%rdi)

    # 3. 关键：加载下一个进程的栈顶
    mov %rsi, %rsp

    # 4. 恢复下一个进程的上下文
    pop %r15
    pop %r14
    pop %r13
    pop %r12
    pop %rbx
    pop %rbp
    pop %rdi

    # 5. 跳转执行 (ret 会弹出栈顶的 RIP，这个 RIP 是上一次该进程切出时保存的)
    ret