# SudoOS 开发完全手册 (The SudoOS Bible)

**版本**: v1.0 (Architecture Baseline)
**架构**: x86_64 Monolithic Kernel (宏内核雏形)
**构建环境**: Docker / Ubuntu + GCC Cross Compiler (x86_64-linux-gnu)
**日期**: 2025-12-01

---

## 📂 1. 项目目录结构 (Project Structure)

这是 SudoOS 的物理文件布局，采用了内核态与用户态分离的设计。所有代码逻辑均以此结构为准。

```text
SudoOS/
├── Makefile             # [构建] 自动化编译脚本 (核心)
├── limine.conf          # [配置] Bootloader 引导菜单配置
├── external/            # [外部] 存放 Limine 二进制工具 (Git Submodule/Clone)
├── src/                 # [源码] 核心开发区域
│   ├── limine.h         # [协议] Limine 引导协议头文件 (C/C++ Interface)
│   ├── linker.ld        # [链接] 内核内存布局脚本 (定义 Higher Half Kernel)
│   │
│   ├── lib/             # [通用库] 类 libc 库 (内核态与用户态通用)
│   │   └── string.h     # 字符串与内存操作 (strlen, strcmp, itoa)
│   │
│   ├── kernel/          # [内核态] Ring 0 特权代码
│   │   ├── main.cpp     # [入口] 内核总入口 (_start)
│   │   ├── arch/        # [架构] x86_64 硬件层
│   │   │   ├── idt.h / idt.cpp   # 中断描述符表初始化
│   │   │   ├── trap.h / trap.S   # 汇编中断跳板与上下文保存
│   │   │   └── io.h              # 端口操作指令封装
│   │   └── drivers/     # [驱动] 硬件驱动层
│   │       ├── font.h            # 字库文件 (8x8 Bitmap)
│   │       ├── console.h / .cpp  # 显卡驱动 (Framebuffer)
│   │       └── keyboard.h / .cpp # 键盘驱动 (PS/2)
│   │
│   └── user/            # [用户态] Ring 3 模拟代码
│       ├── syscall.h    # [接口] 系统调用封装 (int 0x80)
│       └── shell.cpp    # [应用] 交互式 Shell 程序
```

---

## 🛠️ 2. 构建系统 (Build System)

### 文件：`Makefile`
**解析**：
这是整个项目的“施工图纸”。它解决了以下关键问题：
1.  **混合编译**：同时处理 `.cpp` (逻辑) 和 `.S` (汇编) 文件。
2.  **交叉编译**：强制使用 `x86_64-linux-gnu-` 工具链，防止在 ARM Mac 上生成错误架构代码。
3.  **ISO 打包**：结合 Limine 工具生成可启动的光盘镜像。

