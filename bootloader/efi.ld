ENTRY(efi_main)

SECTIONS {
    . = 0x0000000000100000;
    .text : {
        *(.text)
        *(.text.*)
    }
    .data : {
        *(.data)
        *(.data.*)
    }
    .bss : {
        *(.bss)
        *(.bss.*)
        *(COMMON)
    }
    /DISCARD/ : {
        *(.eh_frame)
        *(.note.GNU-stack)
        *(.pdata)
        *(.xdata)
    }
}