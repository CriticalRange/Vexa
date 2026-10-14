# Let's decode 0x38bfc164 as an ARM64 instruction
# Opcode: 0x38bfc164
# In binary:
# 0011 1000 1011 1111 1100 0001 0110 0100
# Bits:
# size (31:30) = 00 -> byte (8-bit)
# 111 (29:27) = 111
# V (26) = 0
# 00 (25:24) = 00
# opc (23:22) = 10 -> LDRSB (64-bit) or LDRB
# 1 (21) = 1
# Rm (20:16) = 11111 (31 / ZR)
# option (15:13) = 110 (SXTX)
# S (12) = 0
# 10 (11:10) = 10
# Rn (9:5) = 01011 (11 -> x11)
# Rt (4:0) = 00100 (4 -> x4)
# Or is it ldrb?
# Let's check with capstone or online disassembler logic if available.
import sys

# Let's check capstone
try:
    import capstone
    md = capstone.Cs(capstone.CS_ARCH_ARM64, capstone.CS_MODE_ARM)
    code = (0x38bfc164).to_bytes(4, 'little')
    for ins in md.disasm(code, 0):
        print(f'{ins.mnemonic} {ins.op_str}')
except ImportError:
    print('No capstone')
