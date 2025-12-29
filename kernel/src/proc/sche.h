#pragma once

#include "../arch/x86_64.h"
#include "../arch/trap.h"
#include "../drivers/console.h"
#include "proc.h"
#include "../arch/gdt.h"
#include "../arch/switch.h"

#define TIME_SLICE_DEFAULT 10 // 默认时间片长度，单位：tick
void schedule();