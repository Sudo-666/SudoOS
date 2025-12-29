#pragma once
#include <stdint.h>


// ==========================================
// GDT Access Byte (访问权限字节) 宏定义
// ==========================================
// Bit 7: Present (存在位, 1=有效)
#define GDT_PRESENT         0x80

// Bit 5-6: DPL (Descriptor Privilege Level, 特权级)
#define GDT_DPL_KERNEL      0x00    // Ring 0
#define GDT_DPL_USER        0x60    // Ring 3

// Bit 4: Segment Type (段类型)
// 1 = 代码段/数据段 (Code/Data Segment)
// 0 = 系统段 (System Segment, 如 TSS)
#define GDT_SEGMENT         0x10
#define GDT_SYSTEM          0x00

// Bit 3: Executable (可执行位)
// 1 = 代码段 (Code), 0 = 数据段 (Data)
#define GDT_EXECUTABLE      0x08

// Bit 2: Direction/Conforming (方向/依从位)
// 对于数据段：0=向上生长, 1=向下生长
// 对于代码段：0=非依从, 1=依从
#define GDT_DC              0x04

// Bit 1: Read/Write (读写权限)
// 对于代码段：1=可读 (Readable)
// 对于数据段：1=可写 (Writable)
#define GDT_RW              0x02

// Bit 0: Accessed (已访问位 - CPU自动设置)
#define GDT_ACCESSED        0x01

// ==========================================
// GDT Flags (粒度/模式标志) 宏定义
// ==========================================
// Bit 7: Granularity (粒度)
// 1 = 4KB (Limit * 4096), 0 = 1B
#define GDT_GRAN_4K         0x80
#define GDT_GRAN_BYTE       0x00

// Bit 6: Size (操作数大小 - 仅对 16/32 位段有效)
// 1 = 32位, 0 = 16位 (64位代码段应设为0)
#define GDT_SIZE_32         0x40

// Bit 5: Long Mode (长模式 - 仅对代码段有效)
// 1 = 64位代码段
#define GDT_LONG_MODE       0x20

// 内核代码段: Present | Ring0 | Segment | Executable | Readable
#define GDT_KERNEL_CODE_ACCESS (GDT_PRESENT | GDT_DPL_KERNEL | GDT_SEGMENT | GDT_EXECUTABLE | GDT_RW)
// 内核数据段: Present | Ring0 | Segment | Writable
#define GDT_KERNEL_DATA_ACCESS (GDT_PRESENT | GDT_DPL_KERNEL | GDT_SEGMENT | GDT_RW)
// 用户代码段: Present | Ring3 | Segment | Executable | Readable
#define GDT_USER_CODE_ACCESS   (GDT_PRESENT | GDT_DPL_USER   | GDT_SEGMENT | GDT_EXECUTABLE | GDT_RW)
// 用户数据段: Present | Ring3 | Segment | Writable
#define GDT_USER_DATA_ACCESS   (GDT_PRESENT | GDT_DPL_USER   | GDT_SEGMENT | GDT_RW)
// TSS 描述符 (64位下 Type=9 表示 Available 64-bit TSS)
#define GDT_TSS_ACCESS         (GDT_PRESENT | GDT_DPL_KERNEL | GDT_SYSTEM | 0x09)

// 64位 GDT 条目
struct gdt_entry {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_middle;
    uint8_t  access;
    uint8_t  granularity;
    uint8_t  base_high;
} __attribute__((packed));

/**
 *  段描述符结构
 *    
        63          56 55    52 51    48 47          40 39          32
          +-------------+--------+--------+--------------+-------------+
    High  | Base [31:24]| Flags  | Limit  | Access Byte  | Base [23:16]|
    32-bit| (base_high) | (gran) | [19:16]|   (access)   | (base_mid)  |
          +-------------+--------+--------+--------------+-------------+
                ^          ^        ^           ^              ^
                |          |        |           |              |
                +----------+--------+---+-------+--------------+
                                        |
                                        | (拼接在一起组成了完整的段信息)
                                        |
        31                          16 15                          0
          +------------------------------+-----------------------------+
    Low   |         Base [15:0]          |         Limit [15:0]        |
    32-bit|         (base_low)           |         (limit_low)         |
          +------------------------------+-----------------------------+


        47    46  45  44    43   42   41   40
        +-----+-------+-----+----+----+----+----+
        |  P  |  DPL  |  S  |  E | DC | RW |  A |
        +-----+-------+-----+----+----+----+----+
            |     |       |     |    |    |    |
            |     |       |     |    |    |    +-- Accessed (已访问位，CPU自动置1)
            |     |       |     |    |    +------- Readable/Writable (读写权限)
            |     |       |     |    +------------ Direction/Conforming (方向/依从)
            |     |       |     +----------------- Executable (1=代码段, 0=数据段)
            |     |       +----------------------- Descriptor Type (1=代码/数据, 0=系统段/TSS)
            |     +------------------------------- DPL (0=内核 Ring0, 3=用户 Ring3)
            +------------------------------------- Present (存在位, 必须为1)

        55   54   53   52   51   50   49   48
        +----+----+----+----+----+----+----+----+
        | G  | D/B| L  | AVL|    Limit [19:16]  |
        +----+----+----+----+----+----+----+----+
            |    |    |    |        |
            |    |    |    |        +--- 段限长的高4位 (与低16位拼接成20位)
            |    |    |    +------------ Available (操作系统自定义使用)
            |    |    +----------------- Long Mode (1=64位代码段, 0=兼容模式)
            |    +---------------------- Default Operation Size (0=16位, 1=32位)
            +--------------------------- Granularity (粒度: 0=1B单位, 1=4KB单位)
 * 
 */

// 64位 TSS 结构体 (Task State Segment)
struct tss_entry {
    uint32_t reserved0;
    uint64_t rsp0;       // 中断时切换到这个栈
    uint64_t rsp1;
    uint64_t rsp2;
    uint64_t reserved1;
    uint64_t ist[7];     // 中断栈表
    uint64_t reserved2;
    uint16_t reserved3;
    uint16_t iomap_base;
} __attribute__((packed));

// 加载到 GDTR 寄存器的指针
struct gdt_ptr {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

void gdt_init();
void set_tss_stack(uint64_t kernel_stack);