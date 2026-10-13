import struct, sys

msg = b"VEXA-32 hello from static i386 ELF\n"
base = 0x08048000
ehsize, phentsize = 52, 32
code_off = ehsize + phentsize  # file offset of code

code = b""
code += b"\xb8" + struct.pack("<I", 4)            # mov eax, 4 (write)
code += b"\xbb" + struct.pack("<I", 1)            # mov ebx, 1
code += b"\xb9" + struct.pack("<I", 0)            # mov ecx, msg (patched)
code += b"\xba" + struct.pack("<I", len(msg))     # mov edx, len
code += b"\xcd\x80"                               # int 0x80
code += b"\xb8" + struct.pack("<I", 252)          # mov eax, 252 (exit_group)
code += b"\x31\xdb"                               # xor ebx, ebx
code += b"\xcd\x80"                               # int 0x80
msg_va = base + code_off + len(code)
code = code[:11] + struct.pack("<I", msg_va) + code[15:]

filesz = code_off + len(code) + len(msg)

ehdr = b"\x7fELF" + bytes([1, 1, 1]) + bytes(9)   # ident: ELF32, LE, SYSV
ehdr += struct.pack("<HH", 2, 3)                  # ET_EXEC, EM_386
ehdr += struct.pack("<I", 1)                      # version
ehdr += struct.pack("<I", base + code_off)        # e_entry
ehdr += struct.pack("<I", ehsize)                 # e_phoff
ehdr += struct.pack("<I", 0)                      # e_shoff
ehdr += struct.pack("<I", 0)                      # e_flags
ehdr += struct.pack("<HHH", ehsize, phentsize, 1) # sizes, phnum
ehdr += struct.pack("<HHH", 0, 0, 0)              # shentsize, shnum, shstrndx

phdr = struct.pack("<IIIIIIII",
    1,                # PT_LOAD
    0,                # p_offset
    base, base,       # vaddr/paddr
    filesz, filesz,   # filesz/memsz
    5,                # R+X
    0x1000)           # align

out = ehdr + phdr + code + msg
assert len(out) == filesz, (len(out), filesz)
open(sys.argv[1], "wb").write(out)
print(f"wrote {len(out)} bytes, entry=0x{base+code_off:x}")
