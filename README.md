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
                    | (可能包含 1:1 映射)  |
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
- **存放内容：** 整个物理内存的副本。在您的设计中，**Bitmap 数组** 应该通过该区域访问。
- **作用：** 内核可以通过 `虚拟地址 = 物理地址 + HHDM_OFFSET` 的简单公式直接操作物理内存，无需反复建立新页表。这对 PMM 和页表项（PTE/PDE）本身的修改至关重要。

#### **B. 内核堆区 (Kernel Heap)**

- **起始地址：** 建议设为 `0xFFFF900000000000`（可根据需要调整）。
- **存放内容：** 存放内核运行期间动态申请的对象，如进程控制块 (PCB)、缓冲区、动态分配的数据结构。
- **作用：** 这是您新实现的 **`kmalloc`** 管理的主战场。通过双向循环链表，在此区域内进行细粒度的字节分配。当空间不足时，调用 `pmm_alloc_page` 申请页框并由 `vmm_map_page` 映射到此区域。

#### **C. MMIO / Vmalloc 映射区**

- **起始地址：** 建议设为 `0xFFFFA00000000000` 或 `0xFFFFB00000000000`。
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

这是一份基于您提供的源代码（`mm/pmm.c`, `mm/pmm.h` 等）编写的内核堆管理器技术文档。

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
- 虽然指针是 64 位的，但标准的 4 级分页只使用了低 48 位（这就是为什么说是 256TB 地址空间）。高 16 位必须进行符号扩展（Canonical Form，通常全是0或全是1）。

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

系统支持以下核心特性：

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

---

这是一份关于 **用户地址空间管理 (User Address Space Management)** 的详细设计与实现指导文档。基于你现有的 `src/mm/vmm.h` 和 `src/mm/paging.c`，我们需要完善 `mm_struct` 的管理逻辑，使得每个进程拥有独立的页表（PML4），并能正确管理虚拟内存区域（VMA）。

---

# 用户地址空间管理 (VMM) 

## 1. 模块概述

本模块负责管理用户进程的虚拟内存。它的核心任务是：

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

我们需要在 `src/mm/vmm.h` 或 `src/proc/proc.h` 中补充以下函数原型。

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

---

## 4. 实现步骤详解

### 4.1 实现 `mm_create`

这是最关键的一步。新进程的页表不能是空的，必须**拷贝内核映射**。

**原理**：
x86_64 虚拟地址高半部分（`0xFFFF800000000000` 以上）是内核空间。这部分映射在 PML4 表中对应的是第 256 到 511 项 (索引)。
因为所有进程共享同一个内核，我们只需要把 `kernel_pml4` 的第 256~511 项的内容拷贝到新进程的 PML4 中即可。

**实现逻辑**：

1. 分配一个新的 PML4 页 (物理页)
2. 获取其虚拟地址 (通过 HHDM) 以便写入
3. 清空用户空间部分 (0~255)
4. 拷贝内核空间映射 (256~511)


### 4.2 实现 `mm_map_range`

这个函数给 ELF Loader 使用。它需要完成“VMA 记录”和“立即映射”两件事（对于简单的 OS，我们暂时不实现请求调页 Demand Paging，而是直接映射）。

**实现逻辑**：

