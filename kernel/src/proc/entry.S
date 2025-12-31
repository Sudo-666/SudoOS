.global kernel_thread_entry
.extern kthread_exit

kernel_thread_entry:
    sti
    call *%rbx
    call kthread_exit

.global fork_ret_entry
fork_ret_entry:
    # 此时 RSP 指向 child_tf (因为 switch_to 弹出了 context)
    
    # 恢复通用寄存器 (与 isr_common_stub 的后半部分类似)
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

    # 弹出中断号和错误码 (trap_frame 结构体里定义的占位符)
    addq $16, %rsp 

    # 返回用户态
    iretq