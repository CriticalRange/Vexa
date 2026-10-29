.global _start
.text
_start:
  mov $59, %eax
  lea target(%rip), %rdi
  lea args(%rip), %rsi
  lea env(%rip), %rdx
  syscall
  mov $97, %edi
  mov $60, %eax
  syscall
.section .rodata
target: .asciz "/data/local/tmp/vexa2610-rpmalloc/probe.exe"
arg1: .asciz "arg-one"
.section .data
args: .quad target, arg1, 0
env: .quad 0