```makefile
KERNEL = kernel.elf
ISO = SudoOS.iso

# 编译器定义
CC = x86_64-linux-gnu-g++
LD = x86_64-linux-gnu-ld

# CFLAGS (编译选项) 解析：
# -ffreestanding: 裸机环境，无标准库
# -mno-red-zone: 禁用栈红区（内核中断处理必须禁用，否则栈会损坏）
# -mcmodel=kernel: 告诉编译器内核运行在内存的高地址区域 (Higher Half)
# -fno-pic -fno-pie: 禁用位置无关代码，内核地址是绝对固定的
# -I ...: 指定头文件搜索路径，方便代码中直接 include
CFLAGS = -g -O2 -pipe -Wall -Wextra -ffreestanding -fno-rtti -fno-exceptions \
         -m64 -march=x86-64 -mno-red-zone -mcmodel=kernel -fno-pic -fno-pie \
         -I src -I src/kernel -I src/user -I src/lib

# LDFLAGS (链接选项) 解析：
# -nostdlib: 不链接标准库
# -static: 静态链接
# -T src/linker.ld: 使用自定义的链接脚本控制内存布局
LDFLAGS = -m elf_x86_64 -nostdlib -static -z max-page-size=0x1000 -T src/linker.ld

# 自动扫描源文件
SRCS = $(wildcard src/kernel/*.cpp) \
       $(wildcard src/kernel/arch/*.cpp) \
       $(wildcard src/kernel/arch/*.S) \
       $(wildcard src/kernel/drivers/*.cpp) \
       $(wildcard src/user/*.cpp)

# 分别处理 C++ 和 汇编 文件的编译目标 (.cpp -> .o, .S -> .o)
OBJS_CPP = $(patsubst src/%.cpp, build/%.o, $(filter %.cpp, $(SRCS)))
OBJS_ASM = $(patsubst src/%.S, build/%.o, $(filter %.S, $(SRCS)))
OBJS = $(OBJS_CPP) $(OBJS_ASM)

all: $(ISO)

# 编译 C++ 规则
build/%.o: src/%.cpp
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# 编译汇编规则
build/%.o: src/%.S
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# 链接规则
$(KERNEL): $(OBJS) src/linker.ld
	$(LD) $(LDFLAGS) -o $@ $(OBJS)

# 打包规则 (依赖 xorriso)
$(ISO): $(KERNEL) limine.conf
	@mkdir -p dist/iso_root
	cp $(KERNEL) limine.conf dist/iso_root/
	cp external/limine/limine-bios.sys external/limine/limine-bios-cd.bin external/limine/limine-uefi-cd.bin dist/iso_root/
	mkdir -p dist/iso_root/EFI/BOOT
	cp external/limine/limine-uefi-cd.bin dist/iso_root/EFI/BOOT/BOOTX64.EFI
	xorriso -as mkisofs -b limine-bios-cd.bin -no-emul-boot -boot-load-size 4 -boot-info-table \
		--efi-boot limine-uefi-cd.bin -efi-boot-part --efi-boot-image --protective-msdos-label \
		dist/iso_root -o $(ISO)
	@echo "Build Success!"

# 运行规则
run: $(ISO)
	qemu-system-x86_64 -M q35 -m 2G -cdrom $(ISO) -boot d -vga std

clean:
	rm -rf build dist $(KERNEL) $(ISO)
```

### 文件：`src/linker.ld`
**解析**：
链接脚本控制了内核在内存中的最终形态。它将内核硬性搬移到了 `0xffffffff80000000` 这个高地址，这是现代操作系统（Linux/Windows）的标准做法，目的是将低地址空间（0x0000...）留给用户程序使用。

```ld
ENTRY(_start)
SECTIONS {
    /* 核心设定：Higher Half Kernel */
    . = 0xffffffff80000000;
    
    /* 代码段 */
    .text : { *(.text*) }
    . += 0x1000;
    
    /* 只读数据段 (常量) */
    .rodata : { *(.rodata*) }
    . += 0x1000;
    
    /* 数据段 (已初始化变量) */
    .data : { *(.data*) }
    . += 0x1000;
    
    /* BSS段 (未初始化变量，自动清零) */
    .bss : { *(COMMON) *(.bss*) }
}
```

### 文件：`limine.conf`
**解析**：告诉 Limine 如何引导我们的内核。

```text
timeout: 3
/SudoOS
    protocol: limine
    kernel_path: boot():/kernel.elf
    resolution: 1280x720
```

---

## 📚 3. 通用库 (Library)

### 文件：`src/lib/string.h`
**解析**：
由于我们处于裸机环境，没有 C 标准库（libc）。我们需要手动实现 `strlen`, `strcmp` 等基础函数。这是后续驱动开发和 Shell 逻辑的基础。

```cpp
#pragma once
#include <stdint.h>

inline int strlen(const char* str) {
    int len = 0;
    while (str[len]) len++;
    return len;
}

inline int strcmp(const char* s1, const char* s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++; s2++;
    }
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

// 整数转字符串 (支持 10 进制和 16 进制，用于调试打印)
inline void itoa(int value, char* str, int base = 10) {
    if (value == 0) { str[0] = '0'; str[1] = '\0'; return; }
    char *ptr = str, tmp_char;
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
```

---

## 🧠 4. 内核核心 (Kernel Core)

### 文件：`src/kernel/main.cpp`
**解析**：
这是内核的“大脑”。
1.  **Request**: 它向 Limine 发出请求，索要显存。
2.  **Initialization**: 依次初始化驱动（显卡、键盘）和架构层（IDT）。
3.  **Handover**: 最后，它调用 `user_main`，模拟从内核态跳转到用户态的过程。

