#include "lib/syscall.c"


int usrmain() {
    write(1,"Error!\n", 7);
    // 必须死循环，防止函数返回
    while(1);
}
