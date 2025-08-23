import sys
code_hex = """55 48 89 e5 48 bf d0 ef 8d 55 55 55 00 00 48 b8 4d 60 65 55 55 55 00 00 ff d0 48 89 c7 48 c7 c6 10 00 00 00 48 b8 7b a4 65 55 55 55 00 00 ff d0 48 89 45 d0 48 89 ef 48 c7 c6 d0 ff ff ff 48 89 c2 48 b8 1b 39 65 55 55 55 00 00 ff d0 48 8b 45 d0 48 89 45 c8 48 c7 c0 0a 00 00 00 48 8b 55 c8 48 89 42 20 48 8b 45 d0 48 89 45 c8"""
bytes_list = code_hex.replace("\n", " ").split()
print("Instruction at offset 0x60 (96 decimal):")
for i, b in enumerate(bytes_list):
    if i >= 96 and i < 96+16:  # Show 16 bytes starting from offset 0x60
        print(f"0x{i:02x}: {b}")

