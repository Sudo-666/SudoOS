#include "string.h"

size_t strlen(const char *s)
{
    size_t n = 0;
    while (s[n])
        n++;
    return n;
}

int strcmp(const char *a, const char *b)
{
    while (*a && (*a == *b))
    {
        a++;
        b++;
    }
    return *(const unsigned char *)a - *(const unsigned char *)b;
}

int strncmp(const char *a, const char *b, size_t n)
{
    while (n-- && *a && (*a == *b))
    {
        a++;
        b++;
    }
    if ((int)n < 0)
        return 0;
    return *(const unsigned char *)a - *(const unsigned char *)b;
}

char *strcpy(char *dst, const char *src)
{
    char *r = dst;
    while ((*dst++ = *src++))
        ;
    return r;
}

char *strncpy(char *dst, const char *src, size_t n)
{
    char *r = dst;
    while (n && (*dst++ = *src++))
        n--;
    while (n--)
        *dst++ = '\0';
    return r;
}

void *memset(void *dst, int value, size_t n)
{
    unsigned char *p = dst;
    while (n--)
        *p++ = (unsigned char)value;
    return dst;
}

void *memcpy(void *dst, const void *src, size_t n)
{
    unsigned char *d = dst;
    const unsigned char *s = src;
    while (n--)
        *d++ = *s++;
    return dst;
}

int memcmp(const void *a, const void *b, size_t n)
{
    const unsigned char *x = a;
    const unsigned char *y = b;
    while (n--)
    {
        if (*x != *y)
            return *x - *y;
        x++;
        y++;
    }
    return 0;
}

char *itoa(int value, char *buffer, int base) {
    if (base < 2 || base > 16) {
        buffer[0] = '\0';
        return buffer;
    }

    static const char digits[] = "0123456789ABCDEF";
    char temp[32];
    int i = 0;
    int is_negative = 0;

    /* 处理10进制负数 */
    if (value < 0 && base == 10) {
        is_negative = 1;
        value = -value;
    }

    /* 处理 0 */
    if (value == 0) {
        buffer[0] = '0';
        buffer[1] = '\0';
        return buffer;
    }

    /* 转换为倒序 */
    while (value != 0) {
        int rem = value % base;
        temp[i++] = digits[rem];
        value /= base;
    }

    if (is_negative)
        temp[i++] = '-';

    /* 倒序复制回 buffer */
    int j = 0;
    while (i > 0) {
        buffer[j++] = temp[--i];
    }
    buffer[j] = '\0';

    return buffer;
}