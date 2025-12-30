
#include "timer.h"

volatile uint64_t ticks = 0; // 必须是 volatile，因为会在中断中修改

// 时钟中断处理函数 (ISR)
void timer_callback(registers_t* regs) {
    ticks++;

    extern pcb_t* current_proc;
    if(current_proc != NULL && current_proc->proc_state == PROC_RUNNING) {
        current_proc->total_runtime++;
        current_proc->time_slice--;
        if(current_proc->time_slice <= 0) {
            // 时间片用完，触发调度
            schedule();
        }
    }
    
    // 注意：EOI (End of Interrupt) 发送逻辑通常在 idt.c 的总 isr_handler 里做，
    // 这里不需要重复发送，除非你的架构设计不同。
}

void init_timer(uint32_t frequency) {
    // 注册中断处理函数
    register_interrupt_handler(32, &timer_callback);

    // 计算分频系数
    uint32_t divisor = PIT_BASE_FREQ / frequency;

    // 发送命令给 PIT
    outb(PIT_CMD_PORT, 0x36);

    // 拆分发送分频系数
    uint8_t low = (uint8_t)(divisor & 0xFF);
    uint8_t high = (uint8_t)((divisor >> 8) & 0xFF);

    outb(PIT_CH0_PORT, low);
    outb(PIT_CH0_PORT, high);

    // 读取主片当前的中断屏蔽寄存器 (IMR)
    uint8_t mask = inb(0x21);
    // 将第 0 位（对应 IRQ 0）置为 0（开启），保留其他位
    outb(0x21, mask & ~0x01); 
    // ===============================================
}

// 简单的延时函数 (忙等待)
void sleep(uint32_t ms) {
    uint64_t end = ticks + ms / 10; // 假设频率设为了 100Hz (10ms一次)
    while (ticks < end) {
        __asm__ volatile ("hlt"); // 让 CPU 休息一下，等待中断唤醒
    }
}