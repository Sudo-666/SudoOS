// usr/usrTest.c

// 1. 前向声明 (让编译器知道有这个函数)
void print(const char* str);

// 2. 必须把 usrmain 放在最前面！
int usrmain() {
    print("Hello, User World!\n");
    // 必须死循环，防止函数返回
    while(1);
}

// 3. 将实现包含在最后 (防止 print 抢占入口位置)
#include "lib/syscall.c"