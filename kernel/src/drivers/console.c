#include "console.h"
#include "font.h" 
#include "../lib/string.h"
#include <stdarg.h>

// === 配置参数 ===
#define SCALE 2             // 放大倍数 (16x16字体)
#define FONT_W 8
#define FONT_H 8
#define FG_COLOR 0xFFFFFFFF // 白色
#define BG_COLOR 0x00000000 // 黑色

// === 历史缓冲区 ===
// 根据你的内存情况，这里保留 100 行历史
#define MAX_HISTORY 100     
#define MAX_COLS 128        

static char history_buffer[MAX_HISTORY][MAX_COLS];
static struct limine_framebuffer* g_fb = NULL;

// 逻辑坐标 (在 buffer 中的位置)
static int g_cursor_x = 0;
static int g_cursor_y = 0; 

// 视觉参数
static int g_view_offset = 0; // 0 = 实时画面, >0 = 往回看了几行
static int g_screen_rows = 0; // 屏幕能显示多少行
static int g_screen_cols = 0; // 屏幕能显示多少列

// === 内部绘图函数 ===

// 画一个放大后的点
static void put_pixel_scaled(int x, int y, uint32_t color) {
    if (!g_fb) return;
    
    int start_x = x * SCALE;
    int start_y = y * SCALE;

    // 性能优化：直接操作内存，减少循环判断
    uint32_t* fb_addr = (uint32_t*)g_fb->address;
    uint32_t pitch_pixels = g_fb->pitch / 4;

    for (int dy = 0; dy < SCALE; dy++) {
        uint32_t* row_ptr = fb_addr + (start_y + dy) * pitch_pixels + start_x;
        for (int dx = 0; dx < SCALE; dx++) {
            // 简单边界检查，防止画出屏幕崩溃
            if ((start_x + dx) < (int)g_fb->width && (start_y + dy) < (int)g_fb->height) {
                row_ptr[dx] = color;
            }
        }
    }
}

// 在屏幕指定行列画一个字符
// row, col 是屏幕上的坐标，不是 buffer 的
static void draw_char_on_screen(int col, int row, char c) {
    if ((unsigned char)c > 127) return;
    
    // 清除背景 (画黑色方块)
    // 实际应用中可以优化，这里为了简单直接覆盖
    
    const uint8_t* bmp = font8x8_basic[(int)c];
    
    int pixel_base_x = col * FONT_W;
    int pixel_base_y = row * FONT_H;

    for(int y=0; y<8; y++) {
        for(int x=0; x<8; x++) {
            // 检查字模位
            int is_set = bmp[y] & (1 << x);
            uint32_t color = is_set ? FG_COLOR : BG_COLOR;
            put_pixel_scaled(pixel_base_x + x, pixel_base_y + y, color);
        }
    }
}

// 全屏刷新 (最耗时，仅在翻页或严重滚屏时调用)
void console_refresh() {
    if (!g_fb) return;

    // 清屏 (或者重绘背景)
    // 既然我们要重绘所有字，背景会被 draw_char_on_screen 的黑色覆盖，
    // 所以不需要显式 kclear，除非行尾有残留。为了干净，建议先全黑。
    // 这里为了性能，只在换行时清空新的一行，全屏刷新时不强求清屏。

    // 计算 buffer 中哪一行对应屏幕的第一行
    // 公式：当前行 - (屏幕高度-1) - 偏移量
    int screen_top_row_idx = g_cursor_y - (g_screen_rows - 1) - g_view_offset;
    
    // 边界修正
    if (screen_top_row_idx < 0) screen_top_row_idx = 0;

    for (int i = 0; i < g_screen_rows; i++) {
        int buf_row = screen_top_row_idx + i;
        
        // 这一行超出 buffer 有效范围了吗？
        if (buf_row > g_cursor_y || buf_row >= MAX_HISTORY) {
            // 画空行 (清空屏幕下半部分残留)
            for(int j=0; j<g_screen_cols; j++) draw_char_on_screen(j, i, ' ');
            continue;
        }

        // 绘制这一行
        for (int j = 0; j < g_screen_cols; j++) {
            char c = history_buffer[buf_row][j];
            if (c == 0) c = ' ';
            draw_char_on_screen(j, i, c);
        }
    }
}

// === 公开接口 ===

void console_init(struct limine_framebuffer* fb) {
    g_fb = fb;
    
    // 计算屏幕容量
    g_screen_cols = g_fb->width / (FONT_W * SCALE);
    g_screen_rows = g_fb->height / (FONT_H * SCALE);
    
    if (g_screen_cols > MAX_COLS) g_screen_cols = MAX_COLS;

    // 初始化状态
    memset(history_buffer, 0, sizeof(history_buffer));
    g_cursor_x = 0;
    g_cursor_y = 0;
    g_view_offset = 0;

    kclear();
}

void kclear() {
    if (!g_fb) return;
    // 暴力填黑
    uint32_t* ptr = (uint32_t*)g_fb->address;
    uint64_t count = g_fb->width * g_fb->height;
    for (uint64_t i = 0; i < count; i++) {
        ptr[i] = BG_COLOR;
    }
}

