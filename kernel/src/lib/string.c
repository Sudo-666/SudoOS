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

char *itoa_uint64(uint64_t value, char *buffer, uint64_t base){
    if (base < 2 || base > 16) {
        buffer[0] = '\0';
        return buffer;
    }

    static const char digits[] = "0123456789ABCDEF";
    char temp[64];
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

char *strtok_r(char *str, const char *delim, char **saveptr) {
    char *token;

    if (str == NULL) {
        str = *saveptr;
    }

    // 1. 跳过开头的分隔符
    while (*str) {
        int is_delim = 0;
        const char *d = delim;
        while (*d) {
            if (*str == *d) {
                is_delim = 1;
                break;
            }
            d++;
        }
        if (!is_delim) break;
        str++;
    }

    // 如果字符串结束，返回 NULL
    if (*str == '\0') {
        *saveptr = str;
        return NULL;
    }

    // 2. 找到 token 的起始位置
    token = str;

    // 3. 扫描直到遇到下一个分隔符
    while (*str) {
        int is_delim = 0;
        const char *d = delim;
        while (*d) {
            if (*str == *d) {
                is_delim = 1;
                break;
            }
            d++;
        }

        if (is_delim) {
            *str = '\0';      // 将分隔符替换为结束符
            *saveptr = str + 1; // 保存下一次扫描的起始位置
            return token;
        }
        str++;
    }

    // 字符串结束，保存位置并返回最后一个 token
    *saveptr = str;
    return token;
}