#pragma once

// ============================================================================
// HIGH PERFORMANCE ASSEMBLY GENERATION FOR REFERENCE COUNTING
// UltraScript JIT optimization for reference counting operations
// ============================================================================

#include <string>
#include <sstream>
#include <iostream>
#include "refcount.h"  // For RefCountHeader and flags

class RefCountASMGenerator {
public:
    // Generate ultra-fast inline assembly for reference counting operations
    
    // Generate optimized retain operation (increment ref count)
    static std::string generate_retain_asm(const std::string& ptr_reg = "rax") {
        std::ostringstream asm_code;
        
        asm_code << "; Ultra-fast reference count increment\n";
        asm_code << "; Input: " << ptr_reg << " = object pointer\n";
        asm_code << "; Output: " << ptr_reg << " = same pointer (for chaining)\n";
        
        asm_code << "test " << ptr_reg << ", " << ptr_reg << "\n";
        asm_code << "jz .retain_null_" << generate_label() << "\n";
        
        // Calculate header address (ptr - sizeof(RefCountHeader))
        asm_code << "sub " << ptr_reg << ", " << sizeof(RefCountHeader) << "\n";
        
        // Atomic increment using lock xadd for maximum performance
        asm_code << "mov ecx, 1\n";
        asm_code << "lock xadd dword ptr [" << ptr_reg << "], ecx\n";
        
        // Restore user pointer
        asm_code << "add " << ptr_reg << ", " << sizeof(RefCountHeader) << "\n";
        
        asm_code << ".retain_null_" << current_label << ":\n";
        
        return asm_code.str();
    }
    
    // Generate optimized release operation (decrement ref count and potentially free)
    static std::string generate_release_asm(const std::string& ptr_reg = "rax") {
        std::ostringstream asm_code;
        
        asm_code << "; Ultra-fast reference count decrement with destroy check\n";
        asm_code << "; Input: " << ptr_reg << " = object pointer\n";
        
        asm_code << "test " << ptr_reg << ", " << ptr_reg << "\n";
        asm_code << "jz .release_null_" << generate_label() << "\n";
        
        // Calculate header address
        asm_code << "sub " << ptr_reg << ", " << sizeof(RefCountHeader) << "\n";
        
        // Atomic decrement using lock xadd
        asm_code << "mov ecx, -1\n";
        asm_code << "lock xadd dword ptr [" << ptr_reg << "], ecx\n";
        
        // Check if result is zero (was 1 before decrement)
        asm_code << "cmp ecx, 1\n";
        asm_code << "jne .release_not_zero_" << current_label << "\n";
        
        // Reference count hit zero - need to destroy object
        // Restore user pointer for destructor call
        asm_code << "add " << ptr_reg << ", " << sizeof(RefCountHeader) << "\n";
        asm_code << "push " << ptr_reg << "\n";
        
        // Call destructor (function pointer in header)
        asm_code << "sub " << ptr_reg << ", " << sizeof(RefCountHeader) << "\n";
        asm_code << "mov rbx, qword ptr [" << ptr_reg << " + " << offsetof(RefCountHeader, destructor) << "]\n";
        asm_code << "test rbx, rbx\n";
        asm_code << "jz .no_destructor_" << current_label << "\n";
        
        asm_code << "pop rdi  ; Object pointer as first argument\n";
        asm_code << "call rbx ; Call destructor\n";
        asm_code << "jmp .free_memory_" << current_label << "\n";
        
        asm_code << ".no_destructor_" << current_label << ":\n";
        asm_code << "pop " << ptr_reg << "  ; Clean up stack\n";
        
        asm_code << ".free_memory_" << current_label << ":\n";
        // Free the memory block (header + object)
        asm_code << "sub " << ptr_reg << ", " << sizeof(RefCountHeader) << "\n";
        asm_code << "mov rdi, " << ptr_reg << "\n";
        asm_code << "call free\n";
        asm_code << "jmp .release_done_" << current_label << "\n";
        
        asm_code << ".release_not_zero_" << current_label << ":\n";
        asm_code << "add " << ptr_reg << ", " << sizeof(RefCountHeader) << "\n";
        
        asm_code << ".release_null_" << generate_label() << ":\n";
        asm_code << ".release_done_" << current_label << ":\n";
        
        return asm_code.str();
    }
    
    // Generate optimized reference count check
    static std::string generate_get_count_asm(const std::string& ptr_reg = "rax", 
                                               const std::string& result_reg = "eax") {
        std::ostringstream asm_code;
        
        asm_code << "; Ultra-fast reference count read\n";
        asm_code << "; Input: " << ptr_reg << " = object pointer\n";
        asm_code << "; Output: " << result_reg << " = reference count\n";
        
        asm_code << "test " << ptr_reg << ", " << ptr_reg << "\n";
        asm_code << "jz .get_count_null_" << generate_label() << "\n";
        
        // Calculate header address and read ref count atomically
        asm_code << "sub " << ptr_reg << ", " << sizeof(RefCountHeader) << "\n";
        asm_code << "mov " << result_reg << ", dword ptr [" << ptr_reg << "]\n";
        asm_code << "add " << ptr_reg << ", " << sizeof(RefCountHeader) << "\n";
        asm_code << "jmp .get_count_done_" << current_label << "\n";
        
        asm_code << ".get_count_null_" << current_label << ":\n";
        asm_code << "xor " << result_reg << ", " << result_reg << "\n";
        
        asm_code << ".get_count_done_" << current_label << ":\n";
        
        return asm_code.str();
    }
    
