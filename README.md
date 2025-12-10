# 进度
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
# 运行

```bash
make run
```

