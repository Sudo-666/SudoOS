# SudoOS：一个完全自主实现的现代 x86-64 宏内核操作系统

## 1. 项目核心定位
SudoOS 是一个**从零构建（From Scratch）**、面向现代 **x86-64 架构**的宏内核（Monolithic Kernel）操作系统。本项目摒弃了传统的 32 位过渡模式，直接通过开源bootloader项目 **Limine 引导协议** 进入 64 位长模式（Long Mode），旨在实现一个具备**抢占式多任务调度**、**虚拟内存管理**、**用户态隔离**以及**交互式 Shell** 的完整内核生态。

## 2. 核心技术优势 (Technical Highlights)

### 🚀 现代化的架构设计 (Architecture)
* **主流 x86-64 长模式**：系统直接运行在 64 位模式下，完全利用现代 CPU 的通用寄存器和指令集，突破了 4GB 内存限制，支持 NX（No-Execute）位保护,。
* **Limine 高级引导协议**：采用业界前沿的 Limine Bootloader，支持 HHDM（Higher Half Direct Map）特性，实现了内核空间的高地址映射，为内核与用户空间的隔离奠定了基础,。
* **硬件抽象层 (HAL)**：实现了 GDT/IDT 的重构与管理，完善的中断上下文保存（Context Save/Restore）机制，支持异常（Exception）与硬件中断（IRQ）的精确分发,。

### 💾 工业级内存管理系统 (Memory Management)
* **标准四级页表 (4-Level Paging)**：完整实现了 **PML4 -> PDPT -> PD -> PT** 的四级分页机制，支持 48 位虚拟地址空间，精确管理用户态与内核态的内存映射。
* **物理内存管理 (PMM)**：基于 **Bitmap（位图）** 算法的高效物理页分配器，通过解析 BIOS/UEFI 提供的内存映射图（E820/UEFI MemMap）动态管理物理内存资源。
* **虚拟内存管理 (VMM)**：支持缺页异常处理，实现了内核堆（Kernel Heap）的动态分配（`kmalloc`/`kfree`），支持 VMA（虚拟内存区域）管理，为每个进程提供独立的地址空间。

### ⚡ 抢占式多任务调度 (Process & Scheduling)
* **1:1 线程模型**：实现了基于 PCB（进程控制块）的内核级线程管理，支持线程的创建、等待、退出和资源回收。
* **时间片轮转调度 (Round-Robin)**：基于 PIT 时钟中断实现**抢占式调度器**。系统能够强制剥夺耗时进程的 CPU 使用权，保证了交互式任务的响应速度和系统的公平性,。
* **完整的生命周期管理**：设计了清晰的进程状态机（READY, RUNNING, ZOMBIE），并实现了 **Exit-Zombie-Reap** 机制，彻底解决了僵尸进程回收和内核栈释放的死锁难题。

### 🖥️ 完整的用户态生态 (User Land)
* **Ring 0 / Ring 3 特权级隔离**：实现了从内核态到用户态的安全切换（`iretq`），用户程序在受限的 Ring 3 级别运行，无法直接破坏内核数据。
* **ELF64 可执行文件加载器**：内核内置 ELF 解析器，能够动态解析并加载标准的 ELF64 二进制文件，支持 `.text`, `.data`, `.bss` 段的内存映射。
* **交互式 Shell 与 Libc**：
    * 移植并实现了一个轻量级的 **Libc**（包含 `stdio`, `string`, `stdlib`）。
    * 开发了功能完备的 **SudoOS Shell**，支持 `ls`, `cd`, `cat`, `run` 等十余种命令，支持参数解析和命令历史。
* **系统调用接口 (System Calls)**：通过 `int 0x80` 软中断实现了标准的系统调用接口，支持文件读写、进程控制、内存申请等核心功能。

### 📂 虚拟文件系统 (RamFS)
* **内存文件系统 (In-Memory FS)**：实现了一个类 VFS 接口的 RamFS，支持目录树结构解析、文件读写（Read/Write）、目录遍历（Getdents）和权限管理，能够模拟真实的文件系统操作体验。

## 3. 工程化与构建系统
* **自动化构建**：使用 GNU Make 构建了模块化的编译系统，支持一键编译内核、用户库和 ISO 镜像打包。
* **跨平台兼容**：支持 BIOS 与 UEFI 双模引导（通过 Limine），可在 QEMU 模拟器及真实物理机上运行。

## 4.项目结构树
SudoOS/
├── .gitignore                 # Git 版本控制忽略配置
├── flake.nix                  # Nix 环境配置文件（用于开发环境构建）
├── GNUmakefile                # 项目顶层 Makefile，负责编译内核、用户程序和制作 ISO
├── limine.conf                # Limine 引导加载程序配置文件，指定内核路径和协议
├── README.md                  # 项目说明文档
├── sudoOS.iso                 # 最终构建出的操作系统 ISO 镜像
├── boot/                      # 引导加载程序相关文件
│   └── limine/                # Limine Bootloader 二进制文件及模块 (UEFI/BIOS)
├── kernel/                    # 操作系统内核源码目录
│   ├── GNUmakefile            # 内核子项目 Makefile
│   ├── get-deps               # 脚本：用于获取内核依赖（如 Limine）
│   ├── linker.lds             # 链接脚本，定义内核内存布局 (.text, .data 等)
│   ├── bin/                   # 内核编译产物目录
│   │   └── kernel             # 编译链接完成的内核 ELF 可执行文件
│   └── src/                   # 内核源代码
│       ├── main.c             # 内核入口 kmain，负责初始化各个子系统
│       ├── limine.h           # Limine 引导协议定义头文件
│       ├── arch/              # 硬件架构相关代码 (x86_64)
│       │   ├── gdt.c/h        # 全局描述符表 (GDT) 初始化与刷新
│       │   ├── gdt.S          # 汇编辅助：加载 GDT
│       │   ├── idt.c/h        # 中断描述符表 (IDT) 初始化
│       │   ├── interrupts.c   # C 语言中断处理函数 (Exception/IRQ Handler)
│       │   ├── isr.S          # 中断服务程序汇编入口 (Stub)，保存现场
│       │   ├── switch.S/h     # 线程上下文切换汇编实现 (switch_to)
│       │   ├── timer.c/h      # PIT (Programmable Interval Timer) 时钟驱动
│       │   ├── trap.h         # 中断与异常向量号定义
│       │   └── x86_64.h       # 架构相关宏定义与端口操作
│       ├── drivers/           # 设备驱动程序
│       │   ├── console.c/h    # 基于 Framebuffer 的图形控制台驱动
│       │   ├── drivers.h      # 驱动子系统通用头文件
│       │   ├── font.h         # 内置位图字体数据
│       │   ├── io.h           # I/O 端口读写辅助函数
│       │   └── keyboard.c/h   # PS/2 键盘驱动与扫描码解析
│       ├── fs/                # 文件系统
│       │   └── ramfs.c/h      # 内存文件系统 (RamFS) 实现 (open, read, write)
│       ├── lib/               # 内核通用库函数
│       │   ├── elf.h          # ELF64 文件格式结构体定义
│       │   ├── list.h         # 双向循环链表实现
│       │   ├── std.h          # 标准整数类型与宏定义
│       │   └── string.c/h     # 字符串与内存操作函数 (memcpy, strlen 等)
│       ├── mm/                # 内存管理子系统
│       │   ├── debug_mm.c/h   # 内存调试辅助工具
│       │   ├── paging.c/h     # 四级页表管理与虚拟地址映射
│       │   ├── pmm.c/h        # 物理内存管理器 (基于 Bitmap)
│       │   └── vmm.c/h        # 虚拟内存区域 (VMA) 与 mm_struct 管理
│       └── proc/              # 进程管理与调度
│           ├── entry.S        # 用户态/内核态切换汇编跳板 (Trampoline)
│           ├── proc.c/h       # 进程控制块 (PCB) 管理与 ELF 加载器
│           └── sche.c/h       # 轮转调度器 (Round-Robin Scheduler) 实现
└── usr/                       # 用户态空间代码
    ├── shell.c                # 交互式 Shell 源码 (系统启动后的第一个程序)
    ├── usrmain.c              # 用户程序主入口 (调用 Shell)
    ├── bin/                   # 用户程序编译产物
    │   └── user.elf           # 编译好的用户态可执行文件
    └── lib/                   # 用户态 C 运行时库 (Mini Libc)
        ├── stdio.c/h          # 标准输入输出库 (printf, gets)
        ├── stdlib.c/h         # 标准工具库 (malloc, atoi, exit)
        ├── string.c/h         # 用户态字符串库
        └── syscall.c/h        # 系统调用封装层 (syscall wrapper)