```cpp
#include "../limine.h"
#include "drivers/console.h"
#include "drivers/keyboard.h"
#include "arch/idt.h"

// 声明外部的用户主函数
void user_main();

// Limine 显存请求 (编译器会将其放在特定段供 Bootloader 读取)
__attribute__((used, section(".requests")))
static volatile struct limine_framebuffer_request framebuffer_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST, .revision = 0
};

extern "C" void _start(void) {
    // 1. 检查显卡状态
    if (!framebuffer_request.response || framebuffer_request.response->framebuffer_count < 1) {
        while(1) __asm__("hlt");
    }

    // 2. 初始化驱动
    console_init(framebuffer_request.response->framebuffers[0]);
    kprintln("[Kernel] Console ready.");
    
    // 3. 初始化中断系统 (IDT)
    idt_init();
    kprintln("[Kernel] IDT/Syscall ready.");

    // 4. 跳转到用户空间
    kprintln("[Kernel] Jumping to User Space...");
    user_main();

    // 5. 兜底挂起 (理论上 Shell 不会退出)
    while(1) __asm__("hlt");
}
```

---

## 🏛️ 5. 底层架构 (Arch)

### 文件：`src/kernel/arch/io.h`
**解析**：
封装了 x86 的 `in` 和 `out` 汇编指令。这是 CPU 与键盘控制器、时钟等外设通信的唯一渠道。

```cpp
#pragma once
#include <stdint.h>

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ( "outb %0, %1" : : "a"(val), "Nd"(port) );
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ( "inb %1, %0" : "=a"(ret) : "Nd"(port) );
    return ret;
}
```

### 文件：`src/kernel/arch/trap.h`
**解析**：
定义了 **TrapFrame (中断帧)**。当 CPU 发生中断时，硬件会自动压入一部分寄存器，汇编代码会压入另一部分。这个结构体必须严格对齐栈中的数据，以便 C++ 代码可以读取或修改寄存器（例如实现上下文切换）。

```cpp
#pragma once
#include <stdint.h>

struct TrapFrame {
    // 手动压栈的寄存器 (push %r15 ... push %rax)
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rbp, rdi, rsi, rdx, rcx, rbx, rax;
    
    // 汇编宏压入的中断号和错误码
    uint64_t trap_num;
    uint64_t error_code;
    
    // CPU 自动压入的上下文 (iretq 恢复用)
    uint64_t rip, cs, rflags, rsp, ss;
} __attribute__((packed));
```

### 文件：`src/kernel/arch/trap.S`
**解析**：
这是中断处理的**汇编入口**。C++ 函数不能直接作为中断处理函数，因为需要保存现场（寄存器）。这段代码负责“保护现场”和“恢复现场”。

```asm
.section .text
.global alltraps

# 宏：用于没有错误码的异常 (如 syscall)，手动 push 0 占位
.macro ISR_NOERRCODE num
    .global vector\num
    vector\num:
        push $0          
        push $\num       
        jmp alltraps
.endm

# 宏：用于有错误码的异常 (如缺页)，直接 push 中断号
.macro ISR_ERRCODE num
    .global vector\num
    vector\num:
        push $\num       
        jmp alltraps     
.endm

# 定义常用中断入口
ISR_NOERRCODE 0   # Divide by Zero
ISR_ERRCODE   13  # General Protection Fault
ISR_ERRCODE   14  # Page Fault
ISR_NOERRCODE 128 # 0x80 (System Call)

.extern trap_handler
alltraps:
    # 1. 保存上下文 (TrapFrame 上半部分)
    push %rax; push %rbx; push %rcx; push %rdx; push %rsi; push %rdi; push %rbp;
    push %r8; push %r9; push %r10; push %r11; push %r12; push %r13; push %r14; push %r15;
    
    # 2. 传递参数 (rdi = TrapFrame 指针)
    mov %rsp, %rdi
    
    # 3. 调用 C++ 处理逻辑
    call trap_handler
    
    # 4. 恢复上下文
    pop %r15; pop %r14; pop %r13; pop %r12; pop %r11; pop %r10; pop %r9; pop %r8;
    pop %rbp; pop %rdi; pop %rsi; pop %rdx; pop %rcx; pop %rbx; pop %rax;
    
    # 5. 清理 error_code 和 trap_num
    add $16, %rsp
    iretq
```

