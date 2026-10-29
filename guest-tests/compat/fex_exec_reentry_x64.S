/* Freestanding guest probe: pathname exec must re-enter VEXA, without an FD handoff. */
.global _start
.text
_start:
  lea target(%rip), %rdi
  lea child_argv(%rip), %rsi
  lea child_env(%rip), %rdx
  mov $59, %eax
  syscall
  mov $1, %eax
  mov $2, %edi
  lea failure(%rip), %rsi
  mov $12, %edx
  syscall
  mov $60, %eax
  mov $91, %edi
  syscall
.section .rodata
target: .asciz "/data/local/tmp/vexa2610-rpmalloc/vexa-hello"
failure: .ascii "exec failed\n"
.section .data
child_argv: .quad target, 0
child_env: .quad 0
