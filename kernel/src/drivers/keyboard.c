#include "keyboard.h"
#include "io.h"
#include "console.h"     // <--- 必须加，否则找不到 console_scroll
#include "../arch/idt.h" // <--- 必须加，否则找不到 registers_t 和 register_interrupt_handler
#include "../lib/std.h"
#include <stdint.h>
#include <stdbool.h>

// === 键盘环形缓冲区 ===
#define KBUF_SIZE 128
static char kbuf[KBUF_SIZE];
static int r_ptr = 0;
static int w_ptr = 0;

// Shift 键状态
static bool g_shift = false;

// 扫描码集 (Set 1)
static const char kmap_low[128] = {
    0, 27, '1','2','3','4','5','6','7','8','9','0','-','=','\b','\t',
    'q','w','e','r','t','y','u','i','o','p','[',']','\n',0,'a','s',
    'd','f','g','h','j','k','l',';','\'','`',0,'\\','z','x','c','v',
    'b','n','m',',','.','/',0,'*',0,' ',0,0,0,0,0,0,0,0,0,0,0,0,0
};

static const char kmap_up[128] = {
    0, 27, '!','@','#','$','%','^','&','*','(',')','_','+','\b','\t',
    'Q','W','E','R','T','Y','U','I','O','P','{','}','\n',0,'A','S',
    'D','F','G','H','J','K','L',':','\"','~',0,'|','Z','X','C','V',
    'B','N','M','<','>','?',0,'*',0,' ',0,0,0,0,0,0,0,0,0,0,0,0,0
};

// 键盘中断处理函数
void keyboard_handler(registers_t* regs) {
    (void)regs; // 防止 unused parameter 警告

    // 1. 读取扫描码
    uint8_t scancode = inb(0x60);

    // 2. 处理 Shift 键
    if (scancode == 0x2A || scancode == 0x36) { g_shift = true; return; }
    if (scancode == 0xAA || scancode == 0xB6) { g_shift = false; return; }

    // 3. 忽略断开码
    if (scancode & 0x80) return;

    // === 翻页控制 ===
    // PageUp (0x49) 或 '[' (0x1A) -> 上翻
    if (scancode == 0x49 || scancode == 0x1A) {
        console_scroll(10); 
        return;
    }
    // PageDown (0x51) 或 ']' (0x1B) -> 下翻
    if (scancode == 0x51 || scancode == 0x1B) {
        console_scroll(-10); 
        return;
    }

    // 4. 普通字符
    if (scancode > 127) return;
    char c = g_shift ? kmap_up[scancode] : kmap_low[scancode];
    
    if (c != 0) {
        int next_w = (w_ptr + 1) % KBUF_SIZE;
        if (next_w != r_ptr) {
            kbuf[w_ptr] = c;
            w_ptr = next_w;
        }
    }
}

// 初始化键盘
void init_keyboard() {
    // 注册到 IRQ 1 (INT 33)
    register_interrupt_handler(33, keyboard_handler);

    // 开启 PIC 的 IRQ 1
    uint8_t mask = inb(0x21);
    outb(0x21, mask & ~(1 << 1));

    kprintln("Keyboard initialized (Interrupt Mode).");
}

// 获取字符 (非阻塞)
char keyboard_get_char() {
    if (r_ptr == w_ptr) return 0;
    char c = kbuf[r_ptr];
    r_ptr = (r_ptr + 1) % KBUF_SIZE;
    return c;
}

// 阻塞式输入
void kinput(char* buffer, int max_len) {
    int idx = 0;
    while(idx < max_len - 1) {
        char c = keyboard_get_char();
        if (c == 0) {
            __asm__("hlt");
            continue;
        }
        if (c == '\n') {
            kprint_char('\n');
            buffer[idx] = 0;
            return;
        } else if (c == '\b') {
            if (idx > 0) {
                idx--;
                kprint_char('\b');
            }
        } else {
            buffer[idx++] = c;
            kprint_char(c);
        }
    }
    buffer[idx] = 0;
}