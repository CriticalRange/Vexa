/* Guest x86_64 libVDSO-guest.so for FEX, hand-written so it builds without
   thunkgen or an x86_64 sysroot. Each entry packs its arguments into the
   packed ArgsRV_t layout expected by FEX's VDSO_Emulation.cpp handlers, then
   executes the FEX thunk opcode (0F 3F + sha256("libVDSO:<name>")) with RDI
   pointing at the pack. Without this file glibc falls back to real syscalls
   for every clock_gettime/gettimeofday, which is very expensive under FEX. */

    .text

#define THUNK(name, ...) \
    .p2align 4; \
    .type thunk_##name, @function; \
thunk_##name: \
    .byte 0x0f, 0x3f; \
    .byte __VA_ARGS__

/* sha256 hashes copied from FEX VDSO_Emulation.cpp VDSODefinitions. */
THUNK(time, 0x37,0x63,0x46,0xb0,0x79,0x06,0x5f,0x9d,0x00,0xb6,0x8d,0xfd,0x9e,0x4a,0x62,0xcd,0x1e,0x6c,0xcc,0x22,0xcd,0xb2,0xc0,0x17,0x7d,0x42,0x6a,0x40,0xd1,0xeb,0xfa,0xe0)
THUNK(gettimeofday, 0x77,0x2a,0xde,0x1c,0x13,0x2d,0xe9,0x48,0xaf,0xe0,0xba,0xcc,0x6a,0x89,0xff,0xca,0x4a,0xdc,0xd5,0x63,0x2c,0xc5,0x62,0x8b,0x5d,0xde,0x0b,0x15,0x35,0xc6,0xc7,0x14)
THUNK(clock_gettime, 0x3c,0x96,0x9b,0x2d,0xc3,0xad,0x2b,0x3b,0x9c,0x4e,0x4d,0xca,0x1c,0xe8,0x18,0x4a,0x12,0x8a,0xe4,0xc1,0x56,0x92,0x73,0xce,0x65,0x85,0x5f,0x65,0x7e,0x94,0x26,0xbe)
THUNK(clock_getres, 0xe4,0xa1,0xf6,0x23,0x35,0xae,0xb7,0xb6,0xb0,0x37,0xc5,0xc3,0xa3,0xfd,0xbf,0xa2,0xa1,0xc8,0x95,0x78,0xe5,0x76,0x86,0xdb,0x3e,0x6c,0x54,0xd5,0x02,0x60,0xd8,0x6d)
THUNK(getcpu, 0x39,0x83,0x39,0x36,0x0f,0x68,0xd6,0xfc,0xc2,0x3a,0x97,0x11,0x85,0x09,0xc7,0x25,0xbb,0x50,0x49,0x55,0x6b,0x0c,0x9f,0x50,0x37,0xf5,0x9d,0xb0,0x38,0x58,0x57,0x12)

/* time(time_t*) -> { ptr a_0 @0; u64 rv @8 } */
    .p2align 4
    .globl __vdso_time
    .type __vdso_time, @function
__vdso_time:
    subq $24, %rsp
    movq %rdi, 0(%rsp)
    movq $0, 8(%rsp)
    movq %rsp, %rdi
    call thunk_time
    movq 8(%rsp), %rax
    addq $24, %rsp
    ret
    .size __vdso_time, .-__vdso_time

/* gettimeofday(tv, tz) -> { ptr tv @0; ptr tz @8; int rv @16 } */
    .p2align 4
    .globl __vdso_gettimeofday
    .type __vdso_gettimeofday, @function
__vdso_gettimeofday:
    subq $24, %rsp
    movq %rdi, 0(%rsp)
    movq %rsi, 8(%rsp)
    movl $0, 16(%rsp)
    movq %rsp, %rdi
    call thunk_gettimeofday
    movslq 16(%rsp), %rax
    addq $24, %rsp
    ret
    .size __vdso_gettimeofday, .-__vdso_gettimeofday

/* clock_gettime(clk, tp) -> { i32 clk @0; ptr tp @4; int rv @12 } */
    .p2align 4
    .globl __vdso_clock_gettime
    .type __vdso_clock_gettime, @function
__vdso_clock_gettime:
    subq $24, %rsp
    movl %edi, 0(%rsp)
    movq %rsi, 4(%rsp)
    movl $0, 12(%rsp)
    movq %rsp, %rdi
    call thunk_clock_gettime
    movslq 12(%rsp), %rax
    addq $24, %rsp
    ret
    .size __vdso_clock_gettime, .-__vdso_clock_gettime

/* clock_getres(clk, tp) -> same layout as clock_gettime */
    .p2align 4
    .globl __vdso_clock_getres
    .type __vdso_clock_getres, @function
__vdso_clock_getres:
    subq $24, %rsp
    movl %edi, 0(%rsp)
    movq %rsi, 4(%rsp)
    movl $0, 12(%rsp)
    movq %rsp, %rdi
    call thunk_clock_getres
    movslq 12(%rsp), %rax
    addq $24, %rsp
    ret
    .size __vdso_clock_getres, .-__vdso_clock_getres

/* getcpu(cpu, node) -> { ptr cpu @0; ptr node @8; int rv @16 } */
    .p2align 4
    .globl __vdso_getcpu
    .type __vdso_getcpu, @function
__vdso_getcpu:
    subq $24, %rsp
    movq %rdi, 0(%rsp)
    movq %rsi, 8(%rsp)
    movl $0, 16(%rsp)
    movq %rsp, %rdi
    call thunk_getcpu
    movslq 16(%rsp), %rax
    addq $24, %rsp
    ret
    .size __vdso_getcpu, .-__vdso_getcpu

/* CALLBACKRET FEX instruction, located by FEX via the guest VDSO symbols. */
    .p2align 4
    .globl __fex_callback_ret
    .type __fex_callback_ret, @function
__fex_callback_ret:
    .byte 0x0f, 0x3e
    .size __fex_callback_ret, .-__fex_callback_ret

    .weak time
    .set time, __vdso_time
    .weak gettimeofday
    .set gettimeofday, __vdso_gettimeofday
    .weak clock_gettime
    .set clock_gettime, __vdso_clock_gettime
    .weak clock_getres
    .set clock_getres, __vdso_clock_getres
    .weak getcpu
    .set getcpu, __vdso_getcpu

    .section .note.GNU-stack,"",@progbits
