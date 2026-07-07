ENTRY(_start)

MEMORY {
    KERNEL (rw) : ORIGIN = 0xFFFFFFFF80000000, LENGTH = 0x10000000
}

SECTIONS {
    . = 0xFFFFFFFF80000000;

    .text : {
        *(.text.startup)
        *(.text)
        *(.text.*)
    } > KERNEL

    .rodata : {
        *(.rodata)
        *(.rodata.*)
    } > KERNEL

    .data : {
        *(.data)
        *(.data.*)
    } > KERNEL

    .bss : {
        *(.bss)
        *(.bss.*)
        *(COMMON)
    } > KERNEL

    /DISCARD/ : {
        *(.eh_frame)
        *(.note.GNU-stack)
        *(.pdata)
        *(.pdata.*)
        *(.pdata.startup)
        *(.xdata)
        *(.xdata.*)
        *(.xdata.startup)
    }
}
