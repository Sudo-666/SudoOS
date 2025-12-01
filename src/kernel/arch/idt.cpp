#include "idt.h"
#include "trap.h"
#include "../drivers/console.h"
#include "../drivers/keyboard.h"

// 声明汇编里的入口符号
extern "C" void vector0();
extern "C" void vector1();
// ... 为了省事，我们这里只列出关键的，实际可以用数组填充 ...
extern "C" void vector13(); // GPF
extern "C" void vector14(); // Page Fault
extern "C" void vector128(); // Syscall 0x80

static struct IdtEntry idt[256];
static struct IdtPtr idtp;

void idt_set_gate(uint8_t num, uint64_t base, uint16_t sel, uint8_t flags) {
    idt[num].low = base & 0xFFFF;
    idt[num].mid = (base >> 16) & 0xFFFF;
    idt[num].high = (base >> 32) & 0xFFFFFFFF;
    idt[num].sel = sel;
    idt[num].ist = 0;
    idt[num].flags = flags;
    idt[num].zero = 0;
}

// ---------------------------------------------------------------------
// 核心：统一中断处理函数
// ---------------------------------------------------------------------
extern "C" void trap_handler(struct TrapFrame* tf) {
    switch (tf->trap_num) {
        case 14:
            kprintln("PANIC: Page Fault! (Memory Access Violation)");
            // 真实的 OS 这里要处理缺页，我们暂时死循环方便调试
            while(1) __asm__("hlt");
            break;
            
        case 13:
            kprintln("PANIC: General Protection Fault!");
            while(1) __asm__("hlt");
            break;
            
        case 0:
            kprintln("PANIC: Divide by Zero!");
            while(1) __asm__("hlt");
            break;

        case 0x80: // 系统调用
            // rax 保存了调用号
            if (tf->rax == 1) { 
                // sys_write(str)
                // rbx 保存了字符串地址
                const char* str = (const char*)tf->rbx;
                kprint(str);
            }
            else if (tf->rax == 2) { 
                // sys_read(buffer, len)
                // rbx = buffer, rcx = len
                char* buf = (char*)tf->rbx;
                int len = (int)tf->rcx;
                kinput(buf, len); // <--- 调用键盘驱动！
            }
            break;

        default:
            kprint("Unhandled Interrupt: ");
            kprint_int(tf->trap_num);
            kprint("\n");
            break;
    }
}

void idt_init() {
    idtp.limit = (sizeof(struct IdtEntry) * 256) - 1;
    idtp.base = (uint64_t)&idt;

    // 1. 注册异常处理 (0-31)
    // 这里偷个懒，只注册几个关键的，为了比赛你应该用循环把 vector0-31 都填上
    idt_set_gate(0, (uint64_t)vector0, 0x28, 0x8E);
    idt_set_gate(13, (uint64_t)vector13, 0x28, 0x8E);
    idt_set_gate(14, (uint64_t)vector14, 0x28, 0x8E);

    // 2. 注册系统调用 (0x80)
    // DPL=3 (0xEE = 11101110) 允许用户态调用
    idt_set_gate(0x80, (uint64_t)vector128, 0x28, 0xEE);

    __asm__ volatile ("lidt %0" : : "m"(idtp));
}