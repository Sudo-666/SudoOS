#include "sche.h"
#include "../lib/list.h" 
#include "../arch/x86_64.h"

extern pcb_t* current_proc;
extern list_node_t ready_queue;
extern pcb_t* idle_proc;

void schedule() {
    // 1. 保存当前中断状态 (IF位) 并关闭中断
    // 防止调度过程中被新的中断打断，保证原子性
    uint64_t rflags = read_rflags();
    bool interrupts_enabled = rflags & (1 << 9); 
    cli(); 

    pcb_t* prev = current_proc;
    pcb_t* next = NULL;

    // 2. 处理当前进程 (prev)
    // 如果当前进程还在运行且不是 Idle 进程
    if(prev->proc_state == PROC_RUNNING && prev != idle_proc) {
        if(prev->time_slice <= 0) {
            // 时间片耗尽：变为就绪态，加入就绪队列尾部
            prev->proc_state = PROC_READY;
            prev->time_slice = TIME_SLICE_DEFAULT;
            list_add_before(&prev->sched_node, &ready_queue);
        } else {
            // 时间片未用完：直接返回，继续运行当前进程
            if(interrupts_enabled) sti();
            return;
        }
    }

    // 3. 从就绪队列选取下一个进程 (next)
    if(ready_queue.next == &ready_queue) {
        // 队列为空：调度 Idle 进程
        next = idle_proc;
        next->proc_state = PROC_RUNNING;
        next->time_slice = TIME_SLICE_DEFAULT;
    } else {
        // 队列不为空：取出队头进程
        list_node_t* next_node = ready_queue.next;
        list_del(next_node); // 从队列中移除
        next = container_of(next_node, pcb_t, sched_node);
    }

    // 4. 执行上下文切换
    if(prev != next) {
        next->proc_state = PROC_RUNNING;
        current_proc = next;
        
        // 重置时间片
        next->time_slice = TIME_SLICE_DEFAULT;

        // 更新 TSS 中的内核栈指针 (用于 Ring 3 -> Ring 0 的栈切换)
        set_tss_stack(next->rsp); 

        // 切换页表 (如果需要，通常用于用户进程)
        if(next->mm != NULL && (prev->mm == NULL || prev->mm != next->mm)) {
            lcr3((uintptr_t)next->mm->pml4);
        }

        // 汇编级上下文切换
        switch_to(&prev->context, next->context);
    }

    // 5. 恢复中断状态
    // 如果进入 schedule 前是开中断的，现在恢复开中断
    if(interrupts_enabled) {
        sti(); 
    }   
}