```c
bool mm_map_range(struct mm_struct *mm, uint64_t va_start, size_t size, uint64_t vm_flags) {
    // 对齐地址
    uint64_t start = ALIGN_DOWN(va_start, PAGE_SIZE);
    uint64_t end   = ALIGN_UP(va_start + size, PAGE_SIZE);
    uint64_t pages = (end - start) / PAGE_SIZE;

    // 1. 创建 VMA 结构体并加入链表
    struct vma_struct *vma = kmalloc(sizeof(struct vma_struct));
    vma->mm = mm;
    vma->vm_start = start;
    vma->vm_end = end;
    vma->vm_flags = vm_flags;
    list_add_before(&vma->list_node, &mm->vma_list); // 简单插入，最好按地址排序插入
    mm->map_count++;

    // 2. 转换 VMA flag 到 页表 flag
    uint64_t pte_flags = PTE_PRESENT;
    if (vm_flags & VM_WRITE) pte_flags |= PTE_RW;
    if (vm_flags & VM_USER)  pte_flags |= PTE_USER; // 用户态必须加这个
    if (vm_flags & VM_EXEC)  { /* x86 如果开启 NX 位这里需要处理，否则默认可执行 */ }

    // 3. 立即分配物理内存并映射
    for (uint64_t i = 0; i < pages; i++) {
        uint64_t pa = pmm_alloc_page();
        if (pa == 0) return false; // TODO: 这里应该回滚释放

        // 调用你已有的 vmm_map_page，但要注意它操作的是 mm->pml4
        vmm_map_page(mm->pml4, start + i * PAGE_SIZE, pa, pte_flags);
        
        // 可选：如果是 BSS 段，可能需要清零物理页
        // memset((void*)(pa + HHDM_OFFSET), 0, PAGE_SIZE);
    }
    
    return true;
}

```

### 4.3 实现 `mm_destroy`

当进程退出时，需要释放其占用的资源：

 1. 释放 VMA 和用户物理内存
 2. 释放页表结构
 3. 释放 PML4 本身

---

# 在qemu上运行

```bash
make run
```

# 内核处理流程

## 1. 概述 (Overview)

## 1. 引导阶段 (Boot Phase)

内核由 **Limine** 引导加载程序启动。

- **配置文件**：`SudoOS/limine.conf` 定义了引导协议、内核路径和模块路径。
- 内核路径：`boot():/boot/kernel`
- 用户程序模块：`boot():/user.bin`

- **Limine 请求**：内核在 `SudoOS/kernel/src/main.c` 中声明了一系列请求结构体，Limine 在加载内核时会填充这些结构体，提供硬件信息。
- `framebuffer_request`: 获取显存信息用于显示。
- `memmap_request`: 获取物理内存布局。
- `hhdm_request`: 获取高半核直接映射 (HHDM) 偏移量。
- `module_request`: 获取加载的模块（即用户程序 `user.bin`）。

## 2. 内核初始化 (Kernel Initialization)

入口函数为 `kmain`，它首先调用 `kernel_init` 进行基础硬件和子系统初始化。

### 2.1 基础硬件与中断初始化

1. **GDT (全局描述符表)**: `gdt_init()` 初始化 GDT，设置内核态和用户态的代码段/数据段描述符，以及 TSS (任务状态段)。
2. **IDT (中断描述符表)**: `idt_init()` 初始化 IDT，配置异常处理、硬件中断（时钟、键盘）以及系统调用（0x80 中断）的入口。
3. **控制台**: `console_init()` 利用 Limine 提供的 framebuffer 初始化图形化控制台，支持 `kprintf` 输出。

### 2.2 内存管理初始化 (`mm_init`)

函数 `mm_init` 负责构建内核的内存环境：

1. **物理内存管理 (PMM)**: `pmm_init()` 解析 Limine 提供的内存图 (Memmap)，利用位图 (Bitmap) 管理物理页的分配与释放。
2. **分页机制 (Paging)**: `paging_init()` 建立页表。它分配新的 PML4 页表，映射内核本身、HHDM 区域（用于访问所有物理内存），并加载 CR3 寄存器激活新页表。
3. **内核堆 (Kernel Heap)**: `kheap_init()` 初始化内核堆，允许动态内存分配 (`kmalloc`)。
4. **内核栈 (Kernel Stack)**: `kstack_init()` 分配并映射新的内核栈。
5. **栈切换**: `set_tss_stack()` 将新分配的内核栈地址填入 TSS 的 `RSP0` 字段。这意味着当用户态发生中断或系统调用时，CPU 会自动切换到这个内核栈。



## 3.用户程序测试 (User Program Loading)

内存初始化完成后，`kmain` 开始准备运行用户程序。

### 3.1 获取模块

内核通过 `module_request` 检查 Limine 是否加载了用户程序模块。

