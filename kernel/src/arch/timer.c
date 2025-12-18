#include <stdint.h>
#include "idt.h" // 为了引用 register_interrupt_handler
#include "../drivers/drivers.h"

// 端口定义
#define PIT_CMD_PORT 0x43
#define PIT_CH0_PORT 0x40
#define PIT_BASE_FREQ 1193180

volatile uint64_t ticks = 0; // 必须是 volatile，因为会在中断中修改

// 时钟中断处理函数 (ISR)
void timer_callback(registers_t* regs) {
    ticks++;
    // 如果之后你要做进程调度，schedule() 就在这里调用
    // if (ticks % 10 == 0) schedule();
    
    // 注意：EOI (End of Interrupt) 发送逻辑通常在 idt.c 的总 isr_handler 里做，
    // 这里不需要重复发送，除非你的架构设计不同。
}

void init_timer(uint32_t frequency) {
    // 注册中断处理函数
    register_interrupt_handler(32, &timer_callback);

    // 计算分频系数 (Divisor)
    // 硬件基础频率 / 目标频率 = 分频数
    // 例如 100Hz -> 11931
    uint32_t divisor = PIT_BASE_FREQ / frequency;

    // 发送命令给 PIT
    // 0x36 = 00(Channel 0) 11(Access lobyte/hibyte) 011(Mode 3 Square Wave) 0(Binary)
    outb(PIT_CMD_PORT, 0x36);

    // 拆分发送分频系数 (先低8位，再高8位)
    uint8_t low = (uint8_t)(divisor & 0xFF);
    uint8_t high = (uint8_t)((divisor >> 8) & 0xFF);

    outb(PIT_CH0_PORT, low);
    outb(PIT_CH0_PORT, high);
}

// 简单的延时函数 (忙等待)
void sleep(uint32_t ms) {
    uint64_t end = ticks + ms / 10; // 假设频率设为了 100Hz (10ms一次)
    while (ticks < end) {
        __asm__ volatile ("hlt"); // 让 CPU 休息一下，等待中断唤醒
    }
}