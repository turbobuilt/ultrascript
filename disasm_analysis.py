#!/usr/bin/env python3

# Let's analyze the machine code to understand the crash
machine_code = """e9 a2 00 00 00 55 48 89 e5 41 57 48 c7 c7 08 00 00 00 48 b8 50 d6 2a eb 2f 71 00 00 ff d0 49 89 c7 48 c7 c7 02 00 00 00 4c 89 fe 48 b8 28 3a a6 40 48 5a 00 00 ff d0 48 bf 90 db a9 51 48 5a 00 00 48 b8 37 e4 a5 40 48 5a 00 00 ff d0 48 89 c7 48 b8 9e d8 af 40 48 5a 00 00 ff d0 48 b8 d9 dc af 40 48 5a 00 00 ff d0 48 c7 c0 00 00 00 00 41 5f 5d c3 4c 89 ff 48 b8 30 dd 2a eb 2f 71 00 00 ff d0 48 c7 c7 02 00 00 00 48 b8 7b 3a a6 40 48 5a 00 00 ff d0 4c 8b 64 24 10 4c 8b 6c 24 08 4c 8b 34 24 48 83 c4 18 55 48 89 e5 41 57 48 81 ec c0 00 00 00 48 83 ec 18 4c 89 64 24 10 4c 89 6c 24 08 4c 89 34 24 48 c7 c7 20 00 00 00 48 b8 50 d6 2a eb 2f 71 00 00 ff d0 49 89 c7 4c 89 ff 48 c7 c6 00 00 00 00 48 c7 c2 20 00 00 00 48 b8 00 94 38 eb 2f 71 00 00 ff d0 48 c7 c7 01 00 00 00 4c 89 fe 48 b8 28 3a a6 40 48 5a 00 00 ff d0 48 bf 00 00 00 00 00 00 45 40 48 b8 52 11 a6 40 48 5a 00 00 ff d0 49 89 47 10 e8 d7 fe ff ff e9 00 00 00 00 48 c7 c0 00 00 00 00 48 81 c4 c0 00 00 00 41 5f 5d c3"""

# Remove spaces and newlines
code_bytes = bytes.fromhex(machine_code.replace(' ', '').replace('\n', ''))

print("Machine code analysis:")
print(f"Total size: {len(code_bytes)} bytes")
print()

# Key addresses
print("Key sections:")
print("Address 0: Jump to main")
print("Address 5-166: Function 'test'")  
print("Address 167-306: Main function")
print("Address 307+: Main epilogue")
print()

# Let's examine the end of the test function and the main function call
print("Critical sections to examine:")

# End of test function (around address 130-166)
print("End of test function (should be around bytes 130-166):")
end_test = code_bytes[130:167]
print(f"Bytes: {end_test.hex(' ')}")

# Main function start (address 167)
print(f"\nMain function start (byte 167):")
main_start = code_bytes[167:200]  
print(f"Bytes: {main_start.hex(' ')}")

# Look for the function call in main (should be near end)
print(f"\nEnd of main function (around bytes 280-307):")
main_end = code_bytes[280:307]
print(f"Bytes: {main_end.hex(' ')}")

# Main epilogue
print(f"\nMain epilogue (byte 307+):")
if len(code_bytes) > 307:
    epilogue = code_bytes[307:]
    print(f"Bytes: {epilogue.hex(' ')}")

print("\nLooking for suspicious patterns:")
print("- Function calls (ff d0)")
print("- Returns (c3)")  
print("- Stack operations (48 81 c4, 48 83 c4)")
print("- Register restores (41 5f = pop r15)")
