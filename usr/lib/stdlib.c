#include "syscall.h"
#include "stdlib.h"
#include <stddef.h>

// 内存块头部
typedef struct header {
    size_t size;        // 数据区大小
    struct header *next;// 下一块
    int free;           // 1=空闲
} header_t;

static header_t *head = NULL;
static header_t *tail = NULL;

// 简单的 malloc
void *malloc(size_t size) {
    if (size == 0) return NULL;

    // 1. 在现有链表中找空闲块
    header_t *curr = head;
    while (curr) {
        if (curr->free && curr->size >= size) {
            curr->free = 0;
            return (void*)(curr + 1); // 跳过头部，返回数据区
        }
        curr = curr->next;
    }

    // 2. 没找到，向内核申请新内存 (sbrk)
    size_t total_size = sizeof(header_t) + size;
    void *block = sbrk(total_size);

    if (block == (void*)-1) {
        return NULL; // 内存耗尽
    }

    header_t *header = (header_t*)block;
    header->size = size;
    header->free = 0;
    header->next = NULL;

    // 3. 加入链表
    if (!head) head = header;
    if (tail) tail->next = header;
    tail = header;

    return (void*)(header + 1);
}

// 简单的 free
void free(void *ptr) {
    if (!ptr) return;

    // 回退指针找到头部
    header_t *header = (header_t*)ptr - 1;
    header->free = 1;

}

int atoi(const char *str) {
    int res = 0;
    int sign = 1;
    int i = 0;
    if (str[0] == '-') {
        sign = -1;
        i++;
    }
    for (; str[i] != '\0'; ++i) {
        if (str[i] >= '0' && str[i] <= '9') {
            res = res * 10 + str[i] - '0';
        }
    }
    return sign * res;
}