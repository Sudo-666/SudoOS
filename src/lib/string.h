#pragma once
#include <stdint.h>

// 字符串长度
inline int strlen(const char* str) {
    int len = 0;
    while (str[len]) len++;
    return len;
}

// 内存比较
inline int memcmp(const void* s1, const void* s2, int n) {
    const uint8_t* p1 = (const uint8_t*)s1;
    const uint8_t* p2 = (const uint8_t*)s2;
    for (int i = 0; i < n; i++) {
        if (p1[i] != p2[i]) return p1[i] - p2[i];
    }
    return 0;
}

// 字符串比较
inline int strcmp(const char* s1, const char* s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++; s2++;
    }
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

// 整数转字符串
inline void itoa(int value, char* str, int base = 10) {
    if (value == 0) { str[0] = '0'; str[1] = '\0'; return; }
    char *ptr = str, *ptr1 = str, tmp_char;
    int tmp_value;
    if (value < 0 && base == 10) { *ptr++ = '-'; value = -value; }
    tmp_value = value;
    while (tmp_value > 0) {
        int rem = tmp_value % base;
        *ptr++ = (rem > 9) ? (rem - 10) + 'a' : rem + '0';
        tmp_value /= base;
    }
    *ptr = '\0';
    char* start = (str[0] == '-') ? str + 1 : str;
    ptr--;
    while (start < ptr) {
        tmp_char = *start; *start++ = *ptr; *ptr-- = tmp_char;
    }
}