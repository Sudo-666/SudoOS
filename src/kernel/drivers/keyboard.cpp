#include "keyboard.h"
#include "../arch/io.h"

// 静态映射表
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

static bool g_shift = false;

static char read_key_raw() {
    if ((inb(0x64) & 1) == 0) return 0;
    uint8_t code = inb(0x60);
    
    if (code == 0x2A || code == 0x36) { g_shift = true; return 0; }
    if (code == 0xAA || code == 0xB6) { g_shift = false; return 0; }
    if (code & 0x80) return 0; 
    
    if (code > 127) return 0;
    return g_shift ? kmap_up[code] : kmap_low[code];
}

void kinput(char* buffer, int max_len) {
    int idx = 0;
    while(1) {
        char c = read_key_raw();
        if (c == 0) continue;
        
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
            if (idx < max_len - 1) {
                buffer[idx++] = c;
                kprint_char(c);
            }
        }
    }
}