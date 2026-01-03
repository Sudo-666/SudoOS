#include "lib/syscall.h"
#include "lib/stdio.h"
#include "lib/string.h"
#include "lib/stdlib.h"

#define MAX_CMD_LEN 256
#define MAX_ARGS 16
#define BUF_SIZE 1024

// === 1. 补全系统调用定义 (适配现有系统) ===
// 都在 shell 内部定义，不动 syscall.h

// 缺失的系统调用号 (参考 Linux x86_64)
#define SYS_MKDIR 83

// 目录项结构 (必须与内核一致)
struct linux_dirent64 {
    uint64_t d_ino;
    int64_t  d_off;
    unsigned short d_reclen;
    unsigned char  d_type;
    char           d_name[];
};

// === 2. 本地系统调用触发器 ===
// 既然不能改库文件，我们就在这里手动实现一个，用于 run 指令和 mkdir
static inline int64_t my_syscall(uint64_t n, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5) {
    int64_t ret;
    register uint64_t r10 __asm__("r10") = a4;
    register uint64_t r8  __asm__("r8")  = a5;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(n), "D"(a1), "S"(a2), "d"(a3), "r"(r10), "r"(r8)
        : "memory"
    );
    return ret;
}

// === 3. 输入处理工具 ===

// 这种写法能防止键盘驱动吞字符，并支持回显
void getline(char* buf, int max_len) {
    int i = 0;
    while (i < max_len - 1) {
        char c = 0;
        // 阻塞读取 1 个字符
        if (read(0, &c, 1) > 0) {
            if (c == '\n') {
                printf("\n");
                break;
            } else if (c == '\b' || c == 127) { // 退格
                if (i > 0) {
                    i--;
                    printf("\b \b");
                }
            } else {
                buf[i++] = c;
                printf("%c", c);
            }
        }
    }
    buf[i] = 0;
}

// 交互式获取整数
long prompt_int(const char* prompt) {
    char buf[64];
    printf("%s", prompt);
    getline(buf, 64);
    return atoi(buf);
}

// 交互式获取字符串
void prompt_str(const char* prompt, char* buf, int max_len) {
    printf("%s", prompt);
    getline(buf, max_len);
}

// === 4. 命令实现 ===

void cmd_help() {
    printf("\n=== SudoOS Shell Help ===\n");
    printf("Commands:\n");
    printf("  ls, mkdir, touch, cat, echo, exec, exit, clear\n");
    printf("  run <id>   : Interactive system call runner\n");
    
    printf("\nSyscall IDs (for 'run'):\n");
    printf("  0:READ   1:WRITE  2:OPEN   3:CLOSE\n");
    printf("  4:STAT   5:FSTAT  12:BRK   24:YIELD\n");
    printf("  35:SLEEP 39:PID   57:FORK  59:EXECVE\n");
    printf("  60:EXIT  61:WAIT  79:CWD   80:CHDIR\n");
    printf("  83:MKDIR 217:GETDENTS\n");
}

void cmd_mkdir(char* name) {
    if (!name) { printf("Usage: mkdir <name>\n"); return; }
    // 使用本地定义的 syscall 调用 83 号中断
    long ret = my_syscall(SYS_MKDIR, (uint64_t)name, 0755, 0, 0, 0);
    if (ret < 0) printf("mkdir failed (ret=%ld)\n", ret);
    else printf("Directory created.\n");
}

void cmd_ls() {
    int fd = open(".", 0, 0);
    if (fd < 0) { printf("ls: cannot open .\n"); return; }
    
    char buf[BUF_SIZE];
    int nread;
    int bpos;
    struct linux_dirent64 *d;

    while (1) {
        nread = getdents64(fd, buf, BUF_SIZE);
        if (nread <= 0) break;

        for (bpos = 0; bpos < nread;) {
            d = (struct linux_dirent64 *)(buf + bpos);
            printf("%s  ", d->d_name);
            bpos += d->d_reclen;
        }
    }
    printf("\n");
    close(fd);
}

void cmd_cat(char* file) {
    if (!file) { printf("Usage: cat <file>\n"); return; }
    int fd = open(file, 0, 0);
    if (fd < 0) { printf("File not found.\n"); return; }
    
    char buf[129];
    int n;
    while ((n = read(fd, buf, 128)) > 0) {
        buf[n] = 0;
        printf("%s", buf);
    }
    printf("\n");
    close(fd);
}

