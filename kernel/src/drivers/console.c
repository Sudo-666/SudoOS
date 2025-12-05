#include "console.h"
#include "font.h" 
#include "../lib/string.h"

// 全局变量定义
static struct limine_framebuffer* g_fb = NULL;
static int g_x = 0;
static int g_y = 0;

// 内部辅助函数：画点
static void put_pixel(int x, int y, uint32_t color) {
    if (!g_fb) return;
    // 防止画出屏幕外导致崩溃
    if (x < 0 || x >= (int)g_fb->width || y < 0 || y >= (int)g_fb->height) return;

    uint32_t* ptr = (uint32_t*)g_fb->address;
    ptr[y * (g_fb->pitch / 4) + x] = color;
}

// 初始化
void console_init(struct limine_framebuffer* fb) {
    g_fb = fb;
    kclear();
}

// 清屏
void kclear() {
    if (!g_fb) return;
    for (uint64_t i = 0; i < g_fb->width * g_fb->height; i++) {
        ((uint32_t*)g_fb->address)[i] = 0x00000000; // 黑色背景
    }
    g_x = 0; 
    g_y = 0;
}

// 打印单个字符 (核心逻辑)
void kprint_char(char c) {
    // 1. 处理换行
    if (c == '\n') {
        g_x = 0; 
        g_y += 8;
        return;
    }

    // 2. 处理退格
    if (c == '\b') {
        if (g_x >= 8) {
            g_x -= 8;
            // 用黑色把原来的字涂掉
            for(int dy=0; dy<8; dy++) {
                for(int dx=0; dx<8; dx++) {
                    put_pixel(g_x+dx, g_y+dy, 0x000000);
                }
            }
        }
        return;
    }

    // 3. 过滤不可见字符 (除了换行和退格)
    // font8x8_basic 只有 128 个字符
    if ((unsigned char)c > 127) return; 

    // 4. 获取字模
    // 注意：下载的文件里数组名叫 font8x8_basic
    const uint8_t* bmp = font8x8_basic[(int)c];
    
    // 5. 逐点绘制
    for(int row=0; row<8; row++) {
        for(int col=0; col<8; col++) {
            // 检查那一位是不是 1
            if (bmp[row] & (1 << col)) {
                put_pixel(g_x+col, g_y+row, 0xFFFFFF); // 白色字体
            }
        }
    }

    // 6. 光标移动
    g_x += 8;
    // 自动换行
    if (g_x >= (int)g_fb->width) {
        g_x = 0; 
        g_y += 8;
    }
    // 简单的滚屏逻辑（防止写出屏幕底部）
    if (g_y + 8 >= (int)g_fb->height) {
        g_y = 0; // 暂时回到顶部，以后实现真正的滚屏
        kclear();
    }
}

// 打印字符串
void kprint(const char* str) {
    while (*str) kprint_char(*str++);
}

// 打印并换行
void kprintln(const char* str) {
    kprint(str);
    kprint_char('\n');
}

// 打印整数
void kprint_int(int val) {
    char buf[32];
    itoa(val, buf,10);
    kprint(buf);
}

void kprint_uint64(uint64_t val){
    char buf[64];
    itoa(val, buf,10);
    kprint(buf);
}