void console_scroll(int lines) {
    g_view_offset += lines;
    
    // 限制范围
    int max_back = g_cursor_y; // 最多能回滚到开头
    if (g_view_offset > max_back) g_view_offset = max_back;
    if (g_view_offset < 0) g_view_offset = 0;
    
    // 只有翻页时才全屏刷新！
    console_refresh();
}

void kprint_char(char c) {
    // 1. 处理特殊字符
    if (c == '\n') {
        g_cursor_x = 0;
        g_cursor_y++;
        g_view_offset = 0; // 强制回到最新
        
        // 性能关键：只有当这一行写满导致屏幕滚动时，才可能需要重绘
        // 但为了简单，如果此时在屏幕底部，我们还是需要重绘或者滚屏
        // 为了避免全屏重绘，通常做法是 memcpy 显存上移，然后清空最后一行。
        // 这里暂时用 refresh，因为换行频率远低于打字频率。
        if (g_cursor_y >= g_screen_rows) {
             console_refresh(); 
        }
        return;
    } 
    
    if (c == '\b') {
        if (g_cursor_x > 0) {
            g_cursor_x--;
            history_buffer[g_cursor_y][g_cursor_x] = ' '; // 清除 Buffer
            // 如果正在看最新，立即擦除屏幕
            if (g_view_offset == 0) {
                // 计算当前行在屏幕上的位置
                int screen_y = g_cursor_y; 
                if (g_cursor_y >= g_screen_rows) {
                    screen_y = g_screen_rows - 1;
                }
                draw_char_on_screen(g_cursor_x, screen_y, ' ');
            }
        }
        return;
    }

    // 2. 写入 Buffer
    if (c >= ' ' && c <= '~') {
        history_buffer[g_cursor_y][g_cursor_x] = c;
        
        // 3. 性能关键：增量绘制！
        // 只有当用户正在看最新内容（没翻页）时，直接把字画在屏幕上
        // 否则只写 Buffer，不画图
        if (g_view_offset == 0) {
            // 计算屏幕 Y 坐标
            // 如果总行数还没填满屏幕，屏幕Y = cursor_y
            // 如果已经填满滚动了，屏幕Y = 屏幕底部 (rows-1)
            int screen_y = g_cursor_y;
            if (g_cursor_y >= g_screen_rows) {
                screen_y = g_screen_rows - 1;
            }
            
            // 直接画！
            draw_char_on_screen(g_cursor_x, screen_y, c);
        }

        // 4. 光标移动
        g_cursor_x++;
        if (g_cursor_x >= g_screen_cols) {
            // 自动换行
            g_cursor_x = 0;
            g_cursor_y++;
            if (g_cursor_y >= g_screen_rows) {
                console_refresh(); // 换行导致滚屏，重绘
            }
        }
    }

    // 5. Buffer 循环/溢出处理
    if (g_cursor_y >= MAX_HISTORY) {
        // 满了，整体挪动 (这是最慢的情况，但在 100 行才发生一次)
        memcpy(history_buffer[0], history_buffer[1], (MAX_HISTORY - 1) * MAX_COLS);
        memset(history_buffer[MAX_HISTORY - 1], 0, MAX_COLS);
        g_cursor_y = MAX_HISTORY - 1;
        // 挪完 buffer 后必须重绘
        console_refresh();
    }
}

// === 你的 kprintf (保持原样) ===
void kprint(const char* str) { while (*str) kprint_char(*str++); }
void kprintln(const char* str) { kprint(str); kprint_char('\n'); }

void kprint_int(int val) {
    char buf[32];
    itoa(val, buf, 10);
    kprint(buf);
}
void kprint_uint64(uint64_t val) {
    char buf[64];
    itoa_uint64(val, buf, 10);
    kprint(buf);
}
void kprint_hex(uint64_t n) {
    const char *digits = "0123456789ABCDEF";
    char buffer[17];
    int i = 0;
    if (n == 0) { kprint("0"); return; }
    while (n > 0) { buffer[i++] = digits[n % 16]; n /= 16; }
    for (int j = i - 1; j >= 0; j--) { kprint_char(buffer[j]); }
}

void kprintf(const char* format, ...) {
    va_list args;
    va_start(args, format);
    for (const char* p = format; *p != '\0'; p++) {
        if (*p != '%') { kprint_char(*p); continue; }
        p++;
        switch (*p) {
            case 'd': case 'i': case 'u': { int val = va_arg(args, int); kprint_int(val); break; }
            case 'c': { int val = va_arg(args, int); kprint_char((char)val); break; }
            case 's': { const char* val = va_arg(args, const char*); if (!val) kprint("(null)"); else kprint(val); break; }
            case 'x': case 'p': { uint64_t val = va_arg(args, uint64_t); if (*p == 'p') kprint("0x"); kprint_hex(val); break; }
            case 'l': { p++; if (*p == 'u' || *p == 'd') kprint_uint64(va_arg(args, uint64_t)); else if (*p == 'x') kprint_hex(va_arg(args, uint64_t)); break; }
            case '%': kprint_char('%'); break;
            default: kprint_char('%'); kprint_char(*p); break;
        }
    }
    va_end(args);
}