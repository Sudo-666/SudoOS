#include "ramfs.h"
#include "../proc/proc.h"
#include "../mm/pmm.h"
#include "../drivers/console.h"

#define MAX_FILES 64
#define MAX_SYSTEM_OPEN_FILES 128 // 系统允许同时打开的最大文件句柄数

#define O_CREAT 64

// ALIGN_UP 在 pmm.h 中已定义，这里不再重复定义

// RAMFS 节点存储
static ramfs_node_t files[MAX_FILES];

// 全局静态文件句柄表
static file_t global_file_table[MAX_SYSTEM_OPEN_FILES];

// Linux dirent64 结构
struct linux_dirent64 {
    uint64_t d_ino;
    int64_t  d_off;
    unsigned short d_reclen;
    unsigned char  d_type;
    char           d_name[];
};

#define DT_DIR     4
#define DT_REG     8

extern pcb_t* current_proc;

// 辅助：获取 FD 表
static file_t** get_cur_fd_table() {
    if (!current_proc) return NULL;
    return (file_t**)current_proc->fd_table;
}

// 辅助：分配一个全局文件句柄
static file_t* alloc_file_handle() {
    for (int i = 0; i < MAX_SYSTEM_OPEN_FILES; i++) {
        if (global_file_table[i].ref_count == 0) {
            memset(&global_file_table[i], 0, sizeof(file_t));
            global_file_table[i].ref_count = 1; // 标记为已使用
            return &global_file_table[i];
        }
    }
    return NULL;
}

// 辅助：释放文件句柄
static void free_file_handle(file_t* f) {
    if (f) {
        f->ref_count = 0;
        f->node = NULL;
    }
}

// 辅助：分配 inode
static int alloc_inode() {
    for (int i = 0; i < MAX_FILES; i++) {
        if (!files[i].is_used) {
            memset(&files[i], 0, sizeof(ramfs_node_t));
            files[i].is_used = true;
            files[i].inode_idx = i;
            return i;
        }
    }
    return -1;
}

// 路径解析
static int resolve_path(const char* path, int* parent_out, char* name_out) {
    int curr = current_proc ? current_proc->cwd_inode : 0;
    
    if (!path) return -1;
    if (path[0] == '/') {
        curr = 0;
        path++;
    }

    char temp_path[128];
    // 安全拷贝，防止溢出
    int len = strlen(path);
    if (len > 127) len = 127;
    memcpy(temp_path, path, len);
    temp_path[len] = '\0';

    char* p = temp_path;
    char* token;
    
    // 使用 string.h 中手动实现的 strtok_r
    extern char *strtok_r(char *str, const char *delim, char **saveptr);

    while ((token = strtok_r(p, "/", &p))) {
        if (strlen(token) == 0) continue;
        if (strcmp(token, ".") == 0) continue;
        if (strcmp(token, "..") == 0) {
            if (files[curr].parent_idx != -1) curr = files[curr].parent_idx;
            continue;
        }

        int found = -1;
        for (int i = 0; i < MAX_FILES; i++) {
            if (files[i].is_used && files[i].parent_idx == curr && strcmp(files[i].name, token) == 0) {
                found = i;
                break;
            }
        }

        if (p != NULL && *p != '\0') {
            if (found == -1 || files[found].type != RAMFS_TYPE_DIR) return -1;
            curr = found;
        } else {
            if (parent_out) *parent_out = curr;
            if (name_out) strcpy(name_out, token);
            return found;
        }
    }
    
    if (parent_out) *parent_out = files[curr].parent_idx;
    return curr;
}

