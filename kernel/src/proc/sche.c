#include "sche.h"

extern pcb_t* current_proc;
extern list_node_t ready_queue;
extern pcb_t* idle_proc;

void schedule() {
    // 简单的轮转调度算法
    uint64_t rflags = read_rflags(); // 读取 RFLAGS 寄存器
    bool interrupts_enabled = rflags & (1 << 9); // IF 位

    cli(); // 关中断，防止调度过程中被打断

    pcb_t* prev = current_proc;
    pcb_t* next = NULL;

    // 先处理当前进程
    if(prev->proc_state == PROC_RUNNING && prev != idle_proc)
    {
        // 如果时间片用完了，把它变成 READY 并加入队列尾部
        if(prev->time_slice == 0) {
            prev->proc_state = PROC_READY;
            prev->time_slice = TIME_SLICE_DEFAULT;
            list_add_before(&ready_queue, &prev->sched_node);
        } else {
            // 时间片没用完，继续运行
            prev->proc_state = PROC_RUNNING;
            if(interrupts_enabled) {
                sti(); // 恢复中断
            }
            return;
        }
    }

    // 从就绪队列中取下一个进程
    // 如果就绪队列为空
    if(ready_queue.next == &ready_queue) {
        if(prev != idle_proc && prev->proc_state == PROC_RUNNING) {
            // 没有其他进程可运行，继续运行当前进程
            prev->proc_state = PROC_RUNNING;
            prev->time_slice = TIME_SLICE_DEFAULT; // 重置时间片
            if(interrupts_enabled) {
                sti(); // 恢复中断
            }
            return;

        }
        else {
            next = idle_proc; // 切换到 idle 进程
            next->proc_state = PROC_RUNNING;
            next->time_slice = TIME_SLICE_DEFAULT;
        }
    }
    else {
        // 就绪队列不为空，取出下一个进程
        list_node_t* next_node = ready_queue.next;
        list_del(next_node);
        next = container_of(next_node, pcb_t, sched_node);
    }

    // 切换到下一个进程
    if(prev != next) {
        next->proc_state = PROC_RUNNING;
        current_proc = next;
        next->time_slice = TIME_SLICE_DEFAULT;
        set_tss_stack(next->rsp); // 切换内核栈
        if(next->mm != NULL) {
            lcr3((uintptr_t)next->mm->pml4); // 切换页表(如果是用户进程)
        }
        switch_to(&prev->context, next->context); // 切换上下文
    }
    if(interrupts_enabled) {
        sti(); // 恢复中断
    }   
    return;
}