## 5. 总结
SudoOS 不仅仅是一个简单的教学内核，它是一个**麻雀虽小，五脏俱全**的现代化操作系统原型。从底层的四级页表到上层的 Shell 交互，从内核的内存分配到用户态的进程调度，每一行代码都体现了对计算机系统底层原理的精准把控和工程实践能力。


# SudoOS 使用手册 (User Guide)

## 1. 环境准备 (Prerequisites)

在编译和运行 SudoOS 之前，请确保你的开发环境（Linux 或 macOS）已安装以下工具：

* **编译器**:
    * **Linux**: `gcc`, `ld`, `make` (通常包含在 `build-essential` 中)。
    * **macOS**: 需要交叉编译器 `x86_64-elf-gcc` 和 `x86_64-elf-ld` (可通过 Homebrew 安装)。
* **构建工具**: `xorriso` (用于打包 ISO 镜像)。
* **模拟器**: `qemu-system-x86_64` (用于运行系统)。
* **网络**: 构建脚本会自动从 GitHub 下载 Limine Bootloader，请保持网络连接。

## 2. 快速启动 (Quick Start)

项目根目录提供了自动化的 Makefile，支持一键构建与运行。

### 编译与运行
* **默认运行 (推荐)**: 编译内核并使用 QEMU (BIOS 模式) 启动。
    ```bash
    make run
    ```
* **UEFI 模式运行**: 需要系统安装了 OVMF 固件。
    ```bash
    make run-uefi
    ```
* **仅编译 ISO**: 生成 `sudoOS.iso` 但不启动模拟器。
    ```bash
    make all
    ```
* **清理构建**: 删除所有编译产物。
    ```bash
    make clean
    ```

## 3. 系统操作指南 (Operations)

系统启动后会自动进入 **SudoOS Shell**，你可以通过键盘与系统交互。

### 3.1 终端控制与翻页机制
SudoOS 的图形控制台 (`console.c`) 内置了历史缓冲区，支持查看之前的输出内容。

* **上翻页 (Scroll Up)**: 按下键盘上的 **`PageUp`** 键。
    * **效果**: 屏幕内容向下滚动，显示更早之前的历史日志（如内核启动时的初始化信息）。
* **下翻页 (Scroll Down)**: 按下键盘上的 **`PageDown`** 键。
    * **效果**: 屏幕内容向上滚动，返回最新的输出视图。
* **自动复位**: 当你在 Shell 中输入任何新字符或按下回车时，视图会自动强制跳转回最底部，确保你能看到最新的输入内容。

### 3.2 Shell 常用命令
Shell 支持基础的文件系统操作和系统调用测试。

| 命令 | 示例 | 说明 |
| :--- | :--- | :--- |
| **help** | `help` | 显示帮助菜单和系统调用号列表。 |
| **ls** | `ls /usr` | 列出目录内容。 |
| **cd** | `cd bin` | 切换当前目录。 |
| **cat** | `cat README.txt` | 查看文件内容。 |
| **clear** | `clear` | 清空屏幕（视觉清空，历史记录仍保留）。 |
| **run** | `run 0` | **系统调用调试器**。交互式调用内核功能（如输入 0 测试 SYS_READ）。 |

### 3.3 调试技巧
如果你在开发过程中需要调试内核：
1.  **修改 QEMU 参数**: 在 `GNUmakefile` 中，将 `QEMUFLAGS` 修改为包含 `-d int`，可以在终端看到中断日志。
2.  **查看中断**: 如果系统卡死，QEMU 控制台通常会打印 `cpu_reset` 或异常中断号（如 `int 13`GP, `int 14` PF）。

## limine引导

用 C 编写的一个符合 Limine 标准的小型 x86-64 内核，并使用 Limine 引导加载程序启动

- Limine 本质只是引导程序（bootloader），它的工作是：
  - 加载你的内核 ELF 文件（bin/sudoOS）
  - 切换到你指定的 CPU 状态（x86_64 long mode）
  - 为你准备好一些必要的信息（内存地图、帧缓冲、SMBIOS...）
  - 跳转到你的内核入口函数 kmain()

- 内核地址空间：0xffffffff80000000 ~ 0xffffffffffffffff

# 内存管理系统

## 内存布局

```
================================================================================
                  阶段 1: Limine 引导交接后 (Limine Page Tables)
================================================================================

      [ 虚拟地址空间 (Virtual Address Space) ]           [ 物理地址空间 (Physical Address Space) ]
      (64-bit Address Space, Canonical Form)

 0xFFFFFFFFFFFFFFFF +----------------------+             +----------------------+ <--- Max RAM
                    |                      |             |                      |
                    |   Kernel Image Map   |  mapped to  |     可用 RAM (Free)  |
 Kernel Virtual Base| (.text, .data, ...)  | ----------> |----------------------|
 (e.g. 0xFFFFFFFF8..+----------------------+             |   Limine Reclaimable |
                    |                      |             | (Memmap, Stack, etc) |
                    |                      |             |----------------------|
                    |                      |             |                      |
                    |         ...          |             |   Kernel Image Phys  | <--- Kernel Phys Base
                    |                      |             | (.text, .data, ...)  |
                    |                      |             |----------------------|
                    |                      |             |                      |
                    |                      |             |     可用 RAM (Free)  |
                    +----------------------+             |                      |
                    |                      |             +----------------------+ 0x00000000
                    |    HHDM (Limine)     |  mapped to
    HHDM Start      | All Physical Memory  | ----------> [覆盖整个物理内存 0 - Max]
 (e.g. 0xFFFF8......+----------------------+

                    |                      |
           0x00...  +----------------------+
                    |    (包含 1:1 映射)  |
                    | (Limine 可能会映射   |
                    |  前 4GB，但不保证)   |
                    +----------------------+
```

```
================================================================================
               阶段 2: kmain 初始化后 (SudoOS Custom Page Tables)
================================================================================

      [ 虚拟地址空间 (Virtual Address Space) ]           [ 物理地址空间 (Physical Address Space) ]
                                                               (由 PMM 管理)

 0xFFFFFFFFFFFFFFFF +----------------------+             +----------------------+ <--- Max RAM
                    |                      |             |                      |
                    |   Kernel Image Map   |  mapped to  |     可用 RAM (Free)  |
 Kernel Virtual Base| (RWX 权限重设/分离)  | ----------> |----------------------|
 (e.g. 0xFFFFFFFF8..+----------------------+             |      PMM Bitmap      | <--- pmm_init 分配
                    |                      |             | (管理所有页面的状态) |
                    |                      |             |----------------------|
                    |                      |             |     可用 RAM (Free)  |
                    |         ...          |             |----------------------|
                    |                      |             |  Kernel PML4 (New)   | <--- paging_init 分配
                    |                      |             |  (及 PDPT, PD 等)    |
                    |                      |             |----------------------|
                    |                      |             |   Limine Reclaimable |
                    +----------------------+             | (此时数据可能仍存在) |
                    |                      |             |----------------------|
                    |     HHDM (SudoOS)    |  mapped to  |   Kernel Image Phys  |
    HHDM Start      | All Physical Memory  | ----------> | (.text, .data, ...)  |
 (e.g. 0xFFFF8......| (重映射，用于内核访问|             |----------------------|
                    |  任意物理地址)       |             |                      |
                    +----------------------+             |     可用 RAM (Free)  |
                    |                      |             |                      |
                    | (Limine 的旧映射被   |             | (0号页通常保留/Busy) |
                    |  丢弃，低地址空间    |             +----------------------+ 0x00000000
                    |  目前为空/User Space)|
           0x00...  +----------------------+
```

## 初始化内存管理系统

- 读取memmap（物理内存探测），检测物理内存的情况
- memmap结构

```c
struct limine_memmap_response {
    uint64_t entry_count;             // 条目数量
    struct limine_memmap_entry **entries; // 指向 entries[] 数组
};

struct limine_memmap_entry {
    uint64_t base;    // 物理地址起点
    uint64_t length;  // 长度
    uint64_t type;    // 区域类型
};

```

- 物理内存的类型

```c
LIMINE_MEMMAP_USABLE               // 可以用来分配给物理内存管理
LIMINE_MEMMAP_RESERVED             // 不可用（可能被固件、BIOS 占用）
LIMINE_MEMMAP_ACPI_RECLAIMABLE     // ACPI 信息区，可在解析 ACPI 后使用
LIMINE_MEMMAP_ACPI_NVS             // ACPI NVS（不可动）
LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE
LIMINE_MEMMAP_KERNEL_AND_MODULES   // 内核自身所在内存
LIMINE_MEMMAP_FRAMEBUFFER          // 显存
```

## 物理页框管理-Bitmap

