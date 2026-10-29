/* Linux x86-64 guest checks Android seccomp-safe fallback and accept routing. */
.global _start
.text
.macro expect_enosys number
  mov $\number, %eax
  xor %edi, %edi
  xor %esi, %esi
  xor %edx, %edx
  xor %r10d, %r10d
  xor %r8d, %r8d
  syscall
  cmp $-38, %rax
  jne fail
.endm
_start:
  expect_enosys 29  /* shmget */
  expect_enosys 30  /* shmat */
  expect_enosys 31  /* shmctl */
  expect_enosys 64  /* semget */
  expect_enosys 68  /* msgget */
  expect_enosys 449 /* futex_waitv, compatibility session */
  mov $43, %eax     /* accept(-1, NULL, NULL) -> accept4 */
  mov $-1, %edi
  xor %esi, %esi
  xor %edx, %edx
  syscall
  cmp $-9, %rax
  jne fail
  mov $1, %eax
  mov $1, %edi
  lea ok(%rip), %rsi
  mov $(ok_end-ok), %edx
  syscall
  xor %edi, %edi
  jmp exit
fail:
  mov $1, %eax
  mov $2, %edi
  lea bad(%rip), %rsi
  mov $(bad_end-bad), %edx
  syscall
  mov $92, %edi
exit:
  mov $60, %eax
  syscall
.section .rodata
ok: .ascii "PASS Android syscall guest fallbacks\n"
ok_end:
bad: .ascii "FAIL guest syscall\n"
bad_end:
