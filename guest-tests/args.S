/* Prints "vexa-args: <argv[1]>\n" or "vexa-args: (none)\n". */
.global _start
_start:
    mov (%rsp), %rax
    cmp $2, %rax
    jl .Lnone
    mov 16(%rsp), %r8
    lea .Lprefix(%rip), %rsi
    mov $11, %rdx
    mov $1, %rax
    mov $1, %rdi
    syscall
    mov %r8, %rsi
    xor %rdx, %rdx
.Llen:
    cmpb $0, (%rsi,%rdx)
    je .Lwritearg
    inc %rdx
    jmp .Llen
.Lwritearg:
    mov %r8, %rsi
    mov $1, %rax
    mov $1, %rdi
    syscall
    lea .Lnl(%rip), %rsi
    mov $1, %rdx
    mov $1, %rax
    mov $1, %rdi
    syscall
    jmp .Lexit
.Lnone:
    lea .Lnonemsg(%rip), %rsi
    mov $18, %rdx
    mov $1, %rax
    mov $1, %rdi
    syscall
.Lexit:
    mov $60, %rax
    xor %rdi, %rdi
    syscall

.section .rodata
.Lprefix:
    .ascii "vexa-args: "
.Lnonemsg:
    .ascii "vexa-args: (none)\n"
.Lnl:
    .ascii "\n"
