option casemap:none

.code

nexus_invoke_registers proc frame
	push rbx
	.pushreg rbx
	push rbp
	.pushreg rbp
	push rsi
	.pushreg rsi
	push rdi
	.pushreg rdi
	push r12
	.pushreg r12
	push r13
	.pushreg r13
	push r14
	.pushreg r14
	push r15
	.pushreg r15
	sub rsp, 0b8h
	.allocstack 0b8h
	.endprolog

	mov qword ptr [ rsp + 20h ], rcx
	mov qword ptr [ rsp + 28h ], rdx
	mov r15, rcx

	mov rax, qword ptr [ r15 + 00h ]
	mov rbx, qword ptr [ r15 + 08h ]
	mov rcx, qword ptr [ r15 + 10h ]
	mov rdx, qword ptr [ r15 + 18h ]
	mov rsi, qword ptr [ r15 + 20h ]
	mov rdi, qword ptr [ r15 + 28h ]
	mov rbp, qword ptr [ r15 + 30h ]
	mov r8, qword ptr [ r15 + 38h ]
	mov r9, qword ptr [ r15 + 40h ]
	mov r10, qword ptr [ r15 + 48h ]
	mov r11, qword ptr [ r15 + 50h ]
	mov r12, qword ptr [ r15 + 58h ]
	mov r13, qword ptr [ r15 + 60h ]
	mov r14, qword ptr [ r15 + 68h ]

	call qword ptr [ rsp + 28h ]

	mov qword ptr [ rsp + 30h ], rax
	mov qword ptr [ rsp + 38h ], rbx
	mov qword ptr [ rsp + 40h ], rcx
	mov qword ptr [ rsp + 48h ], rdx
	mov qword ptr [ rsp + 50h ], rsi
	mov qword ptr [ rsp + 58h ], rdi
	mov qword ptr [ rsp + 60h ], rbp
	mov qword ptr [ rsp + 68h ], r8
	mov qword ptr [ rsp + 70h ], r9
	mov qword ptr [ rsp + 78h ], r10
	mov qword ptr [ rsp + 80h ], r11
	mov qword ptr [ rsp + 88h ], r12
	mov qword ptr [ rsp + 90h ], r13
	mov qword ptr [ rsp + 98h ], r14
	movdqu xmmword ptr [ rsp + 0a0h ], xmm0

	mov r15, qword ptr [ rsp + 20h ]
	mov rax, qword ptr [ rsp + 30h ]
	mov qword ptr [ r15 + 00h ], rax
	mov rax, qword ptr [ rsp + 38h ]
	mov qword ptr [ r15 + 08h ], rax
	mov rax, qword ptr [ rsp + 40h ]
	mov qword ptr [ r15 + 10h ], rax
	mov rax, qword ptr [ rsp + 48h ]
	mov qword ptr [ r15 + 18h ], rax
	mov rax, qword ptr [ rsp + 50h ]
	mov qword ptr [ r15 + 20h ], rax
	mov rax, qword ptr [ rsp + 58h ]
	mov qword ptr [ r15 + 28h ], rax
	mov rax, qword ptr [ rsp + 60h ]
	mov qword ptr [ r15 + 30h ], rax
	mov rax, qword ptr [ rsp + 68h ]
	mov qword ptr [ r15 + 38h ], rax
	mov rax, qword ptr [ rsp + 70h ]
	mov qword ptr [ r15 + 40h ], rax
	mov rax, qword ptr [ rsp + 78h ]
	mov qword ptr [ r15 + 48h ], rax
	mov rax, qword ptr [ rsp + 80h ]
	mov qword ptr [ r15 + 50h ], rax
	mov rax, qword ptr [ rsp + 88h ]
	mov qword ptr [ r15 + 58h ], rax
	mov rax, qword ptr [ rsp + 90h ]
	mov qword ptr [ r15 + 60h ], rax
	mov rax, qword ptr [ rsp + 98h ]
	mov qword ptr [ r15 + 68h ], rax
	movdqu xmm0, xmmword ptr [ rsp + 0a0h ]
	movdqu xmmword ptr [ r15 + 70h ], xmm0

	add rsp, 0b8h
	pop r15
	pop r14
	pop r13
	pop r12
	pop rdi
	pop rsi
	pop rbp
	pop rbx
	ret
nexus_invoke_registers endp

end
