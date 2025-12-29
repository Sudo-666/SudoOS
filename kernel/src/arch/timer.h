#pragma once

#include <stdint.h>
#include "idt.h" // 为了引用 register_interrupt_handler
#include "../drivers/drivers.h"
#include "../proc/sche.h"

// 端口定义
#define PIT_CMD_PORT 0x43
#define PIT_CH0_PORT 0x40
#define PIT_BASE_FREQ 1193180

void timer_callback(registers_t* regs);
void init_timer(uint32_t frequency);
void sleep(uint32_t ms);