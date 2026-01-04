#ifndef KERNEL_STRING_H
#define KERNEL_STRING_H


#include <stddef.h>
#include <stdint.h>


size_t strlen(const char *s);
int strcmp(const char *a, const char *b);
int strncmp(const char *a, const char *b, size_t n);
char *strcpy(char *dst, const char *src);
char *strncpy(char *dst, const char *src, size_t n);
void *memset(void *dst, int value, size_t n);
void *memcpy(void *dst, const void *src, size_t n);
int memcmp(const void *a, const void *b, size_t n);
char *itoa(int value, char *buffer, int base);
char *itoa_uint64(uint64_t value, char *buffer, uint64_t base);
char *strtok_r(char *str, const char *delim, char **saveptr);


#endif // KERNEL_STRING_H