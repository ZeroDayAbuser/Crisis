.code
; restore_call_stack proc
;     mov  rsp, rcx
;     mov  rcx, r8
;     jmp  rdx
; restore_call_stack endp

restore_call_stack proc
    mov  rsp, rcx
    jmp  rdx
restore_call_stack endp
end