#include "idt.h"
#include "../lib/std.h"
#include "../drivers/drivers.h"
#include "../proc/proc.h"
#include "../proc/sche.h"

isr_t interrupt_handlers[256];

void register_interrupt_handler(uint8_t n, isr_t handler)
{
    interrupt_handlers[n] = handler;
}

void syscall_handler(registers_t *regs)
{
    // 获取寄存器参数 (遵循 System V ABI 约定)
    uint64_t syscall_num = regs->rax; // 1. 号码
    uint64_t arg1 = regs->rdi;        // 2. 参数1
    uint64_t arg2 = regs->rsi;        // 3. 参数2
    uint64_t arg3 = regs->rdx;        // 4. 参数3

    switch (syscall_num)
    {
    // === SYS_READ (0) ===
    // 用户态调用: read(fd, buf, count)
    // 目标: 从键盘读取字符填入用户缓冲区
    case 0: 
    {
        int fd = (int)arg1;
        char *buf = (char *)arg2;
        int count = (int)arg3;

        // 目前只支持从标准输入 (STDIN = 0) 读取
        if (fd == 0) 
        {
            int i = 0;
            while(i < count) {
                char c = keyboard_get_char(); 
                if (c != 0) {
                    buf[i++] = c;
                    // 如果是回车，通常代表输入结束，可以提前返回
                    if (c == '\n') break;
                } else {
                     __asm__ volatile("pause");
                }
            }
            regs->rax = i; // 返回实际读取的字节数
        }
        else 
        {
            regs->rax = -1; // 不支持的文件描述符
        }
        break;
    }

    // === SYS_WRITE (1) ===
    // 用户态调用: write(fd, buf, count)
    case 1:
    {
        int fd = (int)arg1;
        char *user_str = (char *)arg2;
        int len = (int)arg3;

        // 支持标准输出(1) 和 标准错误(2)
        if (fd == 1 || fd == 2)
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

    // === SYS_GETPID (39) ===
    case 39:
    {
        extern pcb_t* current_proc;
        regs->rax = current_proc->pid;
        break;
    }

    // === SYS_YIELD (24) ===
    case 24:
    {
        schedule(); // 主动让出 CPU
        regs->rax = 0;
        break;
    }

    // === SYS_EXIT (60) ===
    case 60:
    {
        int status = (int)arg1;
        kprintf("[Kernel] Process exiting with status %d\n", status);
        
        // 标记为僵尸进程，不再调度
        extern pcb_t* current_proc;
        current_proc->proc_state = PROC_ZOMBIE;
        
        schedule(); // 永不返回，直接切走
        break;
    }

    // === 这里的 case 还没有实现，先打印日志方便调试 ===
    default:
        kprintf("[Kernel] Unimplemented Syscall: %d\n", syscall_num);
        regs->rax = -1;
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