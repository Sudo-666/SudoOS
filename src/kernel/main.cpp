#include "../limine.h"
#include "drivers/console.h"
#include "drivers/keyboard.h"
#include "arch/idt.h"

// 声明用户程序的入口 (在 shell.cpp 里)
void user_main();

__attribute__((used, section(".requests")))
static volatile struct limine_framebuffer_request framebuffer_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST, .revision = 0
};

extern "C" void _start(void) {
    if (!framebuffer_request.response || framebuffer_request.response->framebuffer_count < 1) while(1);

    // 1. 内核初始化 (搭建舞台)
    console_init(framebuffer_request.response->framebuffers[0]);
    kprintln("[Kernel] Console ready.");
    
    // 2. 初始化 IDT (建立银行窗口)
    idt_init();
    kprintln("[Kernel] IDT/Syscall ready.");

    // 3. 转交控制权给用户程序
    kprintln("[Kernel] Jumping to User Space...");
    
    user_main(); // 进入 shell.cpp

    // 如果用户程序退出了 (虽然 shell 不会退出)，内核在这里兜底
    while(1) __asm__("hlt");
}