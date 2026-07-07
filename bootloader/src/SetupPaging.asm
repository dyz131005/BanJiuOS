.code
    PUBLIC LoadCr3

LoadCr3 PROC
    mov cr3, rcx
    ret
LoadCr3 ENDP

END