import struct, sys

# Builds two i386 ELF files:
#  - t32dyn: ET_EXEC dynamic executable with PT_INTERP=/lib32/ld-linux.so.2
#  - ld32.so: ET_EXEC fake dynamic loader at fixed base 0x40000000 that prints
#    a marker, scans the auxv on the guest stack for AT_ENTRY, and jumps to it.
# Verifies FEX's PT_INTERP loading + interp handoff without a real i386 libc.

def elf_header(base, entry, phoff, phnum):
    e = b"\x7fELF" + bytes([1, 1, 1]) + bytes(9)
    e += struct.pack("<HH", 2, 3)          # ET_EXEC, EM_386
    e += struct.pack("<I", 1)
    e += struct.pack("<I", entry)
    e += struct.pack("<I", phoff)
    e += struct.pack("<I", 0)
    e += struct.pack("<I", 0)
    e += struct.pack("<HHH", 52, 32, phnum)
    e += struct.pack("<HHH", 0, 0, 0)
    return e

def phdr(ptype, off, va, filesz, memsz, flags, align):
    return struct.pack("<IIIIIIII", ptype, off, va, va, filesz, memsz, flags, align)


def build_ld32(path):
    base = 0x40000000
    msg = b"FEX-LD32 interp ran\n"

    code = b""
    code += b"\xb8" + struct.pack("<I", 4)              # mov eax,4 (write)
    code += b"\xbb" + struct.pack("<I", 1)              # mov ebx,1
    code += b"\xb9" + struct.pack("<I", 0)              # mov ecx,msg (patched)
    code += b"\xba" + struct.pack("<I", len(msg))       # mov edx,len
    code += b"\xcd\x80"                                 # int 0x80
    code += b"\x89\xe6"                                 # mov esi,esp
    code += b"\x8b\x06"                                 # mov eax,[esi] -> argc
    code += b"\x8d\xb4\x86\x08\x00\x00\x00"             # lea esi,[esi+eax*4+8] -> envp[0]
    # envp scan for NULL
    scan_env = len(code)
    code += b"\xad"                                     # lodsd
    code += b"\x85\xc0"                                 # test eax,eax
    code += b"\x75\xfb"                                 # jnz scan_env
    # auxv scan for AT_ENTRY=9
    scan_aux = len(code)
    code += b"\xad"                                     # lodsd -> type
    code += b"\x89\xc3"                                 # mov ebx,eax
    code += b"\xad"                                     # lodsd -> val
    code += b"\x83\xfb\x09"                             # cmp ebx,9
    code += b"\x74\x02"                                 # je +2 -> jmp eax
    code += b"\xeb" + struct.pack("b", scan_aux - (len(code) + 2))  # jmp scan_aux
    code += b"\xff\xe0"                                 # jmp eax (AT_ENTRY)

    ehsize, phentsize, phnum = 52, 32, 1
    code_off = ehsize + phentsize * phnum
    msg_va = base + code_off + len(code)
    code = code[:11] + struct.pack("<I", msg_va) + code[15:]

    filesz = code_off + len(code) + len(msg)
    out = elf_header(base, base + code_off, ehsize, phnum)
    out += phdr(1, 0, base, filesz, filesz, 5, 0x1000)  # PT_LOAD R+X
    out += code + msg
    assert len(out) == filesz
    open(path, "wb").write(out)
    print(f"ld32: {len(out)} bytes, entry=0x{base + code_off:x}")


def build_t32dyn(path):
    base = 0x08048000
    interp = b"/lib32/ld-linux.so.2\x00"
    msg = b"APP-ENTRY via interp\n"

    ehsize, phentsize, phnum = 52, 32, 2
    interp_off = ehsize + phentsize * phnum
    code_off = interp_off + len(interp)

    code = b""
    code += b"\xb8" + struct.pack("<I", 4)
    code += b"\xbb" + struct.pack("<I", 1)
    code += b"\xb9" + struct.pack("<I", 0)              # mov ecx,msg (patched)
    code += b"\xba" + struct.pack("<I", len(msg))
    code += b"\xcd\x80"
    code += b"\xb8" + struct.pack("<I", 252)            # exit_group
    code += b"\x31\xdb"                                 # xor ebx,ebx
    code += b"\xcd\x80"

    msg_va = base + code_off + len(code)
    code = code[:11] + struct.pack("<I", msg_va) + code[15:]

    filesz = code_off + len(code) + len(msg)
    out = elf_header(base, base + code_off, ehsize, phnum)
    out += phdr(1, 0, base, filesz, filesz, 5, 0x1000)               # PT_LOAD R+X
    out += phdr(3, interp_off, base + interp_off, len(interp),
                len(interp), 4, 1)                                  # PT_INTERP
    out += interp + code + msg
    assert len(out) == filesz
    open(path, "wb").write(out)
    print(f"t32dyn: {len(out)} bytes, entry=0x{base + code_off:x}, "
          f"interp_off=0x{interp_off:x}")


if __name__ == "__main__":
    build_ld32(sys.argv[1])
    build_t32dyn(sys.argv[2])
