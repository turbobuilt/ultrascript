#!/usr/bin/env python3

# Decode the actual machine code from the logs
machine_code_hex = """e9 a2 00 00 00 55 48 89 e5 41 57 48 c7 c7 08 00 
00 00 48 b8 50 d6 0a 23 47 71 00 00 ff d0 49 89 
c7 48 c7 c7 02 00 00 00 4c 89 fe 48 b8 52 af 66 
1f f9 57 00 00 ff d0 48 bf d0 5a 19 29 f9 57 00 
00 48 b8 61 59 66 1f f9 57 00 00 ff d0 48 89 c7 
48 b8 c8 4d 70 1f f9 57 00 00 ff d0 48 b8 03 52 
70 1f f9 57 00 00 ff d0 48 c7 c0 00 00 00 00 41 
5f 5d c3 4c 89 ff 48 b8 30 dd 0a 23 47 71 00 00 
ff d0 48 c7 c7 02 00 00 00 48 b8 a5 af 66 1f f9 
57 00 00 ff d0 4c 8b 64 24 10 4c 8b 6c 24 08 4c 
8b 34 24 48 83 c4 18 55 48 89 e5 41 57 48 81 ec 
b0 00 00 00 48 83 ec 18 4c 89 64 24 10 4c 89 6c 
24 08 4c 89 34 24 48 c7 c7 10 00 00 00 48 b8 50 
d6 0a 23 47 71 00 00 ff d0 49 89 c7 4c 89 ff 48 
c7 c6 00 00 00 00 48 c7 c2 10 00 00 00 48 b8 00 
94 18 23 47 71 00 00 ff d0 48 c7 c7 01 00 00 00 
4c 89 fe 48 b8 52 af 66 1f f9 57 00 00 ff d0 e8 
f1 fe ff ff e9 00 00 00 00 48 c7 c0 00 00 00 00 
48 81 c4 b0 00 00 00 41 5f 5d c3"""

# Clean up the hex string
hex_bytes = machine_code_hex.replace('\n', ' ').split()
code_bytes = [int(h, 16) for h in hex_bytes]

print(f"Total bytes: {len(code_bytes)}")
print()

# Look for the test function (starts at offset 5 according to logs)
print("=== test() FUNCTION ANALYSIS ===")
print("Function starts at offset 5:")

# The first 5 bytes are the jump to main: e9 a2 00 00 00
print("Bytes 0-4 (jump to main):", ' '.join(f"{b:02x}" for b in code_bytes[0:5]))
print()

# Function starts at byte 5
test_start = 5
print(f"test() function starts at byte {test_start}:")

# Look for the end of test function
# According to logs, main starts at offset 167
main_start = 167
test_end = main_start

print(f"test() function: bytes {test_start} to {test_end-1}")
test_bytes = code_bytes[test_start:test_end]

print("test() function machine code:")
for i in range(0, len(test_bytes), 16):
    chunk = test_bytes[i:i+16]
    offset = test_start + i
    hex_str = ' '.join(f"{b:02x}" for b in chunk)
    print(f"  {offset:3d}: {hex_str}")

print()

# Look for ret instructions (0xc3) in test function
ret_positions = []
for i, byte in enumerate(test_bytes):
    if byte == 0xc3:
        ret_positions.append(test_start + i)

print(f"RET instructions (0xc3) in test() function at offsets: {ret_positions}")

# Look at the end of test function specifically
print()
print("Last 20 bytes of test() function:")
end_chunk = test_bytes[-20:]
for i, byte in enumerate(end_chunk):
    offset = test_end - 20 + i
    print(f"  {offset:3d}: {byte:02x}")

print()
print("=== ANALYSIS ===")
if ret_positions:
    print(f"✅ test() function DOES have RET instruction(s) at: {ret_positions}")
else:
    print("❌ test() function is missing RET instruction")
