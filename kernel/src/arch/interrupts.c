#include "idt.h"
#include "../lib/std.h"
#include "../drivers/drivers.h"

isr_t interrupt_handlers[256];

void register_interrupt_handler(uint8_t n, isr_t handler)
{
    interrupt_handlers[n] = handler;
}

void syscall_handler(registers_t *regs)
{
    // System V ABI 传参规则 (用户态 -> 内核态):
    // rax: 系统调用号
    // rdi: 参数1
    // rsi: 参数2
    // rdx: 参数3
    // ...

    uint64_t syscall_num = regs->rax; // 获取功能号

    switch (syscall_num)
    {
    case 1:
    {
        int fd = (int)regs->rdi;
        char *user_str = (char *)regs->rsi;
        int len = (int)regs->rdx;
        if (len < 0)
        {
            regs->rax = -1;
            break;
        }
        if (fd == 1)
        {
            for (int i = 0; i < len; i++)
            {
                kprintf("%c", user_str[i]);
            }
            regs->rax = len;
        }
        else
        {
            regs->rax = -1;
        }
        break;
    }

    default:
        kprintf("Unknown Syscall: %d\n", syscall_num);
        regs->rax = -1; // 返回错误码
        break;
    }
}

static const char *exception_messages[] = {
    "0: Divide-by-zero Error",
    "1: Debug Exception",
    "2: Non-maskable Interrupt",
    "3: Breakpoint",
    "4: Overflow",
    "5: Bound Range Exceeded",
    "6: Invalid Opcode",
    "7: Device Not Available",
    "8: Double Fault",
    "9: Coprocessor Segment Overrun",
    "10: Invalid TSS",
    "11: Segment Not Present",
    "12: Stack-Segment Fault",
    "13: General Protection Fault",
    "14: Page Fault",
    "15: Reserved",
    "16: x87 Floating-Point Exception",
    "17: Alignment Check",
    "18: Machine Check",
    "19: SIMD Floating-Point Exception",
    "20: Virtualization Exception",
    "21: Control Protection Exception",
    "22: Reserved",
    "23: Reserved",
    "24: Reserved",
    "25: Reserved",
    "26: Reserved",
    "27: Reserved",
    "28: Reserved",
    "29: Reserved",
    "30: Reserved",
    "31: Reserved",
};

// 汇编跳过来的总入口
void isr_handler(registers_t *regs)
{
    if (regs->int_no == 128)
    {
        syscall_handler(regs);
        return;
    }

    // 只有来自 PIC 8259A 的中断才需要这个
    // 2. 在调用具体的 handler 之前，先发送 EOI！
    // 这样即使 handler 内部触发了调度（切换了栈），PIC 也能收到 EOI，
    // 从而允许下一个时间片的中断进来。
    if (regs->int_no >= 32 && regs->int_no <= 47)
    {
        if (regs->int_no >= 40)
            outb(0xA0, 0x20); // 先发送从片 EOI（若来自从片）
        outb(0x20, 0x20);     // 再发送主片 EOI
    }
    if (interrupt_handlers[regs->int_no] != 0)
    {
        isr_t handler = interrupt_handlers[regs->int_no];
        handler(regs);
    }
    else
    {
        if (regs->int_no < 32)
        {
            const char *msg = "Unknown Exception";
            if (regs->int_no < sizeof(exception_messages) / sizeof(char *))
                msg = exception_messages[regs->int_no];
            kprintf("=== CPU EXCEPTION ===\n");
            kprintf("Exception: %s\n", msg);
            kprintf("Error Code: %lx\n", regs->err_code);
            kprintf("RIP: %lx\n", regs->rip);
            kprintf("=====================\n");
            while (1)
                hlt();
        }
    }
    
}