#pragma once
#include <stdint.h>
#include "../lib/std.h"

// 定义文件类型
#define RAMFS_TYPE_FILE 1
#define RAMFS_TYPE_DIR  2

// 文件节点结构升级
typedef struct {
    char name[32];      // 文件名 (不包含路径，例如 "bin")
    uint8_t* content;   // 文件内容指针
    uint64_t size;      // 文件大小
    bool is_used;
    
    // === 新增字段 ===
    int type;           // RAMFS_TYPE_FILE 或 RAMFS_TYPE_DIR
    int parent_idx;     // 父节点在数组中的下标 (-1 表示无父节点，即根目录)
    int inode_idx;      // 自身在数组中的下标 (方便回溯)
} ramfs_node_t;

typedef struct {
    ramfs_node_t* node;
    uint64_t offset;
    int ref_count;
} file_t;

// 增加路径操作函数
int ramfs_mkdir(const char* path);
int ramfs_chdir(const char* path); // 供 syscall 调用
char* ramfs_getcwd(char* buf, size_t size);

// 原有函数
void ramfs_init(void* init_addr, uint64_t init_size);
int ramfs_open(const char* name, int flags);
int ramfs_read(int fd, void* buf, int count);
int ramfs_write(int fd, const void* buf, int count);
void ramfs_close(int fd);
int ramfs_getdents64(int fd, void* dirp, int count);
int ramfs_stat(const char* path, void* buf);
int ramfs_fstat(int fd, void* buf);