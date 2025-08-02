#pragma once

#include "x86_instruction_builder.h"
#include "codegen_forward.h"
#include <memory>

namespace ultraScript {

// New high-performance X86 code generator using instruction builder abstraction
class X86CodeGenV2 : public CodeGenerator {
private:
    std::vector<uint8_t> code_buffer;
    std::unique_ptr<X86InstructionBuilder> instruction_builder;
    std::unique_ptr<X86PatternBuilder> pattern_builder;
    
    // Register allocation state
    struct RegisterState {
        bool is_free[16];
        X86Reg last_allocated;
        
        RegisterState() : last_allocated(X86Reg::RAX) {
            std::fill(is_free, is_free + 16, true);
            // Mark stack pointer and base pointer as not free
            is_free[static_cast<int>(X86Reg::RSP)] = false;
            is_free[static_cast<int>(X86Reg::RBP)] = false;
        }
    } reg_state;
    
    // Stack frame management
    struct StackFrame {
        size_t local_stack_size = 0;
        std::vector<X86Reg> saved_registers;
        size_t current_offset = 0;
        bool frame_established = false;
    } stack_frame;
    
    // Label management
    std::unordered_map<std::string, int64_t> label_offsets;
    std::vector<std::pair<std::string, size_t>> unresolved_jumps;
    
    // Helper methods for register management
    X86Reg allocate_register();
    void free_register(X86Reg reg);
    X86Reg get_register_for_int(int reg_id);
    
    // Runtime function resolution helper
    void* get_runtime_function_address(const std::string& function_name);
    
    // Optimization helpers
    void optimize_mov_sequences();
    void eliminate_dead_code();
    
public:
    X86CodeGenV2();
    ~X86CodeGenV2() override = default;
    
    // Performance optimization settings
    bool enable_peephole_optimization = true;
    bool enable_register_allocation = true;
    
    // CodeGenerator interface implementation
    void emit_prologue() override;
    void emit_epilogue() override;
    void emit_mov_reg_imm(int reg, int64_t value) override;
    void emit_mov_reg_reg(int dst, int src) override;
    void emit_mov_mem_reg(int64_t offset, int reg) override;
    void emit_mov_reg_mem(int reg, int64_t offset) override;
    
    // Register-relative memory operations for direct object property access
    void emit_mov_reg_reg_offset(int dst_reg, int src_reg, int64_t offset) override;  // dst = [src+offset]
    void emit_mov_reg_offset_reg(int dst_reg, int64_t offset, int src_reg) override;  // [dst+offset] = src
    
    // RSP-relative memory operations for stack manipulation
    void emit_mov_mem_rsp_reg(int64_t offset, int reg) override;
    void emit_mov_reg_mem_rsp(int reg, int64_t offset) override;
    void emit_add_reg_imm(int reg, int64_t value) override;
    void emit_add_reg_reg(int dst, int src) override;
    void emit_sub_reg_imm(int reg, int64_t value) override;
    void emit_sub_reg_reg(int dst, int src) override;
    void emit_mul_reg_reg(int dst, int src) override;
    void emit_div_reg_reg(int dst, int src) override;
    void emit_mod_reg_reg(int dst, int src) override;
    void emit_call(const std::string& label) override;
    void emit_ret() override;
    void emit_function_return() override;
    void emit_jump(const std::string& label) override;
    void emit_jump_if_zero(const std::string& label) override;
    void emit_jump_if_not_zero(const std::string& label) override;
    void emit_compare(int reg1, int reg2) override;
    void emit_setl(int reg) override;
    void emit_setg(int reg) override;
    void emit_sete(int reg) override;
    void emit_setne(int reg) override;
    void emit_setle(int reg) override;
    void emit_setge(int reg) override;
    void emit_and_reg_imm(int reg, int64_t value) override;
    void emit_xor_reg_reg(int dst, int src) override;
    void emit_call_reg(int reg) override;
    void emit_label(const std::string& label) override;
    void emit_goroutine_spawn(const std::string& function_name) override;
    void emit_goroutine_spawn_with_args(const std::string& function_name, int arg_count) override;
    void emit_goroutine_spawn_with_func_ptr() override;
    void emit_goroutine_spawn_with_func_id() override;
    void emit_goroutine_spawn_with_address(void* function_address) override;
    void emit_promise_resolve(int value_reg) override;
    void emit_promise_await(int promise_reg) override;
    