```c
struct limine_module_response *module_response = module_request.response;
// 获取第一个模块 (user.bin)
struct limine_file *user_file = module_response->modules[0];

```

### 3.2 建立用户空间映射

内核为用户程序定义了固定的虚拟地址布局：

- **代码段基址**: `0x1000000` (16MB)
- **用户栈基址**: `0x80000000` (2GB)

加载过程如下：

1. **分配物理页**: 根据 `user.bin` 文件的大小，计算需要多少页，并调用 `pmm_alloc_page()` 分配相应的物理内存。
2. **映射页表**: 使用 `vmm_map_page()` 将用户虚拟地址 (`0x1000000`) 映射到分配的物理地址。关键在于设置了 `PTE_USER` 标志，允许 Ring 3 访问该内存。
3. **拷贝代码**: 利用 HHDM 机制，将 Limine 模块中的原始数据 (`user_file->address`) 拷贝到刚分配的物理内存中。
4. **准备用户栈**: 同样地，为用户栈分配物理页并映射到 `0x80000000`，同样赋予 `PTE_USER` 权限。

## 4. 切换至用户态 (Ring 3 Switch)

一切准备就绪后，内核调用 `enter_user_mode` 函数进行特权级切换。

### 4.1 构造中断返回帧 (Trap Frame)

`enter_user_mode` 是一个内联汇编函数，它模拟了中断返回的动作来“返回”到用户态。它按顺序压入以下寄存器值（x86_64 `iretq` 指令的要求）：

1. **SS (Stack Segment)**: 用户数据段选择子 (`0x1B` = User Data Selector `0x18` | RPL `3`)。
2. **RSP (Stack Pointer)**: 用户栈顶地址 (`user_stack_va + PAGE_SIZE`)。
3. **RFLAGS**: 标志寄存器，特别是开启中断标志位 (`IF` = 1, `0x200`)。
4. **CS (Code Segment)**: 用户代码段选择子 (`0x23` = User Code Selector `0x20` | RPL `3`)。
5. **RIP (Instruction Pointer)**: 用户程序入口地址 (`0x1000000`)。

### 4.2 执行切换

- **设置段寄存器**: 将 `DS`, `ES`, `FS`, `GS` 设置为用户数据段选择子 (`0x1B`)。
- **IRETQ**: 执行 `iretq` 指令。CPU 弹出栈上的上下文，跳转到 `RIP` (`0x1000000`)，并将特权级 (CPL) 从 0 切换到 3。

## 5. 用户程序执行与系统调用

### 5.1 用户代码执行

CPU 开始执行位于 `0x1000000` 的用户代码（即 `user.bin`）。
用户程序的入口是 `usrmain`（在链接脚本或 `ld` 命令中指定了 `-e usrmain`）。`usrmain` 调用 `print()` 函数输出字符串。

### 5.2 触发系统调用

`print()` 函数内部封装了系统调用逻辑：

```c
// usr/lib/syscall.c
__asm__ volatile (
    "int $0x80"
    : "=a" (ret)
    : "a" (num), ...
);

```

它执行 `int 0x80` 指令，触发 128 号中断。

### 5.3 内核处理系统调用

1. **进入内核**: CPU 捕获异常，根据 IDT 跳转到 `isr128` (在 `src/arch/isr.S` 中定义)。
2. **保存上下文**: `isr128` 将所有通用寄存器压栈，保存用户态现场。
3. **分发处理**: 调用 C 函数 `isr_handler`。
4. **执行服务**: `isr_handler` 识别出中断号 128，调用 `syscall_handler`。`syscall_handler` 根据 `RAX` 中的系统调用号（如 `SYS_WRITE`），执行打印操作（调用 `kprintf`）。
5. **返回用户态**: 处理完成后，代码执行 `iretq`，恢复之前保存的上下文，回到用户程序继续执行。

### 5.4 程序结束

用户程序 `usrmain` 最后进入死循环 `while(1)`，防止函数返回导致指令指针跑飞。

