#include "gdt.h"

struct gdt_entry gdt[7]; // 5个基础段 + TSS需要占2个位置(因为TSS在64位下很大)
struct gdt_ptr gdtp;
struct tss_entry tss;

// 汇编里的刷新函数
extern void gdt_flush(uint64_t gdt_ptr_addr);
extern void tss_load();

// 填充函数：设置 GDT 条目
void gdt_set_gate(int num, uint64_t base, uint64_t limit, uint8_t access, uint8_t gran) {
    gdt[num].base_low    = (base & 0xFFFF);
    gdt[num].base_middle = (base >> 16) & 0xFF;
    gdt[num].base_high   = (base >> 24) & 0xFF;

    gdt[num].limit_low   = (limit & 0xFFFF);
    gdt[num].granularity = (limit >> 16) & 0x0F;
    gdt[num].granularity |= gran & 0xF0;
    gdt[num].access      = access;
}

void gdt_init() {
    // 0. 初始化 TSS，全部清零
    // 实际运行时，每次进程切换，我们都要修改 tss.rsp0
    __builtin_memset(&tss, 0, sizeof(tss));
    tss.rsp0 = 0; // 暂时写0，稍后设置

    // 1. 设置 GDT 指针
    gdtp.limit = (sizeof(struct gdt_entry) * 7) - 1;
    gdtp.base  = (uint64_t)&gdt;

    // 2. 填充 GDT 条目
    // 0: Null
    gdt_set_gate(0, 0, 0, 0, 0);

    // 1: Kernel Code (0x08)
    // Access: 0x9A = 1(P) 00(Ring0) 1(S) 1010(Code/Read)
    // Gran:   0xAF = 1(G) 0(D/B) 1(L-64bit) 1111(Limit)
    gdt_set_gate(1, 0, 0, GDT_KERNEL_CODE_ACCESS, GDT_LONG_MODE); // 64位代码段不检查limit，设flags即可

    // 2: Kernel Data (0x10)
    // Access: 0x92 = 1(P) 00(Ring0) 1(S) 0010(Data/Write)
    gdt_set_gate(2, 0, 0, GDT_KERNEL_DATA_ACCESS, 0x00);

    // 3: User Data (0x18)
    // Access: 0xF2 = 1(P) 11(Ring3) 1(S) 0010(Data/Write)
    gdt_set_gate(3, 0, 0, GDT_USER_DATA_ACCESS, 0x00);

    // 4: User Code (0x20)
    // Access: 0xFA = 1(P) 11(Ring3) 1(S) 1010(Code/Read)
    gdt_set_gate(4, 0, 0, GDT_USER_CODE_ACCESS, GDT_LONG_MODE);

    // 5: TSS (0x28) - 这是一个特殊的系统段
    // 在 64 位下，TSS 描述符是 16 字节长，占用 gdt[5] 和 gdt[6]
    uint64_t tss_base = (uint64_t)&tss;
    uint64_t tss_limit = sizeof(tss);

    gdt_set_gate(5, tss_base, tss_limit, GDT_TSS_ACCESS, 0x00); // 0x89 = Present, Ring0, TSS Available
    
    // TSS 描述符的高 64 位特殊处理 (放在 gdt[6] 的位置)
    // 只是简单地把地址的高 32 位放进去
    struct gdt_entry *tss_high = &gdt[6];
    __builtin_memset(tss_high, 0, sizeof(struct gdt_entry));
    tss_high->limit_low = (tss_base >> 32) & 0xFFFF;
    tss_high->base_low  = (tss_base >> 48) & 0xFFFF;

    // 3. 告诉 CPU 加载新表
    gdt_flush((uint64_t)&gdtp);
    tss_load();
}

// 这是一个极其重要的函数：每次进程切换时都要调用
// 告诉 CPU："如果这之后发生了中断，请把栈切换到 kernel_stack 这个地址"
void set_tss_stack(uint64_t kernel_stack) {
    tss.rsp0 = kernel_stack;
}