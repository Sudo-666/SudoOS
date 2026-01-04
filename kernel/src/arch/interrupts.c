#include "idt.h"
#include "../lib/std.h"
#include "../drivers/drivers.h"
#include "../proc/proc.h"
#include "../proc/sche.h"
#include "../fs/ramfs.h"

isr_t interrupt_handlers[256];

extern pcb_t *current_proc;

void register_interrupt_handler(uint8_t n, isr_t handler)
{
    interrupt_handlers[n] = handler;
}

int do_execve(const char *path, const char *argv[], const char *envp[])
{
    // 1. 打开文件
    int fd = ramfs_open(path, 0);
    if (fd < 0)
        return -1; // 文件不存在

    // 2. 获取文件大小 (为了分配缓冲区读取整个文件)
    // 这里偷懒直接读，实际上应该分块读或 mmap
    struct
    {
        uint64_t size;
    } st; // 简易 stat 结构
    // 注意：ramfs_fstat 的第二个参数应该匹配你的 struct stat 定义
    // 这里假设 ramfs_fstat 能填充 size 字段
    ramfs_fstat(fd, &st);

    // 3. 读取 ELF 文件头和内容
    char *elf_buf = (char *)kmalloc(st.size);
    if (!elf_buf)
    {
        ramfs_close(fd);
        return -1;
    }

    int read_bytes = ramfs_read(fd, elf_buf, st.size);
    ramfs_close(fd);

    if (read_bytes != st.size)
    {
        kfree(elf_buf);
        return -1;
    }

    // 4. 加载 ELF (proc.c 中定义)
    uint64_t entry_point = load_elf(current_proc, elf_buf);
    kfree(elf_buf);

    if (entry_point == 0)
        return -1;

    // 5. 修改中断现场 (Trap Frame)
    // 当 syscall_handler 返回执行 iretq 时，CPU 会从栈上弹出 RIP/RSP
    // 我们修改栈上的值，让它"返回"到新程序的入口，而不是原来的地方
    if (current_proc->trap_frame)
    {
        current_proc->trap_frame->rip = entry_point;    // 新程序入口
        current_proc->trap_frame->rsp = USER_STACK_TOP; // 重置用户栈

        // 清空通用寄存器 (可选，为了安全)
        current_proc->trap_frame->rdi = 0; // argc (暂未实现参数传递)
        current_proc->trap_frame->rsi = 0; // argv
    }
    return 0;
}

