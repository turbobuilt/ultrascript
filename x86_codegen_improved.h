#pragma once

#include "codegen_forward.h"
#include "x86_instruction_builder.h"
#include <memory>
#include <unordered_set>



// Enhanced X86 code generator with error reduction and performance optimizations
class X86CodeGenImproved : public CodeGenerator {
private:
    std::unique_ptr<X86InstructionBuilder> builder;
    std::unique_ptr<X86PatternBuilder> patterns;
    
    // Stack-relative memory operations with validation
    struct StackFrame {
        size_t size = 0;
        size_t current_offset = 0;
        std::unordered_set<int64_t> valid_offsets;
        bool is_established = false;
        
        bool is_valid_offset(int64_t offset) const {
            return valid_offsets.find(offset) != valid_offsets.end();
        }
        
        void register_offset(int64_t offset) {
            valid_offsets.insert(offset);
        }
    } stack_frame;
    
    // Register usage tracking for optimization
    struct RegisterTracker {
        std::bitset<16> in_use;
        std::bitset<16> dirty;
        
        void mark_used(int reg) { in_use.set(reg); }
        void mark_dirty(int reg) { dirty.set(reg); }
        void mark_clean(int reg) { dirty.reset(reg); }
        bool is_dirty(int reg) const { return dirty.test(reg); }
    } reg_tracker;
    
    // Performance optimizations
    bool enable_peephole = true;
    bool enable_dead_code_elimination = true;
    bool validate_memory_access = true;
    
    // Helper methods
    X86Reg map_register(int reg_id) const;
    void validate_register(int reg_id) const;
    void validate_memory_operation(int64_t offset) const;
    bool can_use_short_encoding(int64_t value) const;
    
public:
    X86CodeGenImproved();
    ~X86CodeGenImproved() override = default;
    
    // Enhanced CodeGenerator interface with error checking
    void emit_prologue() override;
    void emit_epilogue() override;
    
    // Memory operations with proper RSP-relative addressing
    void emit_mov_reg_imm(int reg, int64_t value) override;
    void emit_mov_reg_reg(int dst, int src) override;
    void emit_mov_mem_reg(int64_t offset, int reg) override;  // [rbp+offset] = reg
    void emit_mov_reg_mem(int reg, int64_t offset) override;  // reg = [rbp+offset]
    
    // Enhanced memory operations for stack management
    void emit_mov_mem_rsp_reg(int64_t offset, int reg);      // [rsp+offset] = reg
    void emit_mov_reg_mem_rsp(int reg, int64_t offset);      // reg = [rsp+offset]
    
    // Arithmetic operations with overflow checking
    void emit_add_reg_imm(int reg, int64_t value) override;
    void emit_add_reg_reg(int dst, int src) override;
    void emit_sub_reg_imm(int reg, int64_t value) override;
    void emit_sub_reg_reg(int dst, int src) override;
    void emit_mul_reg_reg(int dst, int src) override;
    void emit_div_reg_reg(int dst, int src) override;
    void emit_mod_reg_reg(int dst, int src) override;
    
    // Control flow operations
    void emit_call(const std::string& label) override;
    void emit_ret() override;
    void emit_function_return() override;
    void emit_jump(const std::string& label) override;
    void emit_jump_if_zero(const std::string& label) override;
    void emit_jump_if_not_zero(const std::string& label) override;
    
    // Comparison operations
    void emit_compare(int reg1, int reg2) override;
    void emit_setl(int reg) override;
    void emit_setg(int reg) override;
    void emit_sete(int reg) override;
    void emit_setne(int reg) override;
    void emit_setle(int reg) override;
    void emit_setge(int reg) override;
    
    // Logical operations
    void emit_and_reg_imm(int reg, int64_t value) override;
    void emit_xor_reg_reg(int dst, int src) override;
    void emit_call_reg(int reg) override;
    
    // Label management
    void emit_label(const std::string& label) override;
    
    // Goroutine operations (high-performance)
    void emit_goroutine_spawn(const std::string& function_name) override;
    void emit_goroutine_spawn_with_args(const std::string& function_name, int arg_count) override;
    void emit_goroutine_spawn_with_func_ptr() override;
    void emit_goroutine_spawn_with_func_id() override;
    void emit_goroutine_spawn_with_address(void* function_address) override;
    void emit_promise_resolve(int value_reg) override;
    void emit_promise_await(int promise_reg) override;
    
    // High-performance function calls
    void emit_call_fast(uint16_t func_id) override;
    void emit_goroutine_spawn_fast(uint16_t func_id) override;
    void emit_goroutine_spawn_direct(void* function_address) override;
    
    // Lock operations
    void emit_lock_acquire(int lock_reg) override;
    void emit_lock_release(int lock_reg) override;
    void emit_lock_try_acquire(int lock_reg, int result_reg) override;
    void emit_lock_try_acquire_timeout(int lock_reg, int timeout_reg, int result_reg) override;
    
    // Atomic operations
    void emit_atomic_compare_exchange(int ptr_reg, int expected_reg, int desired_reg, int result_reg) override;
    void emit_atomic_fetch_add(int ptr_reg, int value_reg, int result_reg) override;
    void emit_atomic_store(int ptr_reg, int value_reg, int memory_order) override;
    void emit_atomic_load(int ptr_reg, int result_reg, int memory_order) override;
    void emit_memory_fence(int fence_type) override;
    
    // CodeGenerator interface implementation
    std::vector<uint8_t> get_code() const override;
    void clear() override;
    size_t get_current_offset() const override;
    const std::unordered_map<std::string, int64_t>& get_label_offsets() const override;
    void set_function_stack_size(int64_t size) override;
    int64_t get_function_stack_size() const override;
    void resolve_runtime_function_calls() override;
    
    // Performance and debugging features
    void enable_optimization(bool enable) { enable_peephole = enable; }
    void enable_validation(bool enable) { validate_memory_access = enable; }
    size_t get_instruction_count() const;
    void print_assembly_debug() const;
    
    // Advanced code patterns
    void emit_bounds_check(int index_reg, int limit_reg);
    void emit_null_check(int pointer_reg);
    void emit_type_check(int value_reg, int expected_type);
    void emit_string_equals_optimized(int str1_reg, int str2_reg, int result_reg);
    void emit_array_access_optimized(int array_reg, int index_reg, int result_reg);
};

// Factory function
std::unique_ptr<CodeGenerator> create_improved_x86_codegen();