void ramfs_init(void* init_addr, uint64_t init_size) {
    memset(files, 0, sizeof(files));
    memset(global_file_table, 0, sizeof(global_file_table)); 
    
    // 1. 创建根目录 "/"
    files[0].is_used = true;
    strcpy(files[0].name, ""); // 根目录
    files[0].type = RAMFS_TYPE_DIR;
    files[0].parent_idx = -1;
    files[0].inode_idx = 0;

    // 2. 创建 "/init" (系统核心进程)
    int init_idx = alloc_inode();
    strcpy(files[init_idx].name, "init");
    files[init_idx].type = RAMFS_TYPE_FILE;
    files[init_idx].parent_idx = 0;
    files[init_idx].content = (uint8_t*)init_addr;
    files[init_idx].size = init_size;

    // === 3. 构建 "/usr" 目录结构 (模拟你的项目结构) ===
    
    // 创建 /usr
    int usr_idx = alloc_inode();
    strcpy(files[usr_idx].name, "usr");
    files[usr_idx].type = RAMFS_TYPE_DIR;
    files[usr_idx].parent_idx = 0; // 父目录是根

    // 创建 /usr/bin
    int bin_idx = alloc_inode();
    strcpy(files[bin_idx].name, "bin");
    files[bin_idx].type = RAMFS_TYPE_DIR;
    files[bin_idx].parent_idx = usr_idx;

    // 创建 /usr/lib
    int lib_idx = alloc_inode();
    strcpy(files[lib_idx].name, "lib");
    files[lib_idx].type = RAMFS_TYPE_DIR;
    files[lib_idx].parent_idx = usr_idx;

    // === 4. 在 /usr 中放入模拟的源代码文件 ===
    
    // 创建 /usr/shell.c
    int shell_idx = alloc_inode();
    strcpy(files[shell_idx].name, "shell.c");
    files[shell_idx].type = RAMFS_TYPE_FILE;
    files[shell_idx].parent_idx = usr_idx;
    char* shell_code = "// This is SudoOS Shell Source Code...\n";
    files[shell_idx].content = (uint8_t*)kmalloc(PAGE_SIZE);
    strcpy((char*)files[shell_idx].content, shell_code);
    files[shell_idx].size = strlen(shell_code);

    // 创建 /usr/usrmain.c
    int main_idx = alloc_inode(); // 【修正】这里是 main_idx
    strcpy(files[main_idx].name, "usrmain.c");
    files[main_idx].type = RAMFS_TYPE_FILE;
    files[main_idx].parent_idx = usr_idx;
    char* main_code = "// User Main Entry Point\n";
    files[main_idx].content = (uint8_t*)kmalloc(PAGE_SIZE);
    
    // 【修正】下标使用 main_idx，而不是 main_code
    strcpy((char*)files[main_idx].content, main_code); 
    files[main_idx].size = strlen(main_code);

    kprintln("[RamFS] Initialized. Structure: /usr created.");
}

int ramfs_open(const char* path, int flags) {
    int parent_idx = -1;
    char name[32] = {0};
    int inode = resolve_path(path, &parent_idx, name);

    if (inode == -1 && (flags & O_CREAT)) {
        if (parent_idx == -1) return -1;
        inode = alloc_inode();
        if (inode == -1) return -1;
        strcpy(files[inode].name, name);
        files[inode].type = RAMFS_TYPE_FILE;
        files[inode].parent_idx = parent_idx;
        files[inode].size = 0;
        files[inode].content = (uint8_t*)kmalloc(4096);
        memset(files[inode].content, 0, 4096);
    } else if (inode == -1) {
        return -1;
    }

    file_t* file = alloc_file_handle();
    if (!file) return -1; 

    file->node = &files[inode];
    file->offset = 0;
    
    file_t** fds = get_cur_fd_table();
    if (!fds) { free_file_handle(file); return -1; }

    for (int i = 3; i < MAX_FD; i++) {
        if (fds[i] == NULL) {
            fds[i] = file;
            return i;
        }
    }
    
    free_file_handle(file); 
    return -1;
}

int ramfs_read(int fd, void* buf, int count) {
    file_t** fds = get_cur_fd_table();
    if (fd < 0 || fd >= MAX_FD || fds[fd] == NULL) return -1;

    file_t* f = fds[fd];
    if (f->node->type == RAMFS_TYPE_DIR) return -1; 

    if (f->offset >= f->node->size) return 0;
    
    int read_len = count;
    if (f->offset + (uint64_t)count > f->node->size) {
        read_len = f->node->size - f->offset;
    }
    
    if (read_len > 0) {
        memcpy(buf, f->node->content + f->offset, read_len);
        f->offset += read_len;
    }
    return read_len;
}

int ramfs_write(int fd, const void* buf, int count) {
    file_t** fds = get_cur_fd_table();
    if (fd < 0 || fd >= MAX_FD || fds[fd] == NULL) return -1;
    file_t* f = fds[fd];
    if (f->node->type == RAMFS_TYPE_DIR) return -1;

    int max_size = 4096;
    if (f->offset + (uint64_t)count > (uint64_t)max_size) count = max_size - f->offset;

    if (count > 0 && f->node->content) {
        memcpy(f->node->content + f->offset, buf, count);
        f->offset += count;
        if (f->offset > f->node->size) f->node->size = f->offset;
    }
    return count;
}

