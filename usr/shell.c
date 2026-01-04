#include "lib/syscall.h"
#include "lib/stdio.h"
#include "lib/string.h"
#include "lib/stdlib.h"

#define MAX_CMD_LEN 256
#define MAX_ARGS 16
#define BUF_SIZE 1024



// === 辅助工具：裸系统调用触发器 ===
// 用于 run 命令，绕过 C 库封装，直接测试内核响应
static inline int64_t raw_syscall(uint64_t n, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5) {
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

// 交互式读取一行
void getline(char* buf, int max_len) {
    int i = 0;
    while (i < max_len - 1) {
        char c = 0;
        if (read(0, &c, 1) > 0) {
            if (c == '\n' || c == '\r') {
                printf("\n");
                break;
            } else if (c == '\b' || c == 127) { // Backspace
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

// 交互式获取参数
long prompt_int(const char* prompt) {
    char buf[64];
    printf("    -> %s", prompt);
    getline(buf, 64);
    return atoi(buf);
}

void prompt_str(const char* prompt, char* buf, int max_len) {
    printf("    -> %s", prompt);
    getline(buf, max_len);
}

// === 命令实现 ===

void cmd_help() {
    printf("\n=== SudoOS Shell Help ===\n");
    printf("Filesystem:\n");
    printf("  ls [path]       List directory contents\n");
    printf("  cd <path>       Change working directory\n");
    printf("  mkdir <path>    Create a new directory\n");
    printf("  pwd             Print current working directory\n");
    printf("  touch <file>    Create a new empty file\n");
    printf("  cat <file>      Display file content\n");
    printf("  echo <text>     Print text (use > to write to file)\n");
    
    printf("\nSystem:\n");
    printf("  clear           Clear the screen\n");
    printf("  run <id>        Interactive Syscall Runner\n");
    printf("  exit            Exit the shell\n");

    printf("\n[Debug] Syscall Table (ID : Name):\n");
    printf("   0 : READ          1 : WRITE         2 : OPEN\n");
    printf("   3 : CLOSE         4 : STAT          5 : FSTAT\n");
    printf("   8 : LSEEK         9 : MMAP         11 : MUNMAP\n");
    printf("  12 : BRK          24 : YIELD        35 : NANOSLEEP\n");
    printf("  39 : GETPID       57 : FORK         59 : EXECVE\n");
    printf("  60 : EXIT         61 : WAIT4        79 : GETCWD\n");
    printf("  80 : CHDIR        83 : MKDIR        96 : GETTIMEOFDAY\n");
    printf(" 110 : GETPPID     217 : GETDENTS64\n");
}

void cmd_ls(char* path) {
    if (!path) path = "."; // 默认当前目录
    
    int fd = open(path, 0, 0); // O_RDONLY
    if (fd < 0) {
        printf("ls: cannot access '%s': No such file or directory\n", path);
        return;
    }
    
    char buf[BUF_SIZE];
    int nread, bpos;
    struct linux_dirent64 *d;
    int count = 0;

    while ((nread = getdents64(fd, buf, BUF_SIZE)) > 0) {
        for (bpos = 0; bpos < nread;) {
            d = (struct linux_dirent64 *)(buf + bpos);
            
            if (d->d_type == 4) { // DT_DIR
                printf("[%s]  ", d->d_name);
            } else {
                printf("%s  ", d->d_name);
            }
            count++;
            bpos += d->d_reclen;
        }
    }
    if (count > 0) printf("\n");
    close(fd);
}

void cmd_cd(char* path) {
    if (!path) {
        path = "/usr"; 
    }
    if (chdir(path) < 0) {
        printf("cd: no such file or directory: %s\n", path);
    }
}

void cmd_run(char* arg_id) {
    if (!arg_id) { printf("Usage: run <syscall_id>\n"); return; }
    int id = atoi(arg_id);
    long ret = 0;
    
    printf("--- Interactive Syscall Run [ID: %d] ---\n", id);

    switch(id) {
        case SYS_READ: 
        {
            int fd = prompt_int("File Descriptor (0 for stdin): ");
            int len = prompt_int("Bytes to read: ");
            char* buf = (char*)malloc(len + 1);
            if (!buf) { printf("Error: malloc failed\n"); return; }
            
            printf("    [Executing read...]\n");
            ret = raw_syscall(SYS_READ, fd, (uint64_t)buf, len, 0, 0);
            
            if (ret >= 0) {
                buf[ret] = 0;
                printf("    Result: %d bytes\n    Data: \"%s\"\n", ret, buf);
            } else {
                printf("    Error: %d\n", ret);
            }
            free(buf);
            break;
        }
        case SYS_WRITE:
        {
            int fd = prompt_int("File Descriptor (1 for stdout): ");
            char buf[128];
            prompt_str("Content to write: ", buf, 128);
            ret = raw_syscall(SYS_WRITE, fd, (uint64_t)buf, strlen(buf), 0, 0);
            printf("    Result: %d\n", ret);
            break;
        }
        case SYS_OPEN:
        {
            char path[64];
            prompt_str("Path: ", path, 64);
            int flags = prompt_int("Flags (0=RD, 64=CREAT): ");
            ret = raw_syscall(SYS_OPEN, (uint64_t)path, flags, 0, 0, 0);
            printf("    Result (New FD): %d\n", ret);
            break;
        }
        case SYS_GETPID:
            ret = raw_syscall(SYS_GETPID, 0,0,0,0,0);
            printf("    My PID: %d\n", ret);
            break;
        case SYS_MKDIR:
        {
            char path[64];
            prompt_str("Directory Name: ", path, 64);
            ret = raw_syscall(SYS_MKDIR, (uint64_t)path, 0755, 0, 0, 0);
            printf("    Result: %d\n", ret);
            break;
        }
        case SYS_CHDIR:
        {
            char path[64];
            prompt_str("Target Path: ", path, 64);
            ret = raw_syscall(SYS_CHDIR, (uint64_t)path, 0, 0, 0, 0);
            if (ret == 0) printf("    Success.\n");
            else printf("    Failed (ret=%d)\n", ret);
            break;
        }
        case SYS_GETCWD:
        {
            char buf[128];
            ret = raw_syscall(SYS_GETCWD, (uint64_t)buf, 128, 0, 0, 0);
            if (ret != 0) printf("    CWD: %s\n", (char*)ret); 
            else printf("    Error (ret=0)\n");
            break;
        }
        case SYS_BRK:
        {
            uint64_t addr = prompt_int("New Break Address (0 for current): ");
            ret = raw_syscall(SYS_BRK, addr, 0, 0, 0, 0);
            printf("    Current Break: 0x%x\n", ret);
            break;
        }
        case SYS_EXIT:
            ret = prompt_int("Exit Code: ");
            exit(ret); 
            break;
        default:
            printf("    [Generic Mode] Executing syscall %d with 0 arguments...\n", id);
            ret = raw_syscall(id, 0, 0, 0, 0, 0);
            printf("    Result: %d\n", ret);
            break;
    }
}

// === 主程序入口 ===

void shell_main() {
    char input[MAX_CMD_LEN];
    char* args[MAX_ARGS];
    char cwd_buf[128];

    printf("\nWelcome to SudoOS Shell v3.1\n");
    printf("Type 'help' for available commands.\n");

    if (chdir("/usr") < 0) {
        printf("Warning: Could not cd to /usr. Falling back to root.\n");
        chdir("/");
    }

    while (1) {
        // 动态提示符
        if (getcwd(cwd_buf, 128)) {
            printf("root@SudoOS:%s$ ", cwd_buf);
        } else {
            printf("root@SudoOS:?$ ");
        }

        getline(input, MAX_CMD_LEN);
        if (strlen(input) == 0) continue;

        // 解析参数
        int argc = 0;
        char* token = strtok(input, " ");
        while (token && argc < MAX_ARGS) {
            args[argc++] = token;
            token = strtok(NULL, " ");
        }
        if (argc == 0) continue;

        // 命令分发
        if (strcmp(args[0], "help") == 0) cmd_help();
        else if (strcmp(args[0], "ls") == 0) cmd_ls(args[1]);
        else if (strcmp(args[0], "cd") == 0) cmd_cd(args[1]);
        else if (strcmp(args[0], "pwd") == 0) {
            if (getcwd(cwd_buf, 128)) printf("%s\n", cwd_buf);
        }
        else if (strcmp(args[0], "mkdir") == 0) {
            if (mkdir(args[1], 0755) < 0) printf("mkdir failed.\n");
        }
        else if (strcmp(args[0], "cat") == 0) {
            int fd = open(args[1], 0, 0);
            if (fd >= 0) {
                char buf[128];
                int n;
                while((n = read(fd, buf, 127)) > 0) { buf[n]=0; printf("%s", buf); }
                printf("\n");
                close(fd);
            } else {
                printf("File not found.\n");
            }
        }
        else if (strcmp(args[0], "touch") == 0) {
            int fd = open(args[1], 64, 0); // 假设 64 是 O_CREAT
            if (fd >= 0) close(fd);
        }
        else if (strcmp(args[0], "echo") == 0) {
             if (argc >= 4 && strcmp(args[2], ">") == 0) {
                 int fd = open(args[3], 64, 0);
                 if (fd >= 0) {
                     write(fd, args[1], strlen(args[1]));
                     write(fd, "\n", 1);
                     close(fd);
                 }
             } else {
                 printf("%s\n", args[1] ? args[1] : "");
             }
        }
        else if (strcmp(args[0], "clear") == 0) {
             for(int i=0; i<30; i++) printf("\n");
        }
        else if (strcmp(args[0], "exit") == 0) exit(0);
        else if (strcmp(args[0], "run") == 0) cmd_run(args[1]);
        else printf("Unknown command: %s\n", args[0]);
    }
}