.code

exception_callout proc
    push    rbx

    ; Slot sits at rsp+20h and holds 38h bytes of data_request_t, leaving 28h
    ; spare below the saved rbx. The kernel handler writes the response to the
    ; instruction's operand - this slot - using ITS OWN sizeof(data_request_t).
    ; With only 10h of headroom a field added on one side alone would land on the
    ; saved rbx and the return address, and the caller would fail its /GS check
    ; with 0xC0000409 long after the write. The band is what absorbs that.
    sub     rsp, 0A0h
    mov     rbx, rcx

    lea     rdi, [rsp+20h]
    mov     rsi, rcx
    mov     ecx, 38h
    rep     movsb

    lgdt    fword ptr [rsp+20h]

    lea     rsi, [rsp+20h]
    mov     rdi, rbx
    mov     ecx, 38h
    rep     movsb

    add     rsp, 0A0h
    pop     rbx
    ret
exception_callout endp

end
