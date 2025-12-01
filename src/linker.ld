ENTRY(_start)
SECTIONS {
    . = 0xffffffff80000000;
    .text : { *(.text*) }
    . += 0x1000;
    .rodata : { *(.rodata*) }
    . += 0x1000;
    .data : { *(.data*) }
    . += 0x1000;
    .bss : { *(COMMON) *(.bss*) }
}