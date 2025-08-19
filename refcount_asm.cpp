#include "refcount_asm.h"
#include "refcount.h"
#include <cstring>
#include <cstdlib>
#include <iostream>

// ============================================================================
// HIGH PERFORMANCE ASSEMBLY GENERATION IMPLEMENTATION
// ============================================================================

// Static storage for generated assembly strings
static char* retain_asm_buffer = nullptr;
static char* release_asm_buffer = nullptr;
static char* batch_retain_asm_buffer = nullptr;
static char* batch_release_asm_buffer = nullptr;
static char* break_cycles_asm_buffer = nullptr;

// Helper to allocate and copy string
static const char* allocate_asm_string(const std::string& asm_code) {
    size_t len = asm_code.length() + 1;
    char* buffer = static_cast<char*>(malloc(len));
    if (buffer) {
        strcpy(buffer, asm_code.c_str());
    }
    return buffer;
}

extern "C" {

const char* jit_generate_retain(const char* ptr_register) {
    std::string reg = ptr_register ? ptr_register : "rax";
    std::string asm_code = RefCountASMGenerator::generate_retain_asm(reg);
    
    free(retain_asm_buffer);
    retain_asm_buffer = const_cast<char*>(allocate_asm_string(asm_code));
    return retain_asm_buffer;
}

const char* jit_generate_release(const char* ptr_register) {
    std::string reg = ptr_register ? ptr_register : "rax";
    std::string asm_code = RefCountASMGenerator::generate_release_asm(reg);
    
    free(release_asm_buffer);
    release_asm_buffer = const_cast<char*>(allocate_asm_string(asm_code));
    return release_asm_buffer;
}

const char* jit_generate_batch_retain() {
    std::string asm_code = RefCountASMGenerator::generate_batch_retain_asm();
    
    free(batch_retain_asm_buffer);
    batch_retain_asm_buffer = const_cast<char*>(allocate_asm_string(asm_code));
    return batch_retain_asm_buffer;
}

const char* jit_generate_batch_release() {
    // Generate batch release assembly (similar to batch retain but with release logic)
    std::ostringstream asm_code;
    
    asm_code << "; Ultra-fast batch release operation\n";
    asm_code << "; Input: rdi = pointer array, rsi = count\n";
    
    asm_code << "test rsi, rsi\n";
    asm_code << "jz .batch_release_done\n";
    
    asm_code << ".batch_release_loop:\n";
    asm_code << "mov rax, qword ptr [rdi]\n";
    asm_code << "test rax, rax\n";
    asm_code << "jz .batch_release_skip\n";
    
    // Prefetch next pointer
    asm_code << "prefetcht0 [rdi + 8]\n";
    
    // Inline release operation (simplified)
    asm_code << "push rdi\n";
    asm_code << "push rsi\n";
    asm_code << "mov rdi, rax\n";
    asm_code << "call rc_release\n";
    asm_code << "pop rsi\n";
    asm_code << "pop rdi\n";
    
    asm_code << ".batch_release_skip:\n";
    asm_code << "add rdi, 8\n";
    asm_code << "dec rsi\n";
    asm_code << "jnz .batch_release_loop\n";
    
    asm_code << ".batch_release_done:\n";
    
    std::string asm_str = asm_code.str();
    free(batch_release_asm_buffer);
    batch_release_asm_buffer = const_cast<char*>(allocate_asm_string(asm_str));
    return batch_release_asm_buffer;
}

const char* jit_generate_break_cycles() {
    std::string asm_code = RefCountASMGenerator::generate_break_cycles_asm();
    
    free(break_cycles_asm_buffer);
    break_cycles_asm_buffer = const_cast<char*>(allocate_asm_string(asm_code));
    return break_cycles_asm_buffer;
}

}

// ============================================================================
// DEMONSTRATION OF GENERATED ASSEMBLY
// ============================================================================

void demonstrate_generated_assembly() {
    std::cout << "\n=== GENERATED ASSEMBLY DEMONSTRATION ===" << std::endl;
    
    std::cout << "\n--- RETAIN OPERATION ---" << std::endl;
    std::cout << RefCountASMGenerator::generate_retain_asm("rdi") << std::endl;
    
    std::cout << "\n--- RELEASE OPERATION ---" << std::endl;
    std::cout << RefCountASMGenerator::generate_release_asm("rdi") << std::endl;
    
    std::cout << "\n--- GET COUNT OPERATION ---" << std::endl;
    std::cout << RefCountASMGenerator::generate_get_count_asm("rdi", "eax") << std::endl;
    
    std::cout << "\n--- BATCH RETAIN ---" << std::endl;
    std::cout << RefCountASMGenerator::generate_batch_retain_asm() << std::endl;
    
    std::cout << "\n--- CYCLE BREAKING (FREE SHALLOW) ---" << std::endl;
    std::cout << RefCountASMGenerator::generate_break_cycles_asm() << std::endl;
    
    std::cout << "\n--- OPTIMIZED ALLOCATION ---" << std::endl;
    std::cout << RefCountASMGenerator::generate_alloc_asm() << std::endl;
}
