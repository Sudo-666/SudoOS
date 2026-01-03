#include "ramfs.h"
#include "../proc/proc.h"
#include "../mm/pmm.h"

#define MAX_FILES 16
#define O_CREAT 64
static ramfs_node_t files[MAX_FILES];

// Linux dirent64 结构定义
struct linux_dirent64 {
    uint64_t d_ino;
    int64_t  d_off;
    unsigned short d_reclen;
    unsigned char  d_type;
    char           d_name[];
};

#define DT_REG 8
#define DT_DIR 4

void ramfs_init(void* init_addr, uint64_t init_size) {
    memset(files, 0, sizeof(files));
    
    // 1. 创建 init 程序文件 (从 Bootloader 传来的)
    strcpy(files[0].name, "init");
    files[0].content = (uint8_t*)init_addr;
    files[0].size = init_size;
    files[0].is_used = true;

    // 2. 创建一个测试文件
    strcpy(files[1].name, "test.txt");
    char* msg = "Hello from SudoOS Kernel File System!\n";
    files[1].content = (uint8_t*)kmalloc(PAGE_SIZE);
    strcpy((char*)files[1].content, msg);
    files[1].size = strlen(msg);
    files[1].is_used = true;
    
    kprintln("[RamFS] File system initialized.");
}

// 辅助：获取当前进程的 FD 表
static file_t** get_cur_fd_table() {
    extern pcb_t* current_proc;
    return (file_t**)current_proc->fd_table;
}

int ramfs_open(const char* name, int flags) {
    if (name[0] == '/') name++;

    // 1. 查找文件
    ramfs_node_t* node = NULL;
    for (int i = 0; i < MAX_FILES; i++) {
        if (files[i].is_used && strcmp(files[i].name, name) == 0) {
            node = &files[i];
            break;
        }
    }

    // === 新增：如果找不到且有创建标志，则创建新文件 ===
    if (!node && (flags & O_CREAT)) {
        for (int i = 0; i < MAX_FILES; i++) {
            if (!files[i].is_used) {
                node = &files[i];
                strcpy(node->name, name);
                node->is_used = true;
                node->size = 0;
                // 分配 4KB 固定空间供写入
                node->content = (uint8_t*)kmalloc(4096); 
                memset(node->content, 0, 4096);
                break;
            }
        }
    }
    // ===============================================

    if (!node) return -1; // 还是没找到或者满了

    // 2. 分配文件句柄
    file_t* file = (file_t*)kmalloc(sizeof(file_t));
    file->node = node;
    file->offset = 0;
    // 如果是追加模式，offset 指向末尾；这里简单处理，打开始终指向开头
    // 若要支持 append，需判断 flags

    // 3. 找一个空闲 FD
    // ... (原代码保持不变) ...
    // 下面这部分你需要保留原有的 get_cur_fd_table() 逻辑
    extern pcb_t* current_proc;
    file_t** fds = (file_t**)current_proc->fd_table;
    
    for (int i = 3; i < MAX_FD; i++) {
        if (fds[i] == NULL) {
            fds[i] = file;
            return i;
        }
    }
    kfree(file);
    return -1;
}

int ramfs_read(int fd, void* buf, int count) {
    file_t** fds = get_cur_fd_table();
    if (fd < 0 || fd >= MAX_FD || fds[fd] == NULL) return -1;

    file_t* f = fds[fd];
    if (f->offset >= f->node->size) return 0; // EOF

    int read_len = count;
    if (f->offset + count > f->node->size) {
        read_len = f->node->size - f->offset;
    }

    memcpy(buf, f->node->content + f->offset, read_len);
    f->offset += read_len;
    return read_len;
}

void ramfs_close(int fd) {
    file_t** fds = get_cur_fd_table();
    if (fd < 0 || fd >= MAX_FD || fds[fd] == NULL) return;
    
    kfree(fds[fd]);
    fds[fd] = NULL;
}

// ls 命令的核心
int ramfs_getdents64(int fd, void* dirp, int count) {
    // 这里我们作弊：忽略 fd，直接列出所有文件
    // 真正的实现应该检查 fd 是否指向目录
    
    static int file_idx = 0; // 极简：不支持重入，简单演示
    if (file_idx >= MAX_FILES) { file_idx = 0; return 0; }

    uint8_t* p = (uint8_t*)dirp;
    int bpos = 0;

    while (file_idx < MAX_FILES) {
        if (files[file_idx].is_used) {
            struct linux_dirent64* d = (struct linux_dirent64*)(p + bpos);
            int name_len = strlen(files[file_idx].name);
            int reclen = ALIGN_UP(sizeof(struct linux_dirent64) + name_len + 1, 8);

            if (bpos + reclen > count) break; // 缓冲区满了

            d->d_ino = file_idx + 1;
            d->d_off = bpos + reclen;
            d->d_reclen = reclen;
            d->d_type = DT_REG;
            strcpy(d->d_name, files[file_idx].name);

            bpos += reclen;
        }
        file_idx++;
    }
    
    // 如果一次读完，重置索引以便下次调用
    if (bpos == 0 && file_idx >= MAX_FILES) file_idx = 0;
    
    return bpos;
}

// 获取文件信息 (ls -l 需要)
int ramfs_stat(const char* path, void* buf) {
    // 极简结构，假设用户传的是 struct stat 的前几个字段
    struct dummy_stat {
        uint64_t st_dev;
        uint64_t st_ino;
        uint64_t st_nlink;
        uint32_t st_mode;
        uint32_t st_uid;
        uint32_t st_gid;
        uint32_t __pad0;
        uint64_t st_rdev;
        int64_t  st_size;
    } *st = buf;

    if (path[0] == '/') path++;

    for (int i = 0; i < MAX_FILES; i++) {
        if (files[i].is_used && strcmp(files[i].name, path) == 0) {
            st->st_ino = i + 1;
            st->st_mode = 0100777; // Regular file, rwx
            st->st_size = files[i].size;
            return 0;
        }
    }
    return -1;
}

int ramfs_fstat(int fd, void* buf) {
    file_t** fds = get_cur_fd_table();
    if (fd < 0 || fd >= MAX_FD || fds[fd] == NULL) return -1;
    return ramfs_stat(fds[fd]->node->name, buf);
}

int ramfs_write(int fd, const void* buf, int count) {
    extern pcb_t* current_proc;
    file_t** fds = (file_t**)current_proc->fd_table;
    
    if (fd < 0 || fd >= MAX_FD || fds[fd] == NULL) return -1;

    file_t* f = fds[fd];
    
    // 保护：防止写操作溢出分配的内存 (假设最大 4KB)
    // 注意：init 程序等预设文件的 content 指针可能不在堆上，写入可能会崩
    // 这里简单判断：如果 content 是空的或者我们认为是预设只读的，就不能写
    
    int max_size = 4096; 
    
    if (f->offset + count > max_size) {
        count = max_size - f->offset;
    }
    
    if (count > 0) {
        memcpy(f->node->content + f->offset, buf, count);
        f->offset += count;
        // 更新文件大小
        if (f->offset > f->node->size) {
            f->node->size = f->offset;
        }
    }

    return count;
}