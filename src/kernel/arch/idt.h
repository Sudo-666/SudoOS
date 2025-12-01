#pragma once
#include <stdint.h>

struct IdtEntry {
    uint16_t low; uint16_t sel; uint8_t ist; uint8_t flags;
    uint16_t mid; uint32_t high; uint32_t zero;
} __attribute__((packed));

struct IdtPtr { uint16_t limit; uint64_t base; } __attribute__((packed));

// 初始化 IDT
void idt_init();