- 建立过程
  - 获取物理地址最高位。
  - 计算将物理地址空间总共分成多少页，从而计算Bitmap大小。
  - 遍历mmap，找到一个空闲区域放置
  - 通过pa+hhdm获取bitmap的虚拟地址，并初始化
  - 再次遍历mmap，这次对每一个空闲区域进行分页并设置

- Bitmap每一项为`uint8_t`类型，8位无符号整数的每一位对应一个物理页框，总共对应8个物理页框，共`8*4kB` 大小。
- Bitmap的i位对应物理地址`i*PAGE_SIZE`
- 对Bitmap的bit位进行操作：

```c
static inline void bit_set(size_t bit);

static inline void bit_unset(size_t bit);

static inline bool bit_test(size_t bit);
```

- 内存分配接口

```c
// 分配一页
uint64_t pmm_alloc_page();
// 释放一页
void pmm_free_page(uint64_t pa);
```

## 内核地址空间分区

### 1. 内核虚拟地址空间布局图

```text
虚拟地址 (Virtual Address)             内存区域 (Memory Region)              作用与存放内容
+----------------------------+-----------------------------------+------------------------------------------+
| 0xFFFFFFFF81000000 (或更高) |      内核栈区 (Kernel Stacks)      | 存放各进程/线程的内核态运行栈 (kmalloc 分配) |
+----------------------------+-----------------------------------+------------------------------------------+
| 0xFFFFFFFF80000000         |     内核镜像区 (Kernel Image)      | 存放 .text, .rodata, .data, .bss (IDT)   |
+----------------------------+-----------------------------------+------------------------------------------+
|            ...             |              未使用               | 预留空间                                 |
+----------------------------+-----------------------------------+------------------------------------------+
| 0xFFFFB00000000000         |      MMIO / Vmalloc 映射区        | 映射硬件寄存器或不连续物理内存           |
+----------------------------+-----------------------------------+------------------------------------------+
| 0xFFFF900000000000         |      内核堆区 (Kernel Heap)        | kmalloc 管理的区域，用于动态分配小对象   |
+----------------------------+-----------------------------------+------------------------------------------+
| 0xFFFF800000000000 (HHDM)  |   物理内存直接映射区 (HHDM Area)   | 直接访问所有物理内存 (如 Bitmap 数组)    |
+----------------------------+-----------------------------------+------------------------------------------+
|            ...             |            用户态空间             | 存放进程的用户代码、数据、栈 (Ring 3)    |
+----------------------------+-----------------------------------+------------------------------------------+

```

---

### 2. 各分区详细说明

#### **A. 物理内存直接映射区 (HHDM Area)**

- **起始地址：** `HHDM_OFFSET`（由 Limine 协议提供）。
- **存放内容：** 整个物理内存的副本。**Bitmap 数组** 应该通过该区域访问。
- **作用：** 内核可以通过 `虚拟地址 = 物理地址 + HHDM_OFFSET` 的简单公式直接操作物理内存，无需反复建立新页表。这对 PMM 和页表项（PTE/PDE）本身的修改至关重要。

#### **B. 内核堆区 (Kernel Heap)**

- **起始地址：**  `0xFFFF900000000000`。
- **存放内容：** 存放内核运行期间动态申请的对象，如进程控制块 (PCB)、缓冲区、动态分配的数据结构。
- **作用：** 通过双向循环链表，在此区域内进行细粒度的字节分配。当空间不足时，调用 `pmm_alloc_page` 申请页框并由 `vmm_map_page` 映射到此区域。

#### **C. MMIO / Vmalloc 映射区**

- **起始地址：**  `0xFFFFB00000000000`。
- **存放内容：** 硬件寄存器映射（如显存 Framebuffer、APIC 寄存器）或不连续物理页组成的连续虚拟内存。
- **作用：** 提供一个逻辑上连续的窗口来访问硬件或零散内存。

#### **D. 内核镜像区 (Kernel Image)**

- **起始地址：** `0xFFFFFFFF80000000`（由链接脚本 `linker.lds` 定义）。
- **存放内容：**
- `.text`：内核代码。
- `.rodata`：只读常量。
- `.data` & `.bss`：全局变量。**IDT (中断描述符表)** 和 **GDT** 通常静态分配在这里。

- **作用：** 这是内核的“核心大本营”。`paging_init` 会根据 `kernel_addr_request` 的结果将链接好的 ELF 段映射到物理内存。

#### **E. 内核栈区 (Kernel Stacks)**

- **起始地址：** 紧跟在内核镜像之后，例如 `0xFFFFFFFF81000000`。
- **存放内容：** 每个进程独立的内核态栈。
- **作用：** 处理中断或系统调用时，CPU 切换到的临时工作空间。每个栈之间通常留一个不映射的“保护页”，防止一个进程的栈溢出后破坏另一个进程。

## 内核堆内存管理



### 内存布局与配置

堆管理器定义了以下关键的内存布局参数，位于 `src/mm/pmm.h` 中：

- **基地址 (`KERNEL_HEAP_BASE`)**: `0xFFFF900000000000`。这是堆在虚拟地址空间中的起始位置。
- **页大小 (`PAGE_SIZE`)**: `4096` 字节。
- **对齐方式**: 所有分配的内存大小均向上对齐到 **8字节**。
- **最小切割阈值 (`MIN_SPLIT`)**: `16` 字节。当空闲块剩余空间小于此值时，不再进行切割，以减少碎片。

### 核心数据结构

堆内存管理基于**双向循环链表**，链表中包含**所有**内存块（包括已分配和空闲的）。每个内存块由一个头部结构 `kheap_pghdr_t` 描述。

#### 内存块头 (`kheap_pghdr_t`)

定义于 `src/mm/pmm.h`：

```c
typedef struct {
    list_node_t node;   // 链表节点 (prev, next)，用于连接前后物理相邻的内存块
    uint64_t size;      // 数据区大小 (不包含头部本身的大小)
    bool is_free;       // 状态标志：true=空闲, false=已占用
} kheap_pghdr_t;

```

- **`HEADER_SIZE`**: `sizeof(kheap_pghdr_t)`，即每个内存块的固有开销。
- **链表组织**: 所有内存块按照虚拟地址顺序链接在全局链表 `kheap_list` 中。这使得 `kfree` 可以通过检查 `node.prev` 和 `node.next` 快速找到物理上相邻的块进行合并。

### 内存分配 (`kmalloc`)

采用 **First-Fit (首次适应)** 算法：

1. **对齐**: 将请求大小 `size` 向上对齐至 8 字节。
2. **搜索**: 遍历 `kheap_list`，寻找第一个满足 `is_free == true` 且 `size >= request_size` 的块。
3. **扩容**: 如果遍历结束仍未找到合适的块，则计算所需页数并调用 `kheap_expand` 扩容堆空间，随后递归重试分配。
4. **切割 (Splitting)**: 如果找到的块大小远大于请求大小（剩余空间 `>= HEADER_SIZE + MIN_SPLIT`），则将该块分裂为两个块：

- 前半部分为已分配块。
- 后半部分为新的空闲块，并插入到链表中。

5. **标记**: 将目标块的 `is_free` 设为 `false` 并返回数据区指针。

### 内存释放 (`kfree`)

支持 **即时合并 (Immediate Coalescing)**：

1. **定位**: 根据传入指针回退 `HEADER_SIZE` 字节找到块头。
2. **标记**: 将块状态设为 `is_free = true`。
3. **向后合并**: 检查 `node.next`。如果下一个块存在且空闲，并且地址连续，则将其吞并（增加当前块大小，从链表移除下一个块）。
4. **向前合并**: 检查 `node.prev`。如果前一个块存在且空闲，并且地址连续，则将当前块并入前一个块。

### 堆扩容 (`kheap_expand`)

当堆空间不足时，自动向高地址扩展：

1. **物理分配**: 调用 `pmm_alloc_page` 申请新的物理页。
2. **虚拟映射**: 调用 `vmm_map_page` 将新页映射到 `kheap_top` 指向的虚拟地址，属性为 `PTE_PRESENT | PTE_RW`。
3. **初始化**: 在新页起始处构建 `kheap_pghdr_t`，初始状态设为 `is_free = 0` (占用)。
4. **链表插入**: 使用 `list_add_before` 将新块插入到 `kheap_list` 尾部。
5. **触发合并**: 调用 `kfree` 释放这个新块。利用 `kfree` 的合并逻辑，如果新页与之前的堆顶在物理/逻辑上连续，它们会自动合并成一个更大的空闲块。
6. **更新边界**: `kheap_top` 指针增加 `PAGE_SIZE`。

### API 接口说明

以下接口声明在 `src/mm/pmm.h` 中：

