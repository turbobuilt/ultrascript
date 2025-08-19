#include "x86_codegen_v2.h"
#include <iostream>
#include <iomanip>
#include <sys/mman.h>
#include <unistd.h>
#include <cstring>


class MinimalExecutor {
private:
    void* executable_memory;
    size_t memory_size;
    
public:
    MinimalExecutor(const std::vector<uint8_t>& code) {
        memory_size = (code.size() + 4095) & ~4095;  // Round up to page boundary
        
        // Allocate executable memory
        executable_memory = mmap(nullptr, memory_size, 
                               PROT_READ | PROT_WRITE | PROT_EXEC,
                               MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        
        if (executable_memory == MAP_FAILED) {
            throw std::runtime_error("Failed to allocate executable memory");
        }
        
        // Copy code to executable memory
        memcpy(executable_memory, code.data(), code.size());
        
        // Make it executable
        if (mprotect(executable_memory, memory_size, PROT_READ | PROT_EXEC) != 0) {
            munmap(executable_memory, memory_size);
            throw std::runtime_error("Failed to make memory executable");
        }
    }
    
    ~MinimalExecutor() {
        if (executable_memory != MAP_FAILED && executable_memory != nullptr) {
            munmap(executable_memory, memory_size);
        }
    }
    
    int64_t call_function() {
        auto func = reinterpret_cast<int64_t(*)()>(executable_memory);
        return func();
    }
};

void test_minimal_execution() {
    std::cout << "=== Minimal Execution Test ===\n";
    
    X86CodeGenV2 codegen;
    
    // Generate: mov rax, 42; ret
    codegen.emit_mov_reg_imm(0, 42);  // RAX = 42
    codegen.emit_ret();
    
    auto code = codegen.get_code();
    
    std::cout << "Generated code: ";
    for (size_t i = 0; i < code.size(); i++) {
        std::cout << "0x" << std::hex << std::setfill('0') << std::setw(2) 
                  << static_cast<unsigned>(code[i]) << " ";
    }
    std::cout << std::dec << "\n";
    
    try {
        MinimalExecutor executor(code);
        int64_t result = executor.call_function();
        std::cout << "Function returned: " << result << "\n";
        
        if (result == 42) {
            std::cout << "✓ Test PASSED!\n";
        } else {
            std::cout << "✗ Test FAILED! Expected 42, got " << result << "\n";
        }
    } catch (const std::exception& e) {
        std::cout << "✗ Execution failed: " << e.what() << "\n";
    }
}

int main() {
    try {
        test_minimal_execution();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return 1;
    }
}
