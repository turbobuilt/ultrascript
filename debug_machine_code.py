#!/usr/bin/env python3

# Simple tool to analyze the generated machine code
import sys

def analyze_machine_code():
    # From the debug output, the machine code is:
    machine_code = """e9 d8 00 00 00 55 48 89 e5 48 c7 c7 10 00 00 00 
48 b8 50 d6 8a bc 7b 70 00 00 ff d0 4c 89 c7 49 
89 ff 48 c7 c6 00 00 00 00 48 c7 c2 10 00 00 00 
48 b8 00 94 98 bc 7b 70 00 00 ff d0 48 c7 c7 02 
00 00 00 49 89 fe 48 b8 62 00 82 44 54 62 00 00 
ff d0 49 89 3f 48 bf f0 bb 38 5b 54 62 00 00 48 
b8 61 b0 81 44 54 62 00 00 ff d0 48 89 c7 48 b8 
b4 44 8b 44 54 62 00 00 ff d0 48 b8 3b 48 8b 44 
54 62 00 00 ff d0 49 8b 07 48 89 c7 48 b8 94 49 
8b 44 54 62 00 00 ff d0 48 b8 ef 48 8b 44 54 62 
00 00 ff d0 49 8b 07 5d c3 49 89 ff 48 b8 30 dd 
8a bc 7b 70 00 00 ff d0 48 c7 c7 02 00 00 00 48 
b8 b5 00 82 44 54 62 00 00 ff d0 4c 8b 64 24 10 
4c 8b 6c 24 08 4c 8b 34 24 48 83 c4 18 55 48 89 
e5 48 81 ec c0 00 00 00 48 b8 00 00 00 00 00 00 
45 40 48 89 c7 48 b8 7c dd 81 44 54 62 00 00 ff 
d0 48 89 c7 e8 fc fe ff ff 49 89 07 48 bf 70 74 
38 5b 54 62 00 00 48 b8 61 b0 81 44 54 62 00 00 
ff d0 48 89 c7 48 b8 b4 44 8b 44 54 62 00 00 ff 
d0 48 b8 3b 48 8b 44 54 62 00 00 ff d0 49 8b 07 
48 89 c7 48 b8 94 49 8b 44 54 62 00 00 ff d0 48 
b8 ef 48 8b 44 54 62 00 00 ff d0 e9 00 00 00 00 
48 c7 c0 00 00 00 00 48 81 c4 c0 00 00 00 5d c3"""

    # Key instructions to look for:
    # 49 8b 07 = mov rax, [r15]     - Loading from R15 (parameter access)
    # 4c 89 ff = mov rdi, r15       - Moving R15 to RDI for free() call
    # ff d0    = call rax           - Function calls
    
    print("=== MACHINE CODE ANALYSIS ===")
    print("Looking for parameter access patterns...")
    
    bytes_hex = machine_code.replace('\n', ' ').split()
    
    # Find parameter access instructions
    i = 0
    while i < len(bytes_hex) - 2:
        if (bytes_hex[i] == '49' and bytes_hex[i+1] == '8b' and bytes_hex[i+2] == '07'):
            print(f"Found parameter access at offset {i*3}: 49 8b 07 (mov rax, [r15])")
        elif (bytes_hex[i] == '4c' and bytes_hex[i+1] == '89' and bytes_hex[i+2] == 'ff'):
            print(f"Found R15->RDI move at offset {i*3}: 4c 89 ff (mov rdi, r15) - likely free() setup")
        elif (bytes_hex[i] == 'ff' and bytes_hex[i+1] == 'd0'):
            print(f"Found function call at offset {i*3}: ff d0 (call rax)")
        i += 1
    
    print("\n=== FUNCTION BOUNDARIES ===")
    print("testParam function starts at offset 5")
    print("__main function starts at offset 221") 
    
    # Convert decimal offsets to find where testParam ends
    print(f"Analyzing function testParam (offset 5 = 0x{5:x})...")
    print(f"Main function at offset 221 = 0x{221:x}")

if __name__ == "__main__":
    analyze_machine_code()