#### `void kheap_init(size_t init_pages)`

- **功能**: 初始化内核堆管理器。
- **参数**: `init_pages` - 初始预分配的物理页数量。
- **实现细节**: 初始化 `kheap_list` 哨兵节点，设定 `kheap_top` 为基地址，并调用 `kheap_expand` 进行初始扩容。

#### `void* kmalloc(size_t size)`

- **功能**: 申请内核堆内存。
- **参数**: `size` - 请求的字节数。
- **返回**: 成功返回指向数据区的指针，失败返回 `NULL`。
- **依赖**: `first_fit`, `kheap_expand`。

#### `void kfree(void* ptr)`

- **功能**: 释放堆内存。
- **参数**: `ptr` - 由 `kmalloc` 返回的指针。如果为 `NULL` 则直接返回。
- **注意**: 包含双重释放检查（虽然当前实现仅打印日志或直接返回，建议在调试模式下Panic）。

## 4级分页系统

- 将一个 48位 的虚拟地址（Virtual Address），切分成 4 段索引（每段 9 位）和 1 段偏移量（12 位）
- 虽然指针是 64 位的，但标准的 4 级分页只使用了低 48 位。高 16 位必须进行符号扩展（Canonical Form，通常全是0或全是1）。

### 虚拟地址的结构

```
63          48 47     39 38     30 29     21 20     12 11         0
+-------------+---------+---------+---------+---------+------------+
| Sign Extend | PML4    | PDPT    | PD      | PT      | Offset     |
| (不可用)    | Index   | Index   | Index   | Index   |            |
+-------------+---------+---------+---------+---------+------------+
       |           |         |         |         |          |
    必须为全0      9 bits    9 bits    9 bits    9 bits     12 bits
    或全1        (0~511)   (0~511)   (0~511)   (0~511)    (0~4095)
```

### 各级页表结构

- 每一级页表都刚好占用 4KB（一个物理页），并且包含 512 个条目（Entries）。每个条目 8 字节（64位）。

- CR3 寄存器: 存储顶级页表（PML4）的物理基地址

```
CR3 Register (存放 PML4 物理地址)
            |
            v
  +----------------------+
  |     PML4 Table       |  <-- 1. 用虚拟地址 Bits 39-47 (PML4 Index)
  | (Page Map Level 4)   |         找到第 N 个条目 (PML4E)
  +----------------------+
            |
            | PML4E 中包含下一级表的物理地址
            v
  +----------------------+
  |     PDPT Table       |  <-- 2. 用虚拟地址 Bits 30-38 (PDPT Index)
  | (Page Dir Pointer)   |         找到第 N 个条目 (PDPTE)
  +----------------------+
            |
            | PDPTE 中包含下一级表的物理地址
            v
  +----------------------+
  |      PD Table        |  <-- 3. 用虚拟地址 Bits 21-29 (PD Index)
  |   (Page Directory)   |         找到第 N 个条目 (PDE)
  +----------------------+
            |
            | PDE 中包含下一级表的物理地址
            v
  +----------------------+
  |      PT Table        |  <-- 4. 用虚拟地址 Bits 12-20 (PT Index)
  |     (Page Table)     |         找到第 N 个条目 (PTE)
  +----------------------+
            |
            | PTE 中包含最终物理页框的基地址 (Frame Address)
            v
    +------------------+
    |  Physical Page   |
    |   (4KB Frame)    |
    +------------------+
            ^
            |
            +--- 5. 加上虚拟地址 Bits 0-11 (Offset) = 最终物理地址
```

### 页表项结构 (PTE Structure)

无论是 PML4E, PDPTE, PDE 还是 PTE，它们的结构在硬件上高度相似。 这是一个标准的 64位 页表项结构图：

```
63 (NX)   51           12 11   9 8 7 6 5 4 3 2 1 0
+-----------+------------+------+-+-+-+-+-+-+-+-+-+
| NX Bit    | Physical   | Avail|G|S|D|A|P|P|U|R|P|
| (No Exec) | Address    |      | |Z| | |C|W|/|/| |
|           | (Frame)    |      | | | | |D|T|S|W| |
+-----------+------------+------+-+-+-+-+-+-+-+-+-+
      |            |         |     | | | | | | | | +-> Present (1=存在)
      |            |         |     | | | | | | | +---> Read/Write (1=可写)
      |            |         |     | | | | | | +-----> User/Super (1=用户态)
      |            |         |     | | | | | +-------> Write Through
      |            |         |     | | | | +---------> Cache Disable
      |            |         |     | | | +-----------> Accessed (CPU自动置1)
      |            |         |     | | +-------------> Dirty (仅PT有效, 写入后置1)
      |            |         |     | +---------------> Page Size (1=2MB大页)
      |            |         |     +-----------------> Global (TLB刷新不清除)
      |            |         +-----------------------> Available (OS自定义使用)
      |            +---------------------------------> 下一级表的物理基地址 (4KB对齐)
      +----------------------------------------------> No Execute (1=禁止执行代码)
```
---

# 内核线程模型

本设计实现了一个基于 **1:1 模型** 的内核级线程（Kernel Thread）管理系统。内核线程是操作系统调度的基本单位，拥有独立的内核栈和硬件上下文，但共享内存地址空间（`mm_struct`）。

* **PCB 管理**：基于 `pcb_t` 结构体管理进程/线程状态。
* **抢占式调度**：基于时间片的轮转调度算法（Round-Robin）。
* **生命周期管理**：支持线程创建、运行、退出（僵尸态）和资源回收。
* **上下文切换**：基于软件的栈切换与硬件状态保存。

## 2. 数据结构设计 (Data Structure Design)

### 2.1 进程控制块 (PCB)

`pcb_t` 是核心数据结构，定义于 `proc.h`。

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `pid` | `int` | 唯一进程标识符，由 `next_pid` 自增生成。 |
| `name` | `char[]` | 线程名称，最大长度 32 字节。 |
| `proc_state` | `proc_state_t` | 当前状态 (RUNNING, READY, BLOCKED, ZOMBIE)。 |
| `rsp` | `uint64_t` | 上下文切换时的内核栈顶指针。 |
| `kstack_base` | `uint64_t` | 内核栈的基地址（低地址），用于释放内存。 |
| `context` | `context_t*` | 指向栈上保存的寄存器上下文的指针。 |
| `mm` | `mm_struct*` | 内存地址空间指针（内核线程共享父进程的 mm）。 |
| `parent` | `pcb_t*` | 父进程指针。 |
| `proc_list_node` | `list_node_t` | 链接到全局进程链表 `proc_list`。 |
| `sched_node` | `list_node_t` | 链接到就绪队列 `ready_queue`。 |
| `exit_code` | `int` | 线程退出时的返回值。 |

### 2.2 全局链表

* **`proc_list`**：包含系统中所有的 PCB（包括运行中、就绪、阻塞和僵尸进程）。
* **`ready_queue`**：仅包含处于 `PROC_READY` 状态，等待被 CPU 调度的进程。

---

## 3. 核心机制与状态机

### 3.1 线程状态流转

```mermaid
graph LR
    Create((创建)) --> READY
    READY -->|schedule| RUNNING
    RUNNING -->|时间片耗尽| READY
    RUNNING -->|kthread_exit| ZOMBIE
    ZOMBIE -->|free_proc| Destroy((销毁))

```

* **READY**: 已分配资源，在 `ready_queue` 中等待调度。
* **RUNNING**: 当前正在 CPU 上执行（`current_proc` 指向该线程）。
* **ZOMBIE**: 线程已退出，但内核栈和 PCB 尚未回收。
* **BLOCKED**: (预留) 等待外部事件。

### 3.2 僵尸进程机制

为了解决“线程无法释放当前正在使用的栈”这一悖论，采用了 **Exit-Zombie-Reap** 模型：

1. **Exit**: 线程调用 `kthread_exit`，状态变为 `ZOMBIE`，移出就绪队列，但保留内核栈，随后主动让出 CPU。
2. **Reap**: 父进程或 Idle 线程通过 `free_proc` 清理 `ZOMBIE` 状态的线程，释放其物理内存和 PCB。

---

## 4. 详细接口设计与实现

### 4.1 初始化 (`proc_init`)

**功能**：初始化进程管理子系统，创建 Idle 进程。
**实现步骤**：

1. 初始化全局链表 `proc_list` 和 `ready_queue`。
2. 通过 `alloc_new_pcb` 分配 Idle 进程的 PCB。
3. 设置 Idle 进程名为 "idle"，状态为 `PROC_RUNNING`。
4. 将 `current_proc` 和 `idle_proc` 指向该 PCB。
5. 将 Idle 进程加入 `proc_list`。

