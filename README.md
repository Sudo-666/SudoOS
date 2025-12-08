# 进度
用 C 编写的一个符合 Limine 标准的小型 x86-64 内核，并使用 Limine 引导加载程序启动

- Limine 本质只是引导程序（bootloader），它的工作是：
    - 加载你的内核 ELF 文件（bin/sudoOS）
    - 切换到你指定的 CPU 状态（x86_64 long mode）
    - 为你准备好一些必要的信息（内存地图、帧缓冲、SMBIOS...）
    - 跳转到你的内核入口函数 kmain()

- 内核地址空间：0xffffffff80000000 ~ 0xffffffffffffffff


# 内存管理系统

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

## Bitmap

- Bitmap每一项为`uint8_t`类型，对应8个物理页框，共`8*4kB` 大小。
- 对Bitmap的bit位进行操作：
```c
static inline void bit_set(size_t bit);

static inline void bit_unset(size_t bit);

static inline bool bit_test(size_t bit);
```







---
# 运行

```bash
make run
```

