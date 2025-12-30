.global kernel_thread_entry
.extern kthread_exit

kernel_thread_entry:
    sti
    call *%rbx
    call kthread_exit

