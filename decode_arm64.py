val = 0x38bfc164
print(f"Hex: 0x{val:08x}")
print(f"Binary: {val:032b}")

# Bit fields:
# 31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0
# [31:30] = size
# [29:27] = 111 (Load/store unscaled immediate / unprivileged / pair / etc.)
# Let's inspect all fields

def bits(v, high, low):
    mask = (1 << (high - low + 1)) - 1
    return (v >> low) & mask

print(f"bits[31:30] = {bits(val, 31, 30):02b}")
print(f"bits[29:24] = {bits(val, 29, 24):06b}")
print(f"bits[23:22] = {bits(val, 23, 22):02b}")
print(f"bits[21:10] = {bits(val, 21, 10):012b}")
print(f"bits[9:5]   = {bits(val, 9, 5):05b} (Rn = x{bits(val, 9, 5)})")
print(f"bits[4:0]   = {bits(val, 4, 0):05b} (Rt = x{bits(val, 4, 0)})")