### 4.2 线程创建 (`kthread_create`)

**功能**：创建一个新的内核线程，并伪造其上下文使其能被 `schedule` 调度。
**参数**：

* `parent`: 父进程 PCB。
* `name`: 线程名。
* `kthread_func`: 线程入口函数指针。
* `arg`: 传递给入口函数的参数。

**实现步骤**：

1. **分配 PCB**：调用 `alloc_new_pcb`，分配内存并清零。
2. **继承资源**：共享父进程的 `mm` 指针。
3. **分配内核栈**：调用 `kstack_init(KSTACK_SIZE)` 分配物理页并映射，记录 `kstack_base`。
4. **伪造上下文 (Context Forging)**：
* 计算栈顶 `rsp`。
* 在栈顶预留 `context_t` 空间。
* 设置 `context->rip` 指向 **`kernel_thread_entry`** (汇编跳板)。
* 设置 `context->rdi` 为 `arg` (作为第一个参数)。
* 设置 `context->rbx` 为 `kthread_func` (被跳板调用)。


5. **加入队列**：将新线程状态设为 `PROC_READY`，加入 `proc_list` 和 `ready_queue`。

### 4.3 线程入口跳板 (`kernel_thread_entry`)

**位置**：`proc/entry.S`
**设计意图**：统一所有内核线程的启动和退出行为。
**流程**：

1. `sti`：开中断（新线程启动时默认开中断）。
2. `call *%rbx`：调用实际的线程函数（创建时存入 `rbx`）。
3. `call kthread_exit`：如果线程函数返回，自动调用退出函数，防止跑飞。

### 4.4 调度器 (`schedule`)

**位置**：`proc/sche.c`
**功能**：基于时间片的轮转调度。
**实现步骤**：

1. **保护现场**：读取 `RFLAGS` 保存中断状态，执行 `cli()` 关中断。
2. **处理当前进程 (`prev`)**：
* 若为 `RUNNING` 且时间片耗尽 (`<=0`)：状态改为 `READY`，重置时间片，移至 `ready_queue` 尾部。
* 若时间片未耗尽：直接返回 (`sti` 恢复中断)。


3. **选择下一个进程 (`next`)**：
* 若 `ready_queue` 为空：选择 `idle_proc`。
* 若不为空：取出队头节点，从队列移除，获取其 PCB。


4. **上下文切换**：
* 若 `prev != next`：
* 设置 `next` 为 `RUNNING`，更新 `current_proc`。
* **TSS 更新**：`set_tss_stack(next->rsp)`，确保下一次中断能正确找到内核栈。
* **页表切换**：若 `mm` 不同，执行 `lcr3`。
* **汇编切换**：`switch_to(&prev->context, next->context)`。


5. **恢复现场**：恢复中断状态 (`sti`)。

### 4.5 线程退出 (`kthread_exit`)

**功能**：终止当前线程运行。
**实现步骤**：

1. `cli()` 关中断。
2. 将当前进程状态设为 `PROC_ZOMBIE`。
3. 设置 `exit_code`。
4. **关键**：调用 `schedule()`。调度器会发现当前进程不再是 `RUNNING` 且不在就绪队列中，从而不再调度它，永久切出。

### 4.6 资源回收 (`free_proc`)

**功能**：回收僵尸进程占用的物理资源。
**注意**：此函数不能由目标进程自己调用。
**实现步骤**：

1. 检查进程状态是否为 `PROC_ZOMBIE`。
2. 从 `proc_list` 和 `ready_queue` (如果有) 中移除节点。
3. **释放内核栈**：调用 `kstack_free` 释放对应的物理页框。
4. **释放 PCB**：调用 `kfree`。


### 4.7 进程分类

SudoOS 支持两种类型的执行流，它们共享相同的 PCB 结构，但内存视图不同：

1. **内核线程 (Kernel Thread)**:
* **运行级别**: Ring 0。
* **内存空间**: 共享内核的 PML4 页表，没有独立的用户空间映射。
* **创建方式**: `kthread_create()`。
* **典型例子**: `Idle` 进程（PID 0）。


2. **用户进程 (User Process)**:
* **运行级别**: 用户态运行在 Ring 3，陷入内核后运行在 Ring 0。
* **内存空间**: 拥有独立的 `mm_struct` 和 PML4 页表。拥有独立的虚拟地址空间（代码段、数据段、堆、栈）。
* **创建方式**: `create_user_process()` 加载 ELF 文件。
* **典型例子**: `init` 进程（PID 1）。



### 4.8 调度策略

* **算法**: 时间片轮转调度 (Round-Robin)。
* **机制**: `src/proc/sche.c` 维护一个 `ready_queue` (双向循环链表)。
* **触发**:
    1. **主动调度**: 进程阻塞或退出时调用 `schedule()`。
    2. **抢占调度**: 时钟中断 (`timer_callback`) 递减 `time_slice`，减为 0 时强制调用 `schedule()`。


---


# 用户地址空间管理 (VMM) 

## 1. 模块概述

本模块负责管理用户进程的虚拟内存。

1. **生命周期管理**：创建 (`mm_create`) 和销毁 (`mm_destroy`) 进程的地址空间。
2. **内核共享**：确保所有用户进程的页表高半部分（Kernel Space）正确映射到内核，使中断和系统调用能正常工作。
3. **区域管理 (VMA)**：记录用户空间中哪些地址是合法的（代码段、数据段、堆、栈），以及它们的权限。


## 2. 数据结构设计

### 2.1 `struct vma_struct` (虚拟内存区域)

代表一段连续的虚拟地址范围（例如：代码段 0x400000 - 0x401000）。

```c
// src/mm/vmm.h

struct vma_struct {
    list_node_t list_node;   // 链表节点，用于串联该进程的所有 VMA
    struct mm_struct *mm;    // 反向指针，指向所属的 mm_struct
    
    uint64_t vm_start;       // 起始虚拟地址 (Page Aligned)
    uint64_t vm_end;         // 结束虚拟地址 (vm_start + size)
    uint64_t vm_flags;       // 权限标志 (VM_READ, VM_WRITE, VM_EXEC 等)
    uint64_t vm_file_offset; // (可选) 如果是从文件加载的，记录文件偏移
    // struct file *vm_file; // (进阶) 如果是内存映射文件 mmap
};

```

### 2.2 `struct mm_struct` (内存描述符)

代表一个进程完整的地址空间。

```c
// src/mm/vmm.h

struct mm_struct {
    pg_table_t* pml4;        // 【关键】该进程的顶级页表虚拟地址
    uint64_t pml4_pa;        // 【新增】顶级页表的物理地址 (方便 context switch 时 load cr3)

    list_node_t vma_list;    // VMA 链表头
    struct vma_struct* mmap_cache; // 最近一次访问的 VMA (缓存优化查询速度)

    // 数据统计
    int map_count;           // VMA 的数量
    int ref_count;           // 引用计数 (用于多线程共享 mm)

    // 关键段边界 (用于 brk, stack extend 等)
    uint64_t start_code, end_code;
    uint64_t start_data, end_data;
    uint64_t start_brk, brk;       // 堆的起始和当前顶端
    uint64_t start_stack;          // 栈底
};

```

---

## 3. 核心函数原型


```c
// ================= API =================

// 1. 创建一个新的地址空间 (用于 fork 或 exec)
struct mm_struct * mm_alloc();

// 2. 销毁地址空间 (用于 exit)
void mm_free(struct mm_struct *mm);

// 4. 在地址空间中映射一段区域 (用于 load_elf)
// 包含：申请 VMA + 申请物理页 + 修改页表映射
bool mm_map_range(struct mm_struct *mm, uint64_t va, size_t size, uint64_t vm_flags);

```


#  内核启动全流程 (Bootstrapping Flow)

### 阶段一：引导程序 (Limine Bootloader)

1. **加载内核**: Limine 将 `kernel` ELF 文件加载到物理内存。
2. **协议请求**: 处理 `limine_requests` (HHDM, Memmap, Framebuffer 等)。
3. **进入内核**: 跳转到内核入口点 `kmain` (`src/main.c`)。此时 CPU 处于 64 位长模式，分页已开启，但使用的是 Limine 提供的临时页表。

### 阶段二：内核初始化 (`kernel_init`)

内核接管硬件控制权，建立自己的执行环境。

1. **GDT & IDT 初始化**:
* `gdt_init()`: 建立新的 GDT，定义内核代码段/数据段、用户代码段/数据段以及 **TSS**。
* `idt_init()`: 建立中断描述符表，重映射 PIC (8259A)，注册异常处理函数（如 Page Fault）和系统调用（Int 0x80）。


2. **NX 位开启**: 调用 `enable_nx()` 开启页表不可执行位支持，防止缓冲区溢出攻击。
3. **内存管理初始化 (`mm_init`)**:
* **PMM**: 解析 Limine Memmap，初始化物理页分配器（位图法）。
* **Paging**: 创建内核自己的 PML4 页表 (`kernel_pml4`)，映射内核代码、HHDM 区域。**关键动作：切换 CR3 到内核页表。**
* **Heap**: 初始化内核堆 (`kmalloc`)。
* **Stack**: 重新分配并切换到一个更大的内核栈。


4. **进程子系统初始化 (`proc_init`)**:
* 创建 **Idle 进程 (PID 0)**。这是系统第一个 PCB，代表内核主循环。
* 设置 `current_proc = idle`。


5. **加载初始用户程序**:
* 从 Limine Module 请求中获取 `init` 程序（ELF 格式）的数据。
* 调用 `init_userproc()` -> `create_user_process()`。



### 阶段三：第一个用户进程的创建 (`create_user_process`)

这是最复杂的环节之一，涉及内存视图的构建和 ELF 加载。

1. **分配 PCB**: `alloc_new_pcb()` 分配内存，PID = 1。
2. **创建地址空间 (`mm_alloc`)**:
* 分配一个新的 PML4。
* **复制内核映射**: 将内核的高半核映射（256-511 项）复制到新页表。保证陷入内核时能访问内核代码。


3. **加载 ELF (`load_elf`)**:
* **临时切换页表**: 执行 `lcr3(proc->mm->pml4_pa)`。因为我们要往用户地址（如 `0x400000`）写数据，必须激活目标页表。
* **解析 Segment**: 遍历 ELF Program Header，找到 `PT_LOAD` 段。
* **映射内存**: `mm_map_range` 分配物理页并映射到虚拟地址（设置 `VM_WRITE` 权限）。
* **拷贝数据**: `memcpy` 将指令和数据从 ELF 镜像拷贝到物理内存。
* **恢复页表**: 切回原来的 CR3。


4. **分配栈**:
* **用户栈**: 映射 `USER_STACK_TOP` 向下的区域。
* **内核栈**: `kstack_init` 分配 16KB，用于该进程在内核态执行（中断/Syscall）。


5. **构建伪造的上下文 (Context Forging)**:
* 我们需要“欺骗”调度器，让它以为这个进程是“被暂停”的。
* 在内核栈顶构建 `context_t`。
* `context->rip` = `user_entry` (一个汇编跳板函数)。
* `context->r12` = ELF 入口点 (e.g., `0x400000`)。
* `context->r13` = 用户栈顶。
* `proc->context` 指向这个伪造的栈顶。


6. **入队**: 将 PID 1 加入 `ready_queue`。

### 阶段四：从内核态跳跃到用户态 (The Context Switch)

此时，CPU 还在运行 `kmain` 的 `idle` 线程。

1. **开启中断**: `kmain` 调用 `sti`。
2. **触发调度**: `kmain` 调用 `schedule()`。
3. **上下文切换 (`switch_to`)**:
* 调度器选中 PID 1。
* **切换 CR3**: 加载 PID 1 的页表。此时用户空间的映射（0x400000 等）变得可见。
* **切换 TSS RSP0**: 设置 `tss.rsp0` 为 PID 1 的内核栈底。**这至关重要**，否则下次中断发生时 CPU 无处保存状态。
* `switch_to` 保存 Idle 的寄存器，加载 PID 1 的寄存器。
* `ret` 指令弹出 `rip`。此时 CPU 跳转到 `user_entry`。


4. **执行跳板函数 (`user_entry`)**:
* 从 `r12` 读取 ELF 入口地址。
* 从 `r13` 读取用户栈地址。
* 调用 `enter_user_mode(entry, stack)`。


5. **特权级切换 (`iretq`)**:
* `enter_user_mode` 手动在栈上构建 **中断返回帧 (Interrupt Return Stack Frame)**：
* `SS` (User Data Selector, 0x23)
* `RSP` (用户栈顶)
* `RFLAGS` (开启 IF 中断位)
* `CS` (User Code Selector, 0x1B)
* `RIP` (ELF 入口点)


* 执行 `iretq`。
* **CPU 硬件动作**: 从栈中弹出上述值，将 `CPL` (当前特权级) 从 0 变为 3，跳转到用户代码。



**至此，第一个用户进程 `init` 开始运行，屏幕打印 "Hello, User World!"。**

---

## 系统调用路径 (System Call Path)概述

当用户程序调用 `print` 时：

1. **用户态**: 执行 `int 0x80` 指令。
2. **硬件**:
* 检查 IDT 第 128 项。
* 切换到 Ring 0。
* 从 TSS 读取 `RSP0`，切换到内核栈。
* 压入用户态的 SS, RSP, RFLAGS, CS, RIP。


3. **内核态 (`isr_stub` -> `isr_handler`)**:
* `pushall` 保存通用寄存器。
* 调用 C 函数 `syscall_handler`。
* 根据 `rax` 执行逻辑（如 `kprint`）。


4. **返回**:
* `popall` 恢复寄存器。
* `iretq` 返回用户态。



---

## 内存布局图示

```text
+-------------------------+ 0xFFFFFFFF FFFFFFFF
|                         |
|       Kernel Image      | .text, .data (Higher Half)
|                         |
+-------------------------+ 0xFFFF8000 00000000 (HHDM Start)
|                         |
|      Direct Map (HHDM)  | 所有物理内存的直接映射
|                         |
+-------------------------+ 
|           ...           |
+-------------------------+ 0x00007FFF FFFFFFFF (User Space End)
|                         |
|       User Stack        | 0x00000000 80000000 (向下生长)
|           |             |
|           v             |
|                         |
|           ^             |
|           |             |
|       User Heap         | (动态分配)
|                         |
+-------------------------+
|       User Code         | 0x00000000 00400000 (ELF Load Addr)
+-------------------------+ 0x00000000 00000000

```

# 硬件抽象层 (Architecture Layer)

SudoOS 的架构层代码位于 `kernel/src/arch/`，专门针对 **x86_64** 架构实现。该层负责屏蔽底层硬件细节，提供中断管理、特权级切换、上下文保存与恢复等核心机制。

## 1. 全局描述符表 (GDT) 与 TSS

**源文件**: `kernel/src/arch/gdt.c`, `kernel/src/arch/gdt.S`
**头文件**: `kernel/src/arch/gdt.h`

虽然 x86_64 弱化了分段机制，但 GDT 仍然是切换特权级（Ring 0 <-> Ring 3）和加载 TSS 的必要条件。

### 1.1 段描述符布局
内核定义了以下 5 个关键段描述符（加上 Null 段共 6 个）：

* **Null Descriptor**: 索引 0，必须为全 0。
* **Kernel Code (0x08)**: 64位代码段，DPL=0，用于内核指令执行。
* **Kernel Data (0x10)**: 数据段，DPL=0，用于内核数据访问。
* **User Data (0x23)**: 数据段，DPL=3，用于用户栈和堆。注意选择子低 3 位为 `11b` (RPL=3)。
* **User Code (0x1B)**: 64位代码段，DPL=3，用于用户程序执行。
* **TSS (Task State Segment)**: 系统段，用于保存中断时的栈指针。

### 1.2 TSS 的关键作用
在 x86_64 中，TSS 不再用于保存任务上下文（寄存器等），其唯一的核心用途是保存 **RSP0**。
* **场景**: 当 CPU 处于 Ring 3（用户态）并接收到中断或系统调用时，必须切换到 Ring 0。
* **机制**: 硬件会自动从 TR 寄存器指向的 TSS 中读取 `RSP0` 字段，并将栈指针 `RSP` 切换到该地址。
* **代码体现**: `gdt.c` 中的 `write_tss` 和 `set_tss_stack` 函数负责动态更新这个栈地址（通常在进程调度 `schedule()` 时更新为下一个进程的内核栈基址）。

---

## 2. 中断描述符表 (IDT)

**源文件**: `kernel/src/arch/idt.c`
**头文件**: `kernel/src/arch/idt.h`, `kernel/src/arch/trap.h`

IDT 定义了 CPU 如何响应异常（Exception）和外部中断（Interrupt）。

### 2.1 向量表布局
SudoOS 将 256 个中断向量划分为三个区域：

1.  **异常 (0 - 31)**: CPU 内部错误。
    * 例如：`#PF (14)` 页错误，`#GP (13)` 一般保护性错误，`#DF (8)` 双重错误。
2.  **外部中断 (32 - 47)**: 由 PIC (8259A) 芯片重映射而来。
    * `IRQ 0` -> 向量 32 (时钟)
    * `IRQ 1` -> 向量 33 (键盘)
3.  **系统调用 (128 / 0x80)**: 用户态陷入内核的软件中断入口。

### 2.2 初始化流程
`idt_init()` 函数执行以下步骤：
1.  **重映射 PIC**: 将主片偏移设为 32，从片偏移设为 40，防止 IRQ 与 CPU 异常冲突。
2.  **填充 IDT**: 循环设置 256 个门描述符，指向 `isr_stub_table` 中的汇编入口。
3.  **加载 IDTR**: 执行 `lidt` 指令。

---

## 3. 中断处理流程 (ISR & Dispatch)

**汇编入口**: `kernel/src/arch/isr.S`
**C 处理函数**: `kernel/src/arch/interrupts.c`

SudoOS 采用“汇编存根 + C 分发器”的两级处理模型。

### 3.1 第一级：汇编存根 (`isr.S`)
对于每个中断向量，汇编宏 `ISR_NOERRCODE` 或 `ISR_ERRCODE` 会生成一个微型入口：

```asm
isr_stub_0:
    push 0          ; 压入伪错误码 (为了栈对齐)
    push 0          ; 压入中断号
    jmp isr_common_stub
```

`isr_common_stub` 负责保存现场（Context Save）：
1.  **保存寄存器**: `pushall` 宏将 `r15-r8`, `rbp`, `rdi`, `rsi`, `rdx`, `rcx`, `rbx`, `rax` 全部压栈。
2.  **切换段寄存器**: 将 `ds`, `es` 设为内核数据段 (`0x10`)。
3.  **调用 C 函数**: `call isr_handler`，此时栈顶指针 `rsp` 作为参数传递给 C 函数（指向 `registers_t` 结构体）。
4.  **恢复现场**: 函数返回后，`popall` 恢复所有寄存器，最后 `iretq` 返回。

### 3.2 第二级：C 分发器 (`interrupts.c`)
`isr_handler(registers_t* regs)` 是所有中断的统一 C 入口：

* **异常分发**: 如果 `int_no < 32`，打印错误信息（如 Page Fault 地址）并暂停系统 (`hcf`)，或者调用注册的异常处理回调。
* **IRQ 分发**: 如果 `int_no >= 32`，调用 `interrupt_handlers[int_no]` 中注册的驱动回调（如 `timer_callback` 或 `keyboard_handler`）。处理完毕后发送 EOI (End of Interrupt) 信号给 PIC。

---

## 4. 上下文切换 (Context Switch)

**源文件**: `kernel/src/arch/switch.S`

这是多任务调度的核心引擎，用于在两个内核线程之间切换执行流。

### `switch_to` 函数原理
C 原型：`void switch_to(struct context **prev, struct context *next);`

由于这是主动切换（Cooperative），编译器生成的代码已经处理了 Caller-saved 寄存器。`switch_to` 只需要保存 **Callee-saved** 寄存器：

```asm
switch_to:
    ; 1. 保存当前进程 (prev) 的上下文到它的内核栈
    push rbx
    push rbp
    push r12
    push r13
    push r14
    push r15

    ; 2. 将当前的栈指针 (rsp) 保存到 prev->context 变量中
    ; rdi 是第一个参数 (struct context **prev)
    mov [rdi], rsp

    ; 3. 加载下一个进程 (next) 的栈指针
    ; rsi 是第二个参数 (struct context *next)
    mov rsp, rsi

    ; 4. 从新进程的栈中恢复上下文
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbp
    pop rbx

    ; 5. 跳转执行
    ; ret 指令会弹出栈顶的 RIP (即新进程上次调用 switch_to 后的返回地址)
    ret
```

---

## 5. 系统时钟 (Timer)

**源文件**: `kernel/src/arch/timer.c`
**头文件**: `kernel/src/arch/timer.h`

* **硬件**: 编程 8253/8254 PIT (Programmable Interval Timer) 芯片。
* **配置**: 使用模式 3 (方波发生器)，频率设置为 100Hz (10ms 一个 Tick)。
* **中断处理**: 注册 `timer_callback` 到 IRQ 0。
    * 该函数不仅增加系统 tick 计数。
    * 更重要的是调用 `schedule()` 检查当前进程时间片 (`current_proc->time_slice`)，如果耗尽则触发抢占。

## 6. 底层辅助 (x86_64 Helper)

**头文件**: `kernel/src/arch/x86_64.h`

提供了 C 语言无法直接表达的汇编指令封装：
* **端口 I/O**: `outb`, `inb`, `io_wait`。
* **控制寄存器**: `read_cr2()` (读取缺页地址), `write_cr3()` (切换页表), `read_rflags()` (读取中断状态)。
* **MSR 操作**: `rdmsr`, `wrmsr` (用于开启 NX 位或系统调用相关设置)。
---

# 设备驱动子系统 (Device Drivers)

SudoOS 目前实现了两个核心驱动：基于 Framebuffer 的图形控制台 (`console.c`) 和 PS/2 键盘驱动 (`keyboard.c`)。

位于 `kernel/src/drivers/` 目录。

## 1. 图形控制台驱动 (Framebuffer Console)

**源文件**: `kernel/src/drivers/console.c`

SudoOS 摒弃了传统的 VGA 文本模式 (0xB8000)，直接运行在高分辨率图形模式（如 1920x1080, 32位色深）下。这意味着内核必须像画图软件一样，通过修改内存中的像素数据来“画”出每一个字符。

### 1.1 核心原理：像素是如何画在屏幕上的？

屏幕在操作系统眼中，本质上只是一块巨大的内存区域（Framebuffer）。

1.  **物理内存映射**: Limine 引导协议在启动时会告诉内核这块显存的物理地址 (`g_fb->address`)、宽度 (`width`)、高度 (`height`) 以及每行占用的字节数 (`pitch`)。
2.  **定位像素**: 要在坐标 `(x, y)` 画一个点，必须计算它在内存中的线性偏移量。

**代码深度解析 (`put_pixel_scaled`)**:
在 `console.c` 中，绘制像素的核心逻辑如下：

```c
// 1. 获取显存基地址 (转换为 uint32_t* 指针，因为每个像素占 4 字节/32位)
uint32_t* fb_addr = (uint32_t*)g_fb->address;

// 2. 计算显存的“跨度” (Pitch)
// Pitch 是显存中一行的总字节数（可能包含硬件对齐填充）。
// 除以 4 是为了将其转换为“以像素为单位”的步长。
uint32_t pitch_pixels = g_fb->pitch / 4;

// 3. 绘制逻辑 (支持 SCALE 放大倍数)
// 我们不仅画一个点，而是画一个 SCALE * SCALE 的矩形块，这样字体看起来更大。
for (int dy = 0; dy < SCALE; dy++) {
    // 计算目标行的内存指针：
    // 基址 + (起始Y + 偏移Y) * 每行像素数 + 起始X
    uint32_t* row_ptr = fb_addr + (start_y + dy) * pitch_pixels + start_x;
    
    for (int dx = 0; dx < SCALE; dx++) {
        // 直接写入颜色值 (例如白色 0xFFFFFFFF)
        row_ptr[dx] = color; 
    }
}
```

### 1.2 字体渲染 (Font Rendering)

由于是图形模式，没有硬件帮我们显示 "A" 这个字。我们需要内置一套位图字库 (`font.h`)。

**绘制流程 (`draw_char_on_screen`)**:
1.  **获取字模**: 读取字符对应的 8x8 位图数组 (`font8x8_basic[c]`)。
2.  **扫描位图**: 双重循环遍历这 64 个点。
3.  **着色**:
    * 如果某一位是 `1`: 画前景色 (`FG_COLOR`, 白色)。
    * 如果某一位是 `0`: 画背景色 (`BG_COLOR`, 黑色)。这一步至关重要，否则文字会重叠在旧内容上。

### 1.3 屏幕滚动与历史回溯

驱动维护了一个 `history_buffer`（字符数组），用于记录最近 100 行的文本内容。

* **逻辑滚动**: 当光标 `g_cursor_y` 超过屏幕行数时，并不立即移动显存数据（因为太慢），而是通过 `g_view_offset` 控制视图。
* **物理刷新 (`console_refresh`)**: 当需要翻页或发生剧烈变动时，`console_refresh` 会根据当前的 `history_buffer` 和 `g_view_offset` 重新计算并绘制整个屏幕的每一个字符。这比显存拷贝 (`memcpy`) 慢，但逻辑更清晰。

---

## 2. 键盘驱动 (PS/2 Keyboard)

**源文件**: `kernel/src/drivers/keyboard.c`

SudoOS 使用中断驱动的方式处理键盘输入，避免了 CPU 轮询带来的资源浪费。

### 2.1 硬件交互机制
* **端口**: `0x60` (数据端口)。
* **中断**: `IRQ 1` (映射到 IDT 向量 33)。

### 2.2 驱动工作流 (`keyboard_handler`)

每当你按下一个键，键盘控制器就会向 CPU 发送一个中断信号，触发以下流程：

1.  **读取扫描码**:
    ```c
    uint8_t scancode = inb(0x60); // 从 IO 端口直接读取硬件数据
    ```
2.  **特殊键处理**:
    * **断开码 (Break Code)**: 如果 `scancode & 0x80` 为真（最高位是1），说明是按键**松开**（Key Up），通常忽略（除非是 Shift 键松开）。
    * **Shift 键**: 检测到 `0x2A/0x36` (按下) 设置 `g_shift = true`；检测到 `0xAA/0xB6` (松开) 设置 `g_shift = false`。
    * **翻页键**: 检测到 PageUp (`0x49`)，调用 `console_scroll(10)` 实现终端向上滚屏查看历史。
3.  **字符映射**:
    使用查找表 `kmap_low` (无Shift) 或 `kmap_up` (有Shift) 将扫描码转换为 ASCII 字符。例如扫描码 `0x1E` 会被转换为字符 `'a'` 或 `'A'`。
4.  **环形缓冲区 (Circular Buffer)**:
    解析出的字符被存入 `kbuf`。这是一个生产者-消费者模型：中断处理函数是生产者，用户态的 `read()` 系统调用是消费者。

---

## 3. I/O 端口抽象 (Port I/O)

**源文件**: `kernel/src/drivers/io.h`

x86 架构使用独立的 I/O 地址空间来与外设通信。C 语言本身不支持 `in/out` 指令，因此必须通过内联汇编封装。

```c
// 向端口 port 写入一个字节 val
// "a"(val) -> 将 val 放入 al 寄存器
// "Nd"(port) -> 将 port 放入 dx 寄存器
static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ( "outb %0, %1" : : "a"(val), "Nd"(port) );
}

// 从端口 port 读取一个字节
static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ( "inb %1, %0" : "=a"(ret) : "Nd"(port) );
    return ret;
}
```

这些底层函数是键盘驱动 (`inb(0x60)`) 和 PIC 初始化 (`outb(0x21, ...)`) 的基石。
---

# 文件系统 (RamFS)

SudoOS 实现了一个简单的内存文件系统 (In-Memory Filesystem)，称为 **RamFS**。它不是基于磁盘的持久化存储，而是利用内核堆内存动态模拟文件和目录结构。

**源文件**: `src/fs/ramfs.c`

## 1. 核心架构设计

### 存储结构
RamFS 摒弃了复杂的块设备驱动层，直接使用数组存储文件节点（Inode 模拟）。

- **节点存储**: `static ramfs_node_t files[MAX_FILES];` (最大支持 64 个文件/目录)
- **全局打开文件表**: `static file_t global_file_table[MAX_SYSTEM_OPEN_FILES];` (系统级文件句柄池)

### 数据结构

**文件节点 (`ramfs_node_t`)**:
代表文件系统中的一个实体（文件或目录）。
```c
typedef struct {
    char name[32];          // 文件名
    int type;               // 类型：RAMFS_TYPE_DIR (目录) 或 RAMFS_TYPE_FILE (文件)
    int parent_idx;         // 父目录索引 (用于支持 .. 跳转)
    int inode_idx;          // 自身在 files 数组中的索引
    uint8_t* content;       // 文件内容指针 (指向 kmalloc 分配的内存)
    uint64_t size;          // 文件大小
    bool is_used;           // 分配标记
} ramfs_node_t;
```

**文件句柄 (`file_t`)**:
代表进程打开的一个文件实例，记录读写偏移量。
```c
typedef struct {
    ramfs_node_t* node;     // 指向底层文件节点
    uint64_t offset;        // 当前读写指针位置
    int ref_count;          // 引用计数
} file_t;
```

## 2. 路径解析与目录结构

系统内置了路径解析器 `resolve_path`，支持绝对路径（`/usr/bin`）和相对路径（`../lib`），以及特殊符号 `.` 和 `..`。

### 初始化目录树
内核启动时，`ramfs_init` 会自动构建以下目录结构并将模拟文件写入内存：

```text
/
├── init             (Pid 1 用户进程镜像)
└── usr
    ├── bin          (存放用户可执行程序)
    |   └── user.elf (实际 Shell 程序)
    ├── lib          (库文件目录)
    ├── shell.c      (Shell 源代码预览)
    └── usrmain.c    (用户主程序源码预览)
```

## 3. 支持的操作 (VFS 接口)

RamFS 实现了类似 POSIX 的标准文件操作接口：

* **`open`**: 解析路径，分配文件句柄 (`fd`)。支持 `O_CREAT` 标志创建新文件。
* **`read / write`**: 基于内存的拷贝 (`memcpy`)。写入时若超过预分配大小会自动更新文件 size。
* **`getdents64`**: 读取目录项，返回 `linux_dirent64` 结构，供 `ls` 命令使用。
* **`mkdir`**: 创建新目录。
* **`chdir`**: 修改进程 PCB 中的 `cwd_inode`，实现工作目录切换。
* **`getcwd`**: 通过回溯 `parent_idx` 直到根目录，反向构建当前路径字符串。

---

# 用户空间环境 (User Land)

SudoOS 提供了一个基于 libc 的用户环境和一个交互式 Shell。

## 1. SudoOS Shell

位于 `/usr/bin/user.elf`，是系统启动后运行的第一个交互程序。

**源文件**: `usr/shell.c`

### 内置命令
Shell 支持以下常用命令：

| 命令 | 格式 | 描述 |
| :--- | :--- | :--- |
| **ls** | `ls [path]` | 列出目录内容，区分目录（带 `[]`）和文件。 |
| **cd** | `cd <path>` | 切换当前工作目录。 |
| **pwd** | `pwd` | 显示当前工作目录的全路径。 |
| **mkdir** | `mkdir <path>` | 创建新目录。 |
| **touch** | `touch <file>` | 创建空文件。 |
| **cat** | `cat <file>` | 打印文件内容到终端。 |
| **echo** | `echo <text> [> file]` | 输出文本，支持简单的重定向写入文件。 |
| **clear** | `clear` | 清屏。 |
| **run** | `run <syscall_id>` | **内核调试工具**：交互式调用任意系统调用。 |

### 调试工具：Run 命令
`run` 命令允许开发者绕过 libc 封装，直接通过汇编指令 `int 0x80` 触发系统调用，用于测试内核响应。
* **用法示例**: `run 0` (测试 SYS_READ) -> Shell 会提示输入 fd 和长度。

## 2. 系统调用表 (System Calls)

用户程序通过寄存器传递参数触发软中断。

**源文件**: `usr/lib/syscall.c`, `usr/lib/syscall.h`

* **中断号**: `0x80`
* **ABI**: `rax` (调用号), `rdi`, `rsi`, `rdx`, `r10`, `r8` (参数 1-5)。

| ID | 宏名称 (syscall.h) | 功能 |
| :--- | :--- | :--- |
| 0 | `SYS_READ` | 从文件描述符读取数据 |
| 1 | `SYS_WRITE` | 向文件描述符写入数据 |
| 2 | `SYS_OPEN` | 打开或创建文件 |
| 3 | `SYS_CLOSE` | 关闭文件描述符 |
| 39 | `SYS_GETPID` | 获取当前进程 ID |
| 57 | `SYS_FORK` | 创建子进程 (Copy-on-Write / Deep Copy) |
| 59 | `SYS_EXECVE` | 执行新程序 |
| 60 | `SYS_EXIT` | 终止进程 |
| 79 | `SYS_GETCWD` | 获取当前工作目录 |
| 80 | `SYS_CHDIR` | 改变工作目录 |
| 83 | `SYS_MKDIR` | 创建目录 |
| 217 | `SYS_GETDENTS64` | 获取目录项列表 |

## 3. ELF 加载器
**源文件**: `src/proc/proc.c` (配合 `src/lib/elf.h`)

内核具备解析 ELF64 格式可执行文件的能力：
1.  **验证**: 检查 ELF Magic Header 确认文件格式。
2.  **解析**: 遍历 Program Headers，寻找 `PT_LOAD` 类型的段。
3.  **加载**: 根据段信息（FileSiz, MemSiz）调用 `mm_map_range` 分配物理页，并映射到用户虚拟地址空间。
4.  **BSS 处理**: 如果 MemSiz > FileSiz，将多余部分（BSS）清零。
5.  **入口跳转**: 读取 ELF Header 中的 Entry Point，设置为新进程上下文的 `rip`。

---