void syscall_handler(registers_t *regs)
{
    // 1. 保存当前 TrapFrame (这对 fork/execve 至关重要)
    if (current_proc)
        current_proc->trap_frame = (trap_frame_t *)regs;

    uint64_t syscall_num = regs->rax; // 系统调用号
    uint64_t ret = -1;                // 默认返回值 (错误)

    // 参数别名，方便阅读
    uint64_t arg1 = regs->rdi;
    uint64_t arg2 = regs->rsi;
    uint64_t arg3 = regs->rdx;

    switch (syscall_num)
    {
    // ============================
    // 1. 基础 IO (0-3)
    // ============================
    case 0: // SYS_READ (fd, buf, count)
        if (arg1 == 0)
        { // STDIN (键盘)
            char *buf = (char *)arg2;
            char c = keyboard_get_char(); // 非阻塞读取
            if (c)
            {
                buf[0] = c;
                ret = 1;
            }
            else
            {
                ret = 0;
            }
        }
        else
        {
            ret = ramfs_read((int)arg1, (void *)arg2, (int)arg3);
        }
        break;

    case 1: // SYS_WRITE (fd, buf, count)
        if (arg1 == 1 || arg1 == 2)
        { // STDOUT/STDERR
            char *s = (char *)arg2;
            int len = (int)arg3;
            for (int i = 0; i < len; i++)
                kprint_char(s[i]);
            ret = len;
        }
        else
        {
            ret = ramfs_write((int)arg1, (void *)arg2, (int)arg3);
        }
        break;

    case 2: // SYS_OPEN (path, flags, mode)
        ret = ramfs_open((const char *)arg1, (int)arg2);
        break;

    case 3: // SYS_CLOSE (fd)
        ramfs_close((int)arg1);
        ret = 0;
        break;

    // ============================
    // 2. 文件系统增强 (4, 5, 8, 79, 80, 217)
    // ============================
    case 4: // SYS_STAT (path, stat_buf)
        ret = ramfs_stat((const char *)arg1, (void *)arg2);
        break;

    case 5: // SYS_FSTAT (fd, stat_buf)
        ret = ramfs_fstat((int)arg1, (void *)arg2);
        break;

    case 8: // SYS_LSEEK (fd, offset, whence)
    {
        // 这里为了简单，假设 ramfs_lseek 存在或者暂时返回错误
        // ret = ramfs_lseek((int)arg1, (long)arg2, (int)arg3);
        ret = -1; // 暂时未实现，防止崩溃
        break;
    }

    case 79: // SYS_GETCWD (buf, size)
    {
        char *buf = (char *)arg1;
        uint64_t size = arg2;

        // 检查参数有效性
        if (buf == NULL || size == 0)
        {
            ret = 0; // 失败返回 NULL (0)
        }
        else
        {
            char *res = ramfs_getcwd(buf, size);

            // 按照 Linux 惯例，成功返回 buf 指针，失败返回 0
            if (res != NULL)
                ret = (uint64_t)res;
            else
                ret = 0;
        }
        break;
    }

    case 80: // SYS_CHDIR (path)
    {
        const char *path = (const char *)arg1;
        if (path == NULL)
        {
            ret = -1; // -EFAULT
        }
        else
        {
            // 调用 ramfs_chdir，它会解析路径并更新 current_proc->cwd_inode
            // 成功返回 0，失败返回 -1
            ret = ramfs_chdir(path);
        }
        break;
    }

    case 83: // SYS_MKDIR (path, mode)
    {
        const char *path = (const char *)arg1;
        // arg2 (mode) 目前被 ramfs 忽略，默认为 0755
        if (path == NULL)
        {
            ret = -1;
        }
        else
        {
            // 调用 ramfs_mkdir，它会解析父目录并在其中创建新目录节点
            // 成功返回 0，失败返回 -1 (例如父目录不存在或目录已存在)
            ret = ramfs_mkdir(path);
        }
        break;
    }

    case 217: // SYS_GETDENTS64 (fd, dirp, count)
        int fd = (int)arg1;
        void *dirp = (void *)arg2;
        int count = (int)arg3;

        // 调用 ramfs 实现
        ret = ramfs_getdents64(fd, dirp, count);
        break;

    // ============================
    // 3. 内存管理 (9, 11, 12)
    // ============================
    case 9: // SYS_MMAP (addr, len, prot, flags, fd, offset)
    {
        // 极简实现：只支持匿名映射 (MAP_ANONYMOUS)，忽略 fd
        uint64_t len = arg2;
        uint64_t flags = regs->r10; // 第4个参数

        // 如果 addr 为 0，应该找一块空闲区域。
        // 这里为了最简单，我们直接让它失败，强制用户使用 malloc (通过 brk)
        // 或者：直接在堆顶上方映射一块 (非常危险，但能跑)
        ret = 0; // 0 表示失败
        break;
    }

    case 11: // SYS_MUNMAP (addr, len)
        // 暂不回收内存，直接返回成功
        ret = 0;
        break;

    case 12: // SYS_BRK (addr)
    {
        uint64_t new_brk = arg1;
        uint64_t cur_brk = current_proc->mm->heap;

        if (new_brk == 0)
        {
            ret = cur_brk; // 返回当前堆位置
        }
        else if (new_brk > cur_brk)
        {
            // 扩展堆：映射内存
            uint64_t size = new_brk - cur_brk;
            // 注意：需要确保 vmm.h 中有 VM_HEAP 等定义
            if (mm_map_range(current_proc->mm, cur_brk, size, VM_READ | VM_WRITE | VM_HEAP))
            {
                current_proc->mm->heap = new_brk;
                ret = new_brk;
            }
            else
            {
                ret = cur_brk; // 失败返回旧值
            }
        }
        else
        {
            ret = cur_brk; // 不支持收缩
        }
        break;
    }

    // ============================
    // 4. 进程管理 (24, 39, 57, 59, 60, 61, 110)
    // ============================
    case 24: // SYS_YIELD
        schedule();
        ret = 0;
        break;

    case 39: // SYS_GETPID
        ret = current_proc->pid;
        break;

    case 57: // SYS_FORK
        ret = sys_fork();
        break;

    case 59: // SYS_EXECVE (filename, argv, envp)
        ret = do_execve((const char *)arg1, (const char **)arg2, (const char **)arg3);
        break;

    case 60: // SYS_EXIT (error_code)
        current_proc->exit_code = (int)arg1;
        current_proc->proc_state = PROC_ZOMBIE;
        kprintf("Process %d exited with code %d\n", current_proc->pid, arg1);
        schedule(); // 切换进程，不再返回
        while (1)
            ; // 防御性代码
        break;

    case 61: // SYS_WAIT4 (pid, status, options, rusage)
        // 极简实现：直接返回 -1 (没有子进程需要回收)，让 shell 不阻塞
        // 完整实现需要遍历 proc_list 找子进程
        ret = -1;
        break;

    case 110: // SYS_GETPPID
        if (current_proc->parent)
            ret = current_proc->parent->pid;
        else
            ret = 0;
        break;

    // ============================
    // 5. 时间与休眠 (35, 96)
    // ============================
    case 35: // SYS_NANOSLEEP
        // 假装睡了
        // 实际应该调用 sleep(ms)
        ret = 0;
        break;

    case 96: // SYS_GETTIMEOFDAY
        // 返回 0
        ret = 0;
        break;

    default:
        kprintf("Warning: Unknown Syscall %d\n", syscall_num);
        ret = -1;
        break;
    }

    // 设置返回值 (放入 RAX 寄存器)
    regs->rax = ret;
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