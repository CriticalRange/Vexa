import struct
import subprocess

objdump_path = r"C:\Users\zeytu\AppData\Local\Android\Sdk\ndk\29.0.14206865\toolchains\llvm\prebuilt\windows-x86_64\bin\llvm-objdump.exe"

instructions = [
    ("host_pc_insn (crash 1)", 0x38bfc164),
    ("host_lr_insn (crash 1)", 0x9400006c),
    ("host_pc_insn (crash 2)", 0xd65f03c0),
    ("host_lr_insn (crash 2)", 0xd63f0060),
]

for label, insn in instructions:
    data = struct.pack("<I", insn)
    bin_file = f"test_{insn:08x}.bin"
    with open(bin_file, "wb") as f:
        f.write(data)
    
    res = subprocess.run([objdump_path, "-D", "--arch-name=aarch64", bin_file], capture_output=True, text=True)
    print(f"=== {label} (0x{insn:08x}) ===")
    print("STDOUT:", res.stdout.strip())
    print("STDERR:", res.stderr.strip())
