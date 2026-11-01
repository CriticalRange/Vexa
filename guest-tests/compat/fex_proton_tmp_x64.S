/* Verify guest /tmp mutations are redirected to the compatibility rootfs. */
.global _start
.text
_start:
  mov $83, %eax      /* mkdir */
  lea dir(%rip), %rdi
  mov $448, %esi
  syscall
  test %rax, %rax
  jne fail
  mov $80, %eax      /* chdir */
  lea dir(%rip), %rdi
  syscall
  test %rax, %rax
  jne fail
  mov $258, %eax     /* mkdirat(AT_FDCWD, absolute, 0700) */
  mov $-100, %edi
  lea subdir(%rip), %rsi
  mov $448, %edx
  syscall
  test %rax, %rax
  jne fail
  mov $1, %eax
  mov $1, %edi
  lea ok(%rip), %rsi
  mov $(ok_end-ok), %edx
  syscall
  xor %edi, %edi
  jmp exit
fail:
  mov $94, %edi
exit:
  mov $60, %eax
  syscall
.section .rodata
dir: .asciz "/tmp/vexa2610-tmp-test"
subdir: .asciz "/tmp/vexa2610-tmp-test/sub"
ok: .ascii "PASS Proton /tmp redirection\n"
ok_end:
