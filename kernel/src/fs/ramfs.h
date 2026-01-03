#pragma once
#include <stdint.h>
#include "../lib/std.h"

// 文件节点
typedef struct {
    char name[32];
    uint8_t* content;
    uint64_t size;
    bool is_used;
} ramfs_node_t;

// 打开的文件句柄
typedef struct {
    ramfs_node_t* node;
    uint64_t offset;
    int ref_count;
} file_t;

void ramfs_init(void* init_addr, uint64_t init_size);
int ramfs_open(const char* name, int flags);
int ramfs_read(int fd, void* buf, int count);
int ramfs_write(int fd, const void* buf, int count);
void ramfs_close(int fd);
int ramfs_getdents64(int fd, void* dirp, int count);
int ramfs_stat(const char* path, void* buf);
int ramfs_fstat(int fd, void* buf);