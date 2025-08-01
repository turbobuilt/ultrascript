#include "x86_codegen_v2.h"
#include <iostream>
#include <iomanip>
#include <sys/mman.h>
#include <unistd.h>
#include <cstring>

using namespace ultraScript;

// Simple debug test without execution
void debug_code_generation() {
    std::cout << "=== Debug Code Generation ===\n";
    
    X86CodeGenV2 codegen;
    
    // Generate: mov rax, 42; ret
    std::cout << "Generating MOV RAX, 42...\n";
    codegen.emit_mov_reg_imm(0, 42);  // RAX = 42
    
    std::cout << "Generating RET...\n";
    codegen.emit_ret();
    
    auto code = codegen.get_code();
    
    std::cout << "Generated " << code.size() << " bytes:\n";
    for (size_t i = 0; i < code.size(); i++) {
        std::cout << "0x" << std::hex << std::setfill('0') << std::setw(2) 
                  << static_cast<unsigned>(code[i]);
        if (i < code.size() - 1) std::cout << " ";
    }
    std::cout << std::dec << "\n";
    
    // Manual verification
    std::cout << "\nExpected sequence:\n";
    std::cout << "MOV RAX, 42: 0x48 0xC7 0xC0 0x2A 0x00 0x00 0x00\n";
    std::cout << "RET:         0xC3\n";
    std::cout << "Total:       0x48 0xC7 0xC0 0x2A 0x00 0x00 0x00 0xC3\n";
}

int main() {
    try {
        debug_code_generation();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return 1;
    }
}