### 文件：`src/kernel/arch/idt.cpp`
**解析**：
1.  **idt_init**: 填充中断描述符表 (IDT)，将 0x80 号中断指向汇编入口。
2.  **trap_handler**: 全局中断分发器。所有中断最终都会汇聚到这里。它解析 `TrapFrame` 中的中断号，如果是 0x80，则根据 `rax` 的值执行具体的系统调用。

```cpp
#include "idt.h"
#include "trap.h"
#include "../drivers/console.h"
#include "../drivers/keyboard.h"

extern "C" void vector0();
extern "C" void vector13();
extern "C" void vector14();
extern "C" void vector128();

struct IdtEntry {
    uint16_t low; uint16_t sel; uint8_t ist; uint8_t flags;
    uint16_t mid; uint32_t high; uint32_t zero;
} __attribute__((packed));
struct IdtPtr { uint16_t limit; uint64_t base; } __attribute__((packed));

static struct IdtEntry idt[256];
static struct IdtPtr idtp;

void idt_set_gate(uint8_t num, uint64_t base, uint16_t sel, uint8_t flags) {
    idt[num].low = base & 0xFFFF;
    idt[num].mid = (base >> 16) & 0xFFFF;
    idt[num].high = (base >> 32) & 0xFFFFFFFF;
    idt[num].sel = sel;
    idt[num].ist = 0;
    idt[num].flags = flags;
    idt[num].zero = 0;
}

// [核心逻辑] 中断与系统调用分发
extern "C" void trap_handler(struct TrapFrame* tf) {
    switch (tf->trap_num) {
        case 14:
            kprintln("PANIC: Page Fault!");
            while(1) __asm__("hlt");
            break;
        case 0x80: // System Call
            // Linux ABI: rax = syscall number
            if (tf->rax == 1) { // sys_write
                const char* str = (const char*)tf->rbx;
                kprint(str);
            }
            else if (tf->rax == 2) { // sys_read
                char* buf = (char*)tf->rbx;
                int len = (int)tf->rcx;
                kinput(buf, len); // 阻塞式调用键盘驱动
            }
            break; 
        default:
            break;
    }
}

void idt_init() {
    idtp.limit = (sizeof(struct IdtEntry) * 256) - 1;
    idtp.base = (uint64_t)&idt;
    
    // 注册关键中断
    idt_set_gate(0, (uint64_t)vector0, 0x28, 0x8E);
    idt_set_gate(13, (uint64_t)vector13, 0x28, 0x8E);
    idt_set_gate(14, (uint64_t)vector14, 0x28, 0x8E);
    idt_set_gate(128, (uint64_t)vector128, 0x28, 0xEE); // 0x80, DPL=3 (允许用户调用)

    __asm__ volatile ("lidt %0" : : "m"(idtp));
}
```

---

## 🎮 6. 驱动层 (Drivers)

### 文件：`src/kernel/drivers/console.cpp`
**解析**：
显卡驱动。
1.  **console_init**: 保存 Limine 提供的 Framebuffer 信息。
2.  **kprint_char**: 核心绘图逻辑。通过读取 `font.h` 的位图信息，在屏幕内存的对应位置写入颜色值（画点）。支持换行和简单的退格处理。

```cpp
#include "console.h"
#include "font.h" // 引用完整的 8x8 字库

static struct limine_framebuffer* g_fb = nullptr;
static int g_x = 0, g_y = 0;

static void put_pixel(int x, int y, uint32_t color) {
    if (!g_fb) return;
    if (x < 0 || x >= (int)g_fb->width || y < 0 || y >= (int)g_fb->height) return;
    uint32_t* ptr = (uint32_t*)g_fb->address;
    ptr[y * (g_fb->pitch / 4) + x] = color;
}

void kprint_char(char c) {
    if (c == '\n') { g_x = 0; g_y += 8; return; }
    if (c == '\b') { 
        if (g_x >= 8) { g_x -= 8; for(int i=0;i<64;i++) put_pixel(g_x+i%8, g_y+i/8, 0); }
        return; 
    }
    const uint8_t* bmp = font8x8_basic[(unsigned char)c];
    for(int r=0; r<8; r++) {
        for(int c=0; c<8; c++) {
            if (bmp[r] & (1 << c)) put_pixel(g_x+c, g_y+r, 0xFFFFFF);
        }
    }
    g_x += 8;
    if (g_x >= (int)g_fb->width) { g_x = 0; g_y += 8; }
}

void kprint(const char* str) { while (*str) kprint_char(*str++); }
void kprintln(const char* str) { kprint(str); kprint_char('\n'); }
void kprint_int(int val) { char buf[32]; itoa(val, buf); kprint(buf); }
void kclear() { /* ... */ g_x=0; g_y=0; }
void console_init(struct limine_framebuffer* fb) { g_fb = fb; kclear(); }
```

