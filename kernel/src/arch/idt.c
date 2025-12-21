#include "idt.h"
#include "../drivers/drivers.h"

// IDT 条目结构
struct idt_entry_struct {
    uint16_t base_low;
    uint16_t selector;   // 内核代码段选择子
    uint8_t  ist;        // 中断栈表索引 (暂时写0)
    uint8_t  flags;      // 属性 (P | DPL | Type)
    uint16_t base_mid;
    uint32_t base_high;
    uint32_t reserved;
} __attribute__((packed));

struct idt_ptr_struct {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

struct idt_entry_struct idt_entries[256];
struct idt_ptr_struct   idt_ptr;

// 声明汇编中的入口
extern void isr0();
extern void isr1();
extern void isr2();
extern void isr3();
extern void isr4();
extern void isr5();
extern void isr6();
extern void isr7();
extern void isr8();
extern void isr9();
extern void isr10();
extern void isr11();
extern void isr12();
extern void isr13();
extern void isr14();
extern void isr15();
extern void isr16();
extern void isr17();
extern void isr18();
extern void isr19();
extern void isr20();
extern void isr21();
extern void isr22();
extern void isr23();
extern void isr24();
extern void isr25();
extern void isr26();
extern void isr27();
extern void isr28();
extern void isr29();
extern void isr30();
extern void isr31();
// === 32-47 号硬件中断 (IRQs) ===
extern void isr32(); // IRQ0: Timer
extern void isr33(); // IRQ1: Keyboard

// === 128 号系统调用 ===
extern void isr128(); // 0x80: Syscall

void idt_set_gate(uint8_t num, uint64_t base, uint16_t sel, uint8_t flags) {
    idt_entries[num].base_low = base & 0xFFFF;
    idt_entries[num].base_mid = (base >> 16) & 0xFFFF;
    idt_entries[num].base_high = (base >> 32) & 0xFFFFFFFF;
    idt_entries[num].selector = sel;
    idt_entries[num].ist = 0;
    idt_entries[num].flags = flags;
    idt_entries[num].reserved = 0;
}

// PIC 重映射
void pic_remap() {
    // 这里的魔术代码是标准的8259A初始化流程
    outb(0x20, 0x11); outb(0xA0, 0x11);
    outb(0x21, 0x20); // 主片映射到 0x20 (32)
    outb(0xA1, 0x28); // 从片映射到 0x28 (40)
    outb(0x21, 0x04); outb(0xA1, 0x02);
    outb(0x21, 0x01); outb(0xA1, 0x01);
    outb(0x21, 0x0);  outb(0xA1, 0x0); // 打开所有中断
}

void idt_init() {
    idt_ptr.limit = sizeof(struct idt_entry_struct) * 256 - 1;
    idt_ptr.base = (uint64_t)&idt_entries;

    // 1. 重新映射硬件中断
    pic_remap();

    // 2. 设置异常门 (0-31)
    // 0x8E = 1(Present) 00(Ring0) 0(S) 1110(Interrupt Gate)
    idt_set_gate(0, (uint64_t)isr0, 0x08, 0x8E); // 内核代码段是0x28(limine默认是这个)
    idt_set_gate(14, (uint64_t)isr14, 0x08, 0x8E);

    // 3. 设置硬件中断 (32-47)
    idt_set_gate(32, (uint64_t)isr32, 0x08, 0x8E); // 时钟
    idt_set_gate(33, (uint64_t)isr33, 0x08, 0x8E); // 键盘

    // 4. 设置系统调用 (128 / 0x80)
    // 0xEE = 1(Present) 11(Ring3) 0(S) 1110(Interrupt Gate)
    // 注意：系统调用必须允许Ring3进入，所以DPL=3 (0xEE)
    idt_set_gate(128, (uint64_t)isr128, 0x08, 0xEE);

    // 5. 加载IDT
    __asm__ volatile ("lidt %0" : : "m"(idt_ptr));
    __asm__ volatile ("sti"); // 开启中断！
}