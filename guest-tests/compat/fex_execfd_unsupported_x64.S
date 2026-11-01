/* The VEXA loader has no FEX_EXECVEFD handoff yet; fail explicitly. */
.global _start
.text
_start:
  mov $257, %eax
  mov $-100, %edi
  lea target(%rip), %rsi
  xor %edx, %edx
  xor %r10d, %r10d
  syscall
  test %rax, %rax
  js fail
  mov %eax, %edi
  mov $322, %eax
  lea empty(%rip), %rsi
  lea args(%rip), %rdx
  lea env(%rip), %r10
  mov $0x1000, %r8d
  syscall
  cmp $-95, %rax
  jne fail
  mov $1, %eax
  mov $1, %edi
  lea ok(%rip), %rsi
  mov $(ok_end-ok), %edx
  syscall
  xor %edi, %edi
  jmp exit
fail:
  mov $95, %edi
exit:
  mov $60, %eax
  syscall
.section .rodata
target: .asciz "/data/local/tmp/vexa2610-rpmalloc/vexa-hello"
empty: .asciz ""
ok: .ascii "PASS unsupported execveat FD returns ENOTSUP\n"
ok_end:
.section .data
args: .quad target, 0
env: .quad 0