void ramfs_close(int fd) {
    file_t** fds = get_cur_fd_table();
    if (fd < 0 || fd >= MAX_FD || fds[fd] == NULL) return;
    
    free_file_handle(fds[fd]);
    fds[fd] = NULL;
}

int ramfs_getdents64(int fd, void* dirp, int count) {
    file_t** fds = get_cur_fd_table();
    if (fd < 0 || fd >= MAX_FD || fds[fd] == NULL) return -1;

    file_t* dir_file = fds[fd];
    if (dir_file->node->type != RAMFS_TYPE_DIR) return -1;

    int target_parent_idx = dir_file->node->inode_idx;
    uint8_t* p = (uint8_t*)dirp;
    int bpos = 0;
    
    int search_idx = (int)dir_file->offset; 

    while (search_idx < MAX_FILES) {
        if (files[search_idx].is_used && files[search_idx].parent_idx == target_parent_idx) {
            
            int name_len = strlen(files[search_idx].name);
            int reclen = ALIGN_UP(sizeof(struct linux_dirent64) + name_len + 1, 8);

            if (bpos + reclen > count) break;

            struct linux_dirent64* d = (struct linux_dirent64*)(p + bpos);
            d->d_ino = search_idx + 1;
            d->d_off = bpos + reclen;
            d->d_reclen = reclen;
            d->d_type = (files[search_idx].type == RAMFS_TYPE_DIR) ? DT_DIR : DT_REG;
            strcpy(d->d_name, files[search_idx].name);

            bpos += reclen;
        }
        search_idx++;
    }

    dir_file->offset = search_idx;
    return bpos;
}

int ramfs_mkdir(const char* path) {
    int parent_idx = -1;
    char name[32] = {0};

    int inode = resolve_path(path, &parent_idx, name);
    if (inode != -1) return -1; 
    if (parent_idx == -1) return -1; 

    int new_dir = alloc_inode();
    if (new_dir == -1) return -1;

    strcpy(files[new_dir].name, name);
    files[new_dir].type = RAMFS_TYPE_DIR;
    files[new_dir].parent_idx = parent_idx;
    files[new_dir].size = 0; 
    return 0;
}

int ramfs_chdir(const char* path) {
    int inode = resolve_path(path, NULL, NULL);
    if (inode == -1) return -1;
    if (files[inode].type != RAMFS_TYPE_DIR) return -1;

    if (current_proc) current_proc->cwd_inode = inode;
    return 0;
}

char* ramfs_getcwd(char* buf, size_t size) {
    if (!buf || size == 0 || !current_proc) return NULL;
    int curr = current_proc->cwd_inode;
    
    if (curr == 0) {
        if (size < 2) return NULL;
        strcpy(buf, "/");
        return buf;
    }

    int temp = curr;
    int len = 0;
    while (temp != 0) {
        len += 1 + strlen(files[temp].name);
        temp = files[temp].parent_idx;
        if (temp == -1 && temp != 0) break; 
        if ((size_t)len >= size) return NULL;
    }
    
    int write_pos = len;
    buf[write_pos] = '\0';
    
    temp = curr;
    while (temp != 0) {
        int name_len = strlen(files[temp].name);
        write_pos -= name_len;
        memcpy(buf + write_pos, files[temp].name, name_len);
        write_pos--;
        buf[write_pos] = '/';
        temp = files[temp].parent_idx;
    }
    return buf;
}

int ramfs_stat(const char* path, void* buf) {
    int inode = resolve_path(path, NULL, NULL);
    if (inode == -1) return -1;

    struct dummy_stat {
        uint64_t st_dev; uint64_t st_ino; uint64_t st_nlink;
        uint32_t st_mode; uint32_t st_uid; uint32_t st_gid;
        uint32_t __pad0; uint64_t st_rdev; int64_t  st_size;
    } *st = buf;

    st->st_ino = inode + 1;
    st->st_size = files[inode].size;
    if (files[inode].type == RAMFS_TYPE_DIR) st->st_mode = 0040777; 
    else st->st_mode = 0100777;
    return 0;
}

int ramfs_fstat(int fd, void* buf) {
    file_t** fds = get_cur_fd_table();
    if (fd < 0 || fd >= MAX_FD || fds[fd] == NULL) return -1;
    return ramfs_stat(fds[fd]->node->name, buf);
}