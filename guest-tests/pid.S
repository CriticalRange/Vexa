/* Prints "vexa-pid: <getpid>\n". getpid is Linux syscall 39. */
.global _start
_start:
    mov $39, %rax
    syscall
    mov %rax, %r8
    lea .Lprefix(%rip), %rsi
    mov $10, %rdx
    mov $1, %rax
    mov $1, %rdi
    syscall
    lea .Lbuf+31(%rip), %rsi
    movb $10, (%rsi)
    dec %rsi
    mov %r8, %rax
.Ldigit:
    xor %rdx, %rdx
    mov $10, %rcx
    div %rcx
    add $'0', %dl
    mov %dl, (%rsi)
    dec %rsi
    test %rax, %rax
    jnz .Ldigit
    inc %rsi
    lea .Lbuf+32(%rip), %rdx
    sub %rsi, %rdx
    mov $1, %rax
    mov $1, %rdi
    syscall
    mov $60, %rax
    xor %rdi, %rdi
    syscall

.section .rodata
.Lprefix:
    .ascii "vexa-pid: "
.section .bss
.Lbuf:
    .skip 32
