#include <stdint.h>
#include <stddef.h>
#include "string.h"

// 计算字符串长度
size_t strlen(const char *str) {
    size_t len = 0;
    while (str[len])
        len++;
    return len;
}

// 字符串比较
int strcmp(const char *s1, const char *s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(const unsigned char *)s1 - *(const unsigned char *)s2;
}

// 字符串拷贝
char *strcpy(char *dest, const char *src) {
    char *d = dest;
    while ((*d++ = *src++));
    return dest;
}

void *memset(void *s, int c, size_t n) {
    unsigned char *p = s;
    while (n--) {
        *p++ = (unsigned char)c;
    }
    return s;
}

void *memcpy(void *dest, const void *src, size_t n) {
    char *d = dest;
    const char *s = src;
    while (n--) {
        *d++ = *s++;
    }
    return dest;
}

// 内存移动 (处理重叠内存，比 memcpy 安全)
void *memmove(void *dest, const void *src, size_t n) {
    char *d = dest;
    const char *s = src;
    if (d < s) {
        while (n--) *d++ = *s++;
    } else {
        char *lasts = (char *)s + (n - 1);
        char *lastd = d + (n - 1);
        while (n--) *lastd-- = *lasts--;
    }
    return dest;
}

char *strchr(const char *s, int c) {
    while (*s != (char)c) {
        if (!*s++) {
            return NULL;
        }
    }
    return (char *)s;
}

static char *strtok_backup = NULL;

char *strtok(char *str, const char *delim) {
    if (str != NULL) {
        strtok_backup = str;
    }
    if (strtok_backup == NULL) {
        return NULL;
    }

    // 1. 跳过开头的分隔符
    while (*strtok_backup != '\0') {
        int is_delim = 0;
        for (const char *d = delim; *d != '\0'; d++) {
            if (*strtok_backup == *d) {
                is_delim = 1;
                break;
            }
        }
        if (!is_delim) {
            break;
        }
        strtok_backup++;
    }

    if (*strtok_backup == '\0') {
        strtok_backup = NULL;
        return NULL;
    }

    char *ret = strtok_backup;

    // 2. 找到 token 的结尾
    while (*strtok_backup != '\0') {
        int is_delim = 0;
        for (const char *d = delim; *d != '\0'; d++) {
            if (*strtok_backup == *d) {
                is_delim = 1;
                break;
            }
        }
        if (is_delim) {
            *strtok_backup = '\0';
            strtok_backup++;
            return ret;
        }
        strtok_backup++;
    }
    
    // 到了字符串末尾
    strtok_backup = NULL;
    return ret;
}