    // High-Performance Function Calls
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
    
    // CodeGenerator interface getters
    std::vector<uint8_t> get_code() const override { return code_buffer; }
    void clear() override;
    
    // Validation for robust code generation
    bool validate_code_generation() const;  // Validate all labels resolved and code is ready
    
    // High-Performance Floating-Point Operations
    // These provide direct XMM register access for maximum performance
    void emit_movq_xmm_gpr(int xmm_reg, int gpr_reg);  // Move 64-bit from GPR to XMM
    void emit_movq_gpr_xmm(int gpr_reg, int xmm_reg);  // Move 64-bit from XMM to GPR  
    void emit_movsd_xmm_xmm(int dst_xmm, int src_xmm);  // Move scalar double between XMM
    void emit_cvtsi2sd(int xmm_reg, int gpr_reg);  // Convert signed integer to double
    void emit_cvtsd2si(int gpr_reg, int xmm_reg);  // Convert double to signed integer
    
    // High-performance floating-point function calls with proper calling convention
    void emit_call_with_double_arg(const std::string& function_name, int value_gpr_reg);
    void emit_call_with_xmm_arg(const std::string& function_name, int xmm_reg);
    size_t get_current_offset() const override { return code_buffer.size(); }
    const std::unordered_map<std::string, int64_t>& get_label_offsets() const override;
    
    // New high-level APIs for better code generation
    void emit_function_call(const std::string& function_name, const std::vector<int>& args);
    void emit_typed_array_access(int array_reg, int index_reg, int result_reg, OpSize element_size);
    void emit_string_operation(const std::string& operation, int str1_reg, int str2_reg, int result_reg);
    void emit_bounds_check(int index_reg, int limit_reg);
    void emit_null_check(int pointer_reg);
    
    // Performance monitoring and debugging
    void enable_optimization(bool enable) { enable_peephole_optimization = enable; }
    void enable_register_optimization(bool enable) { enable_register_allocation = enable; }
    size_t get_instruction_count() const;
    void print_assembly_debug() const;
    
    // Advanced code generation patterns
    void emit_loop_optimized(int counter_reg, const std::string& body_label);
    void emit_conditional_move(int condition_reg, int true_val_reg, int false_val_reg, int dest_reg);
    void emit_switch_table(int selector_reg, const std::vector<std::string>& case_labels);
    
    // Memory management helpers
    void set_stack_frame_size(size_t size) { stack_frame.local_stack_size = size; }
    
    // Stack management for function frames (required by base interface)
    void set_function_stack_size(int64_t size) override { stack_frame.local_stack_size = size; }
    int64_t get_function_stack_size() const override { return stack_frame.local_stack_size; }
    
    // Runtime function call resolution (required by base interface)
    void resolve_runtime_function_calls() override;
    void add_saved_register(X86Reg reg) { stack_frame.saved_registers.push_back(reg); }
    
    // Direct access to builders for advanced usage
    X86InstructionBuilder& get_instruction_builder() { return *instruction_builder; }
    X86PatternBuilder& get_pattern_builder() { return *pattern_builder; }
};

// Factory function for creating optimized code generators
std::unique_ptr<CodeGenerator> create_optimized_x86_codegen();

// Performance testing and validation
class X86CodeGenTester {
public:
    static bool validate_instruction_encoding(const std::vector<uint8_t>& code);
    static void benchmark_code_generation_speed();
};

}  // namespace ultraScript
