#include "gdt.h"

struct gdt_entry gdt[7]; 
struct gdt_ptr gdtp;
struct tss_entry tss;

extern void gdt_flush(uint64_t gdt_ptr_addr);
extern void tss_load();

void gdt_set_gate(int num, uint64_t base, uint32_t limit, uint8_t access, uint8_t gran) {
    gdt[num].base_low    = (base & 0xFFFF);
    gdt[num].base_middle = (base >> 16) & 0xFF;
    gdt[num].base_high   = (base >> 24) & 0xFF;

    gdt[num].limit_low   = (limit & 0xFFFF);
    gdt[num].granularity = (limit >> 16) & 0x0F;
    gdt[num].granularity |= gran & 0xF0;
    gdt[num].access      = access;
}

void gdt_init() {
    // 0. 初始化 TSS
    __builtin_memset(&tss, 0, sizeof(tss));
    // 必须设置 iomap_base 等于 TSS 的大小，表示没有 I/O 许可位图
    tss.iomap_base = sizeof(struct tss_entry); 

    // 1. 设置 GDT 指针
    gdtp.limit = (sizeof(struct gdt_entry) * 7) - 1;
    gdtp.base  = (uint64_t)&gdt;

    // 2. 填充 GDT 条目 (符合 syscall/sysret 标准顺序)
    gdt_set_gate(0, 0, 0, 0, 0);                // Null
    gdt_set_gate(1, 0, 0, GDT_KERNEL_CODE_ACCESS, GDT_LONG_MODE); // 0x08
    gdt_set_gate(2, 0, 0, GDT_KERNEL_DATA_ACCESS, 0);             // 0x10
    
    // 注意：x86_64 sysret 要求用户代码段在用户数据段之后 (User Data + 8)
    gdt_set_gate(3, 0, 0, GDT_USER_DATA_ACCESS,   0);             // 0x18
    gdt_set_gate(4, 0, 0, GDT_USER_CODE_ACCESS,   GDT_LONG_MODE); // 0x20

    // 3. 填充 TSS 描述符 (占用 Index 5 和 6)
    uint64_t tss_base = (uint64_t)&tss;
    uint32_t tss_limit = sizeof(tss) - 1;

    // 低 8 字节 (Index 5)
    gdt_set_gate(5, tss_base, tss_limit, GDT_TSS_ACCESS, 0);
    
    // 高 8 字节 (Index 6) - 存放基地址的高 32 位
    // 我们强制转换成 uint32_t 指针来直接操作这 8 个字节
    uint32_t *tss_high = (uint32_t *)&gdt[6];
    tss_high[0] = (uint32_t)(tss_base >> 32); // 基地址 63:32
    tss_high[1] = 0;                          // 保留位

    // 4. 加载
    gdt_flush((uint64_t)&gdtp);
    tss_load();
}

void set_tss_stack(uint64_t kernel_stack) {
    tss.rsp0 = kernel_stack;
}