    // Generate batch retain operation for arrays/multiple objects
    static std::string generate_batch_retain_asm() {
        std::ostringstream asm_code;
        
        asm_code << "; Ultra-fast batch retain operation\n";
        asm_code << "; Input: rdi = pointer array, rsi = count\n";
        
        asm_code << "test rsi, rsi\n";
        asm_code << "jz .batch_retain_done_" << generate_label() << "\n";
        
        asm_code << ".batch_retain_loop_" << current_label << ":\n";
        asm_code << "mov rax, qword ptr [rdi]\n";
        asm_code << "test rax, rax\n";
        asm_code << "jz .batch_retain_skip_" << current_label << "\n";
        
        // Prefetch next pointer for cache optimization
        asm_code << "prefetcht0 [rdi + 8]\n";
        
        // Inline retain operation
        asm_code << "sub rax, " << sizeof(RefCountHeader) << "\n";
        asm_code << "lock inc dword ptr [rax]\n";
        
        asm_code << ".batch_retain_skip_" << current_label << ":\n";
        asm_code << "add rdi, 8\n";
        asm_code << "dec rsi\n";
        asm_code << "jnz .batch_retain_loop_" << current_label << "\n";
        
        asm_code << ".batch_retain_done_" << current_label << ":\n";
        
        return asm_code.str();
    }
    
    // Generate optimized cycle breaking assembly (for free shallow)
    static std::string generate_break_cycles_asm() {
        std::ostringstream asm_code;
        
        asm_code << "; Optimized cycle breaking for 'free shallow'\n";
        asm_code << "; Input: rdi = object pointer\n";
        
        asm_code << "test rdi, rdi\n";
        asm_code << "jz .break_cycles_null_" << generate_label() << "\n";
        
        // Get header and set reference count to 1
        asm_code << "sub rdi, " << sizeof(RefCountHeader) << "\n";
        asm_code << "mov dword ptr [rdi], 1  ; Force ref count to 1\n";
        
        // Set cyclic flag
        asm_code << "or dword ptr [rdi + " << offsetof(RefCountHeader, flags) << "], " 
                  << REFCOUNT_FLAG_CYCLIC << "\n";
        
        // Restore pointer and call normal release
        asm_code << "add rdi, " << sizeof(RefCountHeader) << "\n";
        asm_code << "call rc_release\n";
        
        asm_code << ".break_cycles_null_" << current_label << ":\n";
        
        return asm_code.str();
    }
    
    // Generate optimized allocation with reference counting
    static std::string generate_alloc_asm() {
        std::ostringstream asm_code;
        
        asm_code << "; Ultra-fast reference counted allocation\n";
        asm_code << "; Input: rdi = size, rsi = type_id, rdx = destructor\n";
        asm_code << "; Output: rax = object pointer (or NULL)\n";
        
        // Calculate total size (header + object)
        asm_code << "add rdi, " << sizeof(RefCountHeader) << "\n";
        
        // Align to cache line boundary for performance
        asm_code << "add rdi, 63\n";
        asm_code << "and rdi, -64\n";
        
        // Call aligned_alloc
        asm_code << "mov rsi, rdi  ; size\n";
        asm_code << "mov rdi, 64   ; alignment\n";
        asm_code << "call aligned_alloc\n";
        
        asm_code << "test rax, rax\n";
        asm_code << "jz .alloc_failed_" << generate_label() << "\n";
        
        // Initialize header with ref count = 1
        asm_code << "mov dword ptr [rax], 1      ; ref_count = 1\n";
        asm_code << "mov dword ptr [rax + 4], 1  ; weak_count = 1\n";
        asm_code << "mov dword ptr [rax + 8], esi ; type_id\n";
        asm_code << "mov qword ptr [rax + 16], rdx ; destructor\n";
        asm_code << "mov dword ptr [rax + 24], 0  ; flags = 0\n";
        
        // Return user pointer (header + sizeof(RefCountHeader))
        asm_code << "add rax, " << sizeof(RefCountHeader) << "\n";
        
        asm_code << ".alloc_failed_" << current_label << ":\n";
        
        return asm_code.str();
    }
    
    // Generate complete function for JIT integration
    static std::string generate_complete_function(const std::string& func_name,
                                                   const std::string& body_asm) {
        std::ostringstream asm_code;
        
        asm_code << "; Generated by UltraScript RefCount JIT Compiler\n";
        asm_code << ".text\n";
        asm_code << ".globl " << func_name << "\n";
        asm_code << ".type " << func_name << ", @function\n";
        asm_code << func_name << ":\n";
        
        // Function prologue
        asm_code << "push rbp\n";
        asm_code << "mov rbp, rsp\n";
        
        // Function body
        asm_code << body_asm;
        
        // Function epilogue
        asm_code << "mov rsp, rbp\n";
        asm_code << "pop rbp\n";
        asm_code << "ret\n";
        
        asm_code << ".size " << func_name << ", .-" << func_name << "\n";
        
        return asm_code.str();
    }

private:
    static int current_label;
    
    static int generate_label() {
        return ++current_label;
    }
};

int RefCountASMGenerator::current_label = 0;

// ============================================================================
// JIT INTEGRATION FUNCTIONS
// ============================================================================

// Generate optimized assembly for specific operations
extern "C" {
    // Generate retain operation for JIT
    const char* jit_generate_retain(const char* ptr_register);
    
    // Generate release operation for JIT  
    const char* jit_generate_release(const char* ptr_register);
    
    // Generate batch operations for JIT
    const char* jit_generate_batch_retain();
    const char* jit_generate_batch_release();
    
    // Generate cycle breaking for 'free shallow'
    const char* jit_generate_break_cycles();
}