void cmd_echo(char* text, char* file) {
    if (!text || !file) { printf("Usage: echo <txt> > <file>\n"); return; }
    int fd = open(file, 64, 0); // O_CREAT
    if (fd < 0) return;
    write(fd, text, strlen(text));
    write(fd, "\n", 1);
    close(fd);
}

// === 核心：交互式 Run 指令 ===
void cmd_run(char* arg_id) {
    if (!arg_id) { printf("Usage: run <syscall_id>\n"); return; }
    int id = atoi(arg_id);
    long ret = 0;
    
    printf("--- Interactive Syscall %d ---\n", id);

    switch(id) {
        case SYS_READ: // 0
        {
            int fd = prompt_int("fd (0=stdin): ");
            int len = prompt_int("count: ");
            char* buf = (char*)malloc(len + 1);
            printf("[Executing read...]\n");
            ret = read(fd, buf, len);
            if (ret >= 0) {
                buf[ret] = 0;
                printf("Result: %ld bytes\nData: %s\n", ret, buf);
            } else {
                printf("Error: %ld\n", ret);
            }
            free(buf);
            break;
        }
        case SYS_WRITE: // 1
        {
            int fd = prompt_int("fd (1=stdout): ");
            char buf[128];
            prompt_str("content: ", buf, 128);
            ret = write(fd, buf, strlen(buf));
            printf("Result: %ld\n", ret);
            break;
        }
        case SYS_OPEN: // 2
        {
            char path[64];
            prompt_str("path: ", path, 64);
            int flags = prompt_int("flags (0=RD, 64=CREAT): ");
            ret = open(path, flags, 0);
            printf("Result (fd): %ld\n", ret);
            break;
        }
        case SYS_MKDIR: // 83
        {
            char path[64];
            prompt_str("dirname: ", path, 64);
            ret = my_syscall(SYS_MKDIR, (uint64_t)path, 0755, 0, 0, 0);
            printf("Result: %ld\n", ret);
            break;
        }
        case SYS_BRK: // 12
        {
            int addr = prompt_int("addr (0 for current): ");
            ret = my_syscall(12, addr, 0, 0, 0, 0);
            printf("Result (new brk): %ld (0x%x)\n", ret, (int)ret);
            break;
        }
        case SYS_GETPID: // 39
            ret = getpid();
            printf("My PID: %ld\n", ret);
            break;
        case SYS_YIELD: // 24
            printf("Yielding...\n");
            sched_yield();
            printf("Back.\n");
            break;
        case SYS_EXIT: // 60
            ret = prompt_int("exit code: ");
            exit(ret);
            break;
        default:
            printf("Interactive mode not implemented for %d.\n", id);
            printf("Trying generic call with 0 args...\n");
            ret = my_syscall(id, 0, 0, 0, 0, 0);
            printf("Result: %ld\n", ret);
            break;
    }
}

// === 主循环 ===

void shell_main() {
    char input[MAX_CMD_LEN];
    char* args[MAX_ARGS];

    printf("\nWelcome to SudoOS Shell (Fix Mode)\n");
    printf("Type 'help' for commands.\n");

    while (1) {
        printf("root@SudoOS:%s$ ", "/"); // 简化处理，始终显示根目录
        getline(input, MAX_CMD_LEN);

        if (strlen(input) == 0) continue;

        // 分割参数
        int argc = 0;
        char* token = strtok(input, " ");
        while (token && argc < MAX_ARGS) {
            args[argc++] = token;
            token = strtok(NULL, " ");
        }
        if (argc == 0) continue;

        // 命令分发
        if (strcmp(args[0], "help") == 0) cmd_help();
        else if (strcmp(args[0], "ls") == 0) cmd_ls();
        else if (strcmp(args[0], "mkdir") == 0) cmd_mkdir(args[1]);
        else if (strcmp(args[0], "touch") == 0) {
            int fd = open(args[1], 64, 0);
            if (fd >= 0) close(fd);
        }
        else if (strcmp(args[0], "cat") == 0) cmd_cat(args[1]);
        else if (strcmp(args[0], "echo") == 0) {
            // 支持 echo text > file
            if (argc >= 4 && strcmp(args[2], ">") == 0) {
                cmd_echo(args[1], args[3]);
            } else {
                printf("%s\n", args[1]);
            }
        }
        else if (strcmp(args[0], "clear") == 0) {
             for(int i=0; i<30; i++) printf("\n");
        }
        else if (strcmp(args[0], "exit") == 0) exit(0);
        else if (strcmp(args[0], "run") == 0) cmd_run(args[1]);
        else printf("Unknown: %s\n", args[0]);
    }
}