#ifndef _STDIO_H
#define _STDIO_H

#include <stdarg.h>

void printf(const char *format, ...);
static void itoa(char *buf, int base, int d);
void putchar(char c);
void puts(const char *s);


#endif