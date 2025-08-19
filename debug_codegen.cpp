#include "x86_codegen_v2.h"
#include <iostream>
#include <iomanip>


void debug_basic_mov() {
    std::cout << "Debugging basic MOV instruction generation...\n";
    
    X86CodeGenV2 codegen;
    
    // Generate: mov rax, 42; ret
    codegen.emit_mov_reg_imm(0, 42);  // RAX = 42
    codegen.emit_ret();
    
    auto code = codegen.get_code();
    
    std::cout << "Generated " << code.size() << " bytes:\n";
    for (size_t i = 0; i < code.size(); i++) {
        std::cout << "0x" << std::hex << std::setfill('0') << std::setw(2) 
                  << static_cast<unsigned>(code[i]) << " ";
        if ((i + 1) % 8 == 0) std::cout << "\n";
    }
    std::cout << std::dec << "\n";
    
    // Expected for MOV RAX, 42:
    // REX.W (0x48) + MOV r64, imm32 (0xC7) + ModR/M for RAX (0xC0) + imm32 (0x2A 0x00 0x00 0x00)
    // Plus RET (0xC3)
    // Total: 0x48 0xC7 0xC0 0x2A 0x00 0x00 0x00 0xC3
    
    std::cout << "Expected: 0x48 0xC7 0xC0 0x2A 0x00 0x00 0x00 0xC3\n";
}

int main() {
    try {
        std::cout << "Starting debug...\n";
        debug_basic_mov();
        std::cout << "Debug completed.\n";
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }
    return 0;
}
