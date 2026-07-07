; DisablePaging.asm
; Disables paging by clearing CR0.PG bit
; Using MASM64 syntax (ML64)

.code
    PUBLIC DisablePaging

DisablePaging PROC
    mov rax, cr0
    and rax, 7FFFFFFFh  ; Clear PG bit (bit 31)
    mov cr0, rax
    jmp short Done
Done:
    ret
DisablePaging ENDP

END
