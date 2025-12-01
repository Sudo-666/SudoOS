#include "syscall.h"
#include "../lib/string.h"

void user_main() {
    sys_write("\n[User] Shell started. Type something!\n");
    
    char cmd_buffer[64];

    while(1) {
        // 1. 打印提示符
        sys_write("User@SudoOS $ ");
        
        // 2. 获取输入 (这就叫阻塞式系统调用)
        // 这一行代码执行时，内核会接管 CPU，直到你按下回车才会返回
        sys_read(cmd_buffer, 64);
        
        // 3. 简单的命令处理
        if (strcmp(cmd_buffer, "hello") == 0) {
            sys_write("Hello from User Space!\n");
        }
        else if (strcmp(cmd_buffer, "help") == 0) {
            sys_write("Commands: hello, help, exit\n");
        }
        else if (strlen(cmd_buffer) > 0) { // 防止回车空行也报错
            sys_write("Unknown command: ");
            sys_write(cmd_buffer);
            sys_write("\n");
        }
    }
}