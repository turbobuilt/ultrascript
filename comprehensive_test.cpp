#include "x86_codegen_v2.h"
#include <iostream>
#include <iomanip>
#include <sys/mman.h>
#include <unistd.h>
#include <cstring>
#include <chrono>
#include <cassert>

using namespace ultraScript;

class TestExecutor {
private:
    void* executable_memory;
    size_t memory_size;
    
public:
    TestExecutor(const std::vector<uint8_t>& code) {
        memory_size = (code.size() + 4095) & ~4095;
        executable_memory = mmap(nullptr, memory_size, 
                               PROT_READ | PROT_WRITE | PROT_EXEC,
                               MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        
        if (executable_memory == MAP_FAILED) {
            throw std::runtime_error("Failed to allocate executable memory");
        }
        
        memcpy(executable_memory, code.data(), code.size());
        mprotect(executable_memory, memory_size, PROT_READ | PROT_EXEC);
    }
    
    ~TestExecutor() {
        if (executable_memory != MAP_FAILED && executable_memory != nullptr) {
            munmap(executable_memory, memory_size);
        }
    }
    
    int64_t call_function() {
        auto func = reinterpret_cast<int64_t(*)()>(executable_memory);
        return func();
    }
    
    int64_t call_function_with_arg(int64_t arg) {
        auto func = reinterpret_cast<int64_t(*)(int64_t)>(executable_memory);
        return func(arg);
    }
};

void test_arithmetic() {
    std::cout << "=== Arithmetic Test ===\n";
    
    X86CodeGenV2 codegen;
    
    // Generate: mov rax, 10; add rax, 5; ret
    codegen.emit_mov_reg_imm(0, 10);   // RAX = 10
    codegen.emit_add_reg_imm(0, 5);    // RAX += 5
    codegen.emit_ret();
    
    auto code = codegen.get_code();
    TestExecutor executor(code);
    int64_t result = executor.call_function();
    
    std::cout << "10 + 5 = " << result << " (expected 15)\n";
    assert(result == 15);
    std::cout << "✓ Arithmetic test passed\n";
}

void test_register_operations() {
    std::cout << "=== Register Operations Test ===\n";
    
    X86CodeGenV2 codegen;
    
    // Generate: mov rax, 20; mov rbx, 30; add rax, rbx; ret
    codegen.emit_mov_reg_imm(0, 20);   // RAX = 20
    codegen.emit_mov_reg_imm(3, 30);   // RBX = 30  
    codegen.emit_add_reg_reg(0, 3);    // RAX += RBX
    codegen.emit_ret();
    
    auto code = codegen.get_code();
    TestExecutor executor(code);
    int64_t result = executor.call_function();
    
    std::cout << "20 + 30 = " << result << " (expected 50)\n";
    assert(result == 50);
    std::cout << "✓ Register operations test passed\n";
}

void test_function_parameter() {
    std::cout << "=== Function Parameter Test ===\n";
    
    X86CodeGenV2 codegen;
    
    // Generate function that doubles its input: mov rax, rdi; add rax, rdi; ret
    codegen.emit_mov_reg_reg(0, 7);    // RAX = RDI (first parameter)
    codegen.emit_add_reg_reg(0, 7);    // RAX += RDI (double it)
    codegen.emit_ret();
    
    auto code = codegen.get_code();
    TestExecutor executor(code);
    
    int64_t result1 = executor.call_function_with_arg(5);
    std::cout << "double(5) = " << result1 << " (expected 10)\n";
    assert(result1 == 10);
    
    int64_t result2 = executor.call_function_with_arg(25);
    std::cout << "double(25) = " << result2 << " (expected 50)\n";
    assert(result2 == 50);
    
    std::cout << "✓ Function parameter test passed\n";
}

void test_subtraction() {
    std::cout << "=== Subtraction Test ===\n";
    
    X86CodeGenV2 codegen;
    
    // Generate: mov rax, 100; sub rax, 30; ret
    codegen.emit_mov_reg_imm(0, 100);  // RAX = 100
    codegen.emit_sub_reg_imm(0, 30);   // RAX -= 30
    codegen.emit_ret();
    
    auto code = codegen.get_code();
    TestExecutor executor(code);
    int64_t result = executor.call_function();
    
    std::cout << "100 - 30 = " << result << " (expected 70)\n";
    assert(result == 70);
    std::cout << "✓ Subtraction test passed\n";
}

void benchmark_generation_speed() {
    std::cout << "=== Generation Speed Benchmark ===\n";
    
    auto start = std::chrono::high_resolution_clock::now();
    const int iterations = 10000;
    
    for (int i = 0; i < iterations; i++) {
        X86CodeGenV2 codegen;
        
        // Generate a moderately complex function
        codegen.emit_mov_reg_imm(0, i);
        codegen.emit_add_reg_imm(0, 1);
        codegen.emit_mov_reg_reg(1, 0);
        codegen.emit_add_reg_reg(0, 1);
        codegen.emit_sub_reg_imm(0, 5);
        codegen.emit_ret();
        
        auto code = codegen.get_code();  // Force generation
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    std::cout << "Generated " << iterations << " functions in " 
              << duration.count() << " microseconds\n";
    std::cout << "Average: " << (static_cast<double>(duration.count()) / iterations) 
              << " microseconds per function\n";
    std::cout << "✓ Performance benchmark completed\n";
}

int main() {
    try {
        std::cout << "Starting comprehensive tests...\n";
        
        std::cout << "Running arithmetic test...\n";
        test_arithmetic();
        
        std::cout << "Running register operations test...\n";
        test_register_operations();
        
        std::cout << "Running function parameter test...\n";
        test_function_parameter();
        
        std::cout << "Running subtraction test...\n";
        test_subtraction();
        
        std::cout << "Running benchmark...\n";
        benchmark_generation_speed();
        
        std::cout << "\n=== All Tests Passed! ===\n";
        std::cout << "X86 CodeGen V2 is working correctly and ready for integration.\n";
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Test failed with unknown exception\n";
        return 1;
    }
}