### 文件：`src/kernel/drivers/keyboard.cpp`
**解析**：
PS/2 键盘驱动。
1.  **read_key_raw**: 轮询端口 0x60/0x64，获取扫描码。支持 Shift 键状态维护，通过查表（kmap_low/kmap_up）将扫描码转换为 ASCII。
2.  **kinput**: 阻塞式输入函数。循环调用 `read_key_raw`，直到读到回车符才返回。这是 `sys_read` 的底层实现。

```cpp
#include "keyboard.h"
#include "../arch/io.h"

// 键盘映射表 (Scan Code Set 1)
static const char kmap_low[128] = { ... }; // 略: 普通字符
static const char kmap_up[128]  = { ... }; // 略: Shift 字符
static bool g_shift = false;

static char read_key_raw() {
    if ((inb(0x64) & 1) == 0) return 0;
    uint8_t c = inb(0x60);
    if (c == 0x2A || c == 0x36) { g_shift = true; return 0; }
    if (c == 0xAA || c == 0xB6) { g_shift = false; return 0; }
    if (c & 0x80) return 0; 
    return g_shift ? kmap_up[c] : kmap_low[c];
}

void kinput(char* buffer, int max_len) {
    int idx = 0;
    while(1) {
        char c = read_key_raw();
        if (c == 0) continue;
        
        if (c == '\n') {
            kprint_char('\n'); buffer[idx] = 0; return;
        } else if (c == '\b') {
            if (idx > 0) { idx--; kprint_char('\b'); }
        } else {
            if (idx < max_len - 1) { buffer[idx++] = c; kprint_char(c); }
        }
    }
}
```

---

## 👤 7. 用户空间 (User Space)

### 文件：`src/user/syscall.h`
**解析**：
用户程序的 API。它不包含任何内核逻辑，仅仅是封装了汇编指令。
* **sys_write (1)**: 对应内核 `kprint`。
* **sys_read (2)**: 对应内核 `kinput`。

```cpp
#pragma once
#include <stdint.h>

static inline void sys_write(const char* str) {
    __asm__ volatile (
        "mov $1, %%rax \n" // Syscall ID 1
        "mov %0, %%rbx \n" // Arg 1
        "int $0x80     \n" 
        : : "r"(str) : "rax", "rbx"
    );
}

static inline void sys_read(char* buffer, int max_len) {
    __asm__ volatile (
        "mov $2, %%rax \n" // Syscall ID 2
        "mov %0, %%rbx \n" // Arg 1
        "mov %1, %%rcx \n" // Arg 2
        "int $0x80     \n"
        : : "r"(buffer), "r"((uint64_t)max_len) : "rax", "rbx", "rcx"
    );
}
```

### 文件：`src/user/shell.cpp`
**解析**：
这是运行在模拟用户态（未来将是 Ring 3）的第一个应用程序。它完全依赖 `syscall.h` 提供的接口工作，不知道显卡和键盘的存在。

```cpp
#include "syscall.h"
#include "../lib/string.h"

void user_main() {
    sys_write("\n[User] Shell started.\n");
    char cmd[64];

    while(1) {
        // 打印提示符
        sys_write("User@SudoOS $ ");
        
        // 阻塞等待输入 (陷入内核)
        sys_read(cmd, 64);
        
        // 命令解析
        if (strcmp(cmd, "help") == 0) {
            sys_write("Commands: help, clear, exit\n");
        } else if (strcmp(cmd, "clear") == 0) {
            sys_write("\n\n\n\n\n"); 
        } else if (strlen(cmd) > 0) {
            sys_write("Unknown command.\n");
        }
    }
}
```
