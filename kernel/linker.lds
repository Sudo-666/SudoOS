/* 设置输出文件格式 */
OUTPUT_FORMAT(elf64-x86-64)

/* 设置入口函数 */
ENTRY(kmain)

/*

PHDRS 定义 ELF 的 Program Headers，也就是“加载段”。
Limine（和 Linux 内核）都是根据 Program Header 来把 ELF 加载到内存的。

这里我们定义了 4 个加载段：
    limine_requests —— 包含 Limine Request 结构体
    text —— 内核代码段
    rodata —— 只读数据（字符串常量等）
    data —— 读写数据与 BSS

PT_LOAD: Limine 启动内核时必须把它加载到内存中。

*/

PHDRS
{
    limine_requests PT_LOAD;
    text PT_LOAD;
    rodata PT_LOAD;
    data PT_LOAD;
}

/* 段布局 */
SECTIONS
{
    /*
    设置入口地址：从虚拟地址 0xffffffff80000000 开始放内核
    在高半区（Higher Half）设置内核区域
    */
    . = 0xffffffff80000000;

    /*
        .limine_requests 段 ：内核通过它向 Limine 传递请求
        KEEP() 让 ld 不要优化掉这三个节
     
    */
    .limine_requests : {
        KEEP(*(.limine_requests_start))
        KEEP(*(.limine_requests))
        KEEP(*(.limine_requests_end))
    } :limine_requests

    /* Move to the next memory page for .text */
    . = ALIGN(CONSTANT(MAXPAGESIZE));

    /* .text 段（代码段） */
    .text : {
        *(.text .text.*)
    } :text

    /* Move to the next memory page for .rodata */
    . = ALIGN(CONSTANT(MAXPAGESIZE));

    /* .rodata（只读数据） */
    .rodata : {
        *(.rodata .rodata.*)
    } :rodata

    /* Add a .note.gnu.build-id output section in case a build ID flag is added to the */
    /* linker command. */
    .note.gnu.build-id : {
        *(.note.gnu.build-id)
    } :rodata

    /* .data（可写数据） */
    . = ALIGN(CONSTANT(MAXPAGESIZE));

    .data : {
        *(.data .data.*)
    } :data

    /* .bss（未初始化数据）
        .bss 必须是 data Program Header 的最后部分
     */
    .bss : {
        *(.bss .bss.*)
        *(COMMON)
    } :data

    /* 
    丢弃调试信息（避免 QEMU/UEFI 出错）
        丢弃 .eh_frame（异常堆栈帧信息）
        丢弃 .note（ELF 的说明字段） 
    */
    /DISCARD/ : {
        *(.eh_frame*)
        *(.note .note.*)
    }
}

