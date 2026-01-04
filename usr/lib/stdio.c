#include "syscall.h"
#include "stdio.h"
#include <stdarg.h>
#include <stddef.h>

// 辅助：整数转字符串
static void itoa(char *buf, int base, int d) {
    char *p = buf;
    char *p1, *p2;
    unsigned long ud = d;
    int divisor = 10;

    // 如果是负数且是十进制
    if (base == 10 && d < 0) {
        *p++ = '-';
        buf++;
        ud = -d;
    } else if (base == 16) {
        divisor = 16;
    }

    // 取余数转换
    do {
        int remainder = ud % divisor;
        *p++ = (remainder < 10) ? remainder + '0' : remainder + 'a' - 10;
    } while (ud /= divisor);

    // 添加结束符
    *p = 0;

    // 翻转字符串
    p1 = buf;
    p2 = p - 1;
    while (p1 < p2) {
        char tmp = *p1;
        *p1 = *p2;
        *p2 = tmp;
        p1++;
        p2--;
    }
}

// 打印单个字符
void putchar(char c) {
    write(1, &c, 1);
}

// 打印字符串
void puts(const char *s) {
    int len = 0;
    const char *tmp = s;
    while(*tmp++) len++; // 简单的 strlen
    write(1, s, len);
    putchar('\n');
}

// 核心 printf 实现

void printf(const char *format, ...) {

va_list ap;

va_start(ap, format);



char buf[32]; // 临时缓冲区



while (*format) {

if (*format != '%') {

putchar(*format);

format++;

continue;

}



format++; // 跳过 %

switch (*format) {

case 'c': {

// char 在 va_arg 里会提升为 int

char c = (char)va_arg(ap, int);

putchar(c);

break;

}

case 's': {

const char *s = va_arg(ap, const char *);

if (!s) s = "(null)";

while (*s) putchar(*s++);

break;

}

case 'd': {

int d = va_arg(ap, int);

itoa(buf, 10, d);

char *s = buf;

while (*s) putchar(*s++);

break;

}

case 'x': {

int d = va_arg(ap, int);

itoa(buf, 16, d);

char *s = buf;

while (*s) putchar(*s++);

break;

}

default:

putchar('%');

putchar(*format);

break;

}

format++;

}



va_end(ap);

}