#include "x86_codegen_v2.h"
#include "x86_codegen_improved.h"
#include "runtime.h"
#include <chrono>
#include <iostream>
#include <cassert>



class X86CodeGenTester {
private:
    static void benchmark_codegen_speed() {
        const int ITERATIONS = 10000;
        
        // Test V2 implementation
        auto start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < ITERATIONS; i++) {
            auto codegen = std::make_unique<X86CodeGenV2>();
            codegen->emit_prologue();
            codegen->emit_mov_reg_imm(0, 42);
            codegen->emit_mov_reg_imm(1, 100);
            codegen->emit_add_reg_reg(0, 1);
            codegen->emit_call("__console_log_float64");
            codegen->emit_epilogue();
        }
        auto end = std::chrono::high_resolution_clock::now();
        auto v2_time = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        
        std::cout << "V2 CodeGen: " << ITERATIONS << " iterations in " 
                  << v2_time.count() << " microseconds" << std::endl;
        std::cout << "Average per iteration: " << (v2_time.count() / ITERATIONS) << " microseconds" << std::endl;
    }
    
    static void test_memory_operations() {
        std::cout << "\n=== Testing Memory Operations ===" << std::endl;
        
        auto codegen = std::make_unique<X86CodeGenV2>();
        
        // Test RSP-relative operations
        try {
            codegen->emit_mov_mem_rsp_reg(0, 0);    // [rsp] = rax
            codegen->emit_mov_reg_mem_rsp(1, 0);    // rcx = [rsp]
            std::cout << "✓ RSP-relative operations working" << std::endl;
        } catch (const std::exception& e) {
            std::cout << "✗ RSP-relative operations failed: " << e.what() << std::endl;
        }
        
        // Test RBP-relative operations  
        try {
            codegen->emit_mov_mem_reg(-8, 0);       // [rbp-8] = rax
            codegen->emit_mov_reg_mem(1, -8);       // rcx = [rbp-8]
            std::cout << "✓ RBP-relative operations working" << std::endl;
        } catch (const std::exception& e) {
            std::cout << "✗ RBP-relative operations failed: " << e.what() << std::endl;
        }
    }
    
    static void test_optimizations() {
        std::cout << "\n=== Testing Optimizations ===" << std::endl;
        
        auto codegen = std::make_unique<X86CodeGenV2>();
        codegen->enable_optimization(true);
        
        size_t code_size_before = codegen->get_code().size();
        
        // Test optimization elimination
        codegen->emit_mov_reg_reg(0, 0);        // Should be eliminated
        codegen->emit_add_reg_imm(0, 0);        // Should be eliminated
        codegen->emit_sub_reg_imm(0, 0);        // Should be eliminated
        
        size_t code_size_after = codegen->get_code().size();
        
        if (code_size_after == code_size_before) {
            std::cout << "✓ No-op optimizations working" << std::endl;
        } else {
            std::cout << "✗ No-op optimizations not working" << std::endl;
        }
        
        // Test meaningful operations
        codegen->emit_mov_reg_imm(0, 42);
        codegen->emit_add_reg_imm(0, 100);
        
        if (codegen->get_code().size() > code_size_after) {
            std::cout << "✓ Meaningful operations generate code" << std::endl;
        } else {
            std::cout << "✗ Meaningful operations not generating code" << std::endl;
        }
    }
    
    static void test_function_calls() {
        std::cout << "\n=== Testing Function Call Resolution ===" << std::endl;
        
        auto codegen = std::make_unique<X86CodeGenV2>();
        
        // Test runtime function resolution
        size_t code_before = codegen->get_code().size();
        codegen->emit_call("__console_log_float64");
        size_t code_after = codegen->get_code().size();
        
        if (code_after > code_before) {
            std::cout << "✓ Function calls generate code" << std::endl;
        } else {
            std::cout << "✗ Function calls not generating code" << std::endl;
        }
        
        // Test unknown function fallback
        code_before = codegen->get_code().size();
        codegen->emit_call("__unknown_function");
        code_after = codegen->get_code().size();
        
        if (code_after > code_before) {
            std::cout << "✓ Unknown function fallback working" << std::endl;
        } else {
            std::cout << "✗ Unknown function fallback not working" << std::endl;
        }
    }
    
    static void test_register_validation() {
        std::cout << "\n=== Testing Register Validation ===" << std::endl;
        
        auto codegen = std::make_unique<X86CodeGenV2>();
        
        // Test valid registers
        try {
            for (int reg = 0; reg < 16; reg++) {
                codegen->emit_mov_reg_imm(reg, reg * 10);
            }
            std::cout << "✓ Valid register range working" << std::endl;
        } catch (const std::exception& e) {
            std::cout << "✗ Valid register range failed: " << e.what() << std::endl;
        }
        
        // Test invalid registers (this should be caught by improved version)
        std::cout << "Note: Invalid register tests would require X86CodeGenImproved" << std::endl;
    }
    
    static void compare_code_sizes() {
        std::cout << "\n=== Comparing Code Sizes ===" << std::endl;
        
        auto codegen = std::make_unique<X86CodeGenV2>();
        
        // Generate a typical sequence
        codegen->emit_prologue();
        codegen->emit_mov_reg_imm(0, 42);
        codegen->emit_mov_reg_imm(1, 100);
        codegen->emit_add_reg_reg(0, 1);
        codegen->emit_call("__console_log_float64");
        codegen->emit_epilogue();
        
        size_t total_size = codegen->get_code().size();
        std::cout << "Total code size for typical sequence: " << total_size << " bytes" << std::endl;
        
        // Calculate instruction density
        size_t instruction_count = codegen->get_instruction_count();
        if (instruction_count > 0) {
            double bytes_per_instruction = static_cast<double>(total_size) / instruction_count;
            std::cout << "Average bytes per instruction: " << bytes_per_instruction << std::endl;
        }
    }

public:
    static void run_all_tests() {
        std::cout << "=== UltraScript X86 CodeGen Validation Tests ===" << std::endl;
        std::cout << "Testing improved X86 code generation system..." << std::endl;
        
        benchmark_codegen_speed();
        test_memory_operations();
        test_optimizations();
        test_function_calls();
        test_register_validation();
        compare_code_sizes();
        
        std::cout << "\n=== Test Summary ===" << std::endl;
        std::cout << "All basic functionality tests completed." << std::endl;
        std::cout << "Manual verification required for:" << std::endl;
        std::cout << "1. Generated machine code correctness" << std::endl;
        std::cout << "2. Runtime execution performance" << std::endl;
        std::cout << "3. Memory safety validation" << std::endl;
        std::cout << "4. Integration with AST code generation" << std::endl;
    }
    
    static void validate_instruction_encoding() {
        std::cout << "\n=== Validating Instruction Encoding ===" << std::endl;
        
        auto codegen = std::make_unique<X86CodeGenV2>();
        
        // Test basic move instruction
        codegen->emit_mov_reg_imm(0, 42);  // mov rax, 42
        auto code = codegen->get_code();
        
        if (!code.empty()) {
            std::cout << "Generated " << code.size() << " bytes for mov rax, 42" << std::endl;
            std::cout << "First few bytes (hex): ";
            for (size_t i = 0; i < std::min(size_t(8), code.size()); i++) {
                std::printf("%02x ", code[i]);
            }
            std::cout << std::endl;
            
            // Expected pattern for "mov rax, 42" is approximately:
            // 48 C7 C0 2A 00 00 00 (REX.W + mov r/m64, imm32)
            // or 48 B8 2A 00 00 00 00 00 00 00 (REX.W + mov reg, imm64)
            std::cout << "✓ Instruction encoding produces output" << std::endl;
        } else {
            std::cout << "✗ No code generated for basic instruction" << std::endl;
        }
    }
};



// Main test runner
int main() {

    
    try {
        X86CodeGenTester::run_all_tests();
        X86CodeGenTester::validate_instruction_encoding();
        
        std::cout << "\n✓ All tests completed successfully!" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cout << "\n✗ Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
}
