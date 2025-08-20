#pragma once

#include <memory>
#include <vector>
#include <unordered_map>
#include <string>
#include <cstdint>


// Forward declarations to break circular dependencies
class CodeGenerator {
public:
    virtual ~CodeGenerator() = default;
    virtual void emit_prologue() = 0;
    virtual void emit_epilogue() = 0;
    virtual void emit_mov_reg_imm(int reg, int64_t value) = 0;
    virtual void emit_mov_reg_reg(int dst, int src) = 0;
    virtual void emit_mov_mem_reg(int64_t offset, int reg) = 0;  // [rbp+offset] = reg
    virtual void emit_mov_reg_mem(int reg, int64_t offset) = 0;  // reg = [rbp+offset]
    
    // Register-relative memory operations for direct object property access
    virtual void emit_mov_reg_reg_offset(int dst_reg, int src_reg, int64_t offset) = 0;  // dst = [src+offset]
    virtual void emit_mov_reg_offset_reg(int dst_reg, int64_t offset, int src_reg) = 0;  // [dst+offset] = src
    
    // RSP-relative memory operations for stack manipulation
    virtual void emit_mov_mem_rsp_reg(int64_t offset, int reg) = 0;  // [rsp+offset] = reg  
    virtual void emit_mov_reg_mem_rsp(int reg, int64_t offset) = 0;  // reg = [rsp+offset]
    virtual void emit_add_reg_imm(int reg, int64_t value) = 0;
    virtual void emit_add_reg_reg(int dst, int src) = 0;
    virtual void emit_sub_reg_imm(int reg, int64_t value) = 0;
    virtual void emit_sub_reg_reg(int dst, int src) = 0;
    virtual void emit_mul_reg_reg(int dst, int src) = 0;
    virtual void emit_div_reg_reg(int dst, int src) = 0;
    virtual void emit_mod_reg_reg(int dst, int src) = 0;
    virtual void emit_call(const std::string& label) = 0;
    virtual void emit_ret() = 0;
    virtual void emit_function_return() = 0;
    virtual void emit_jump(const std::string& label) = 0;
    virtual void emit_jump_if_zero(const std::string& label) = 0;
    virtual void emit_jump_if_not_zero(const std::string& label) = 0;
    virtual void emit_jump_if_greater_equal(const std::string& label) = 0;
    virtual void emit_compare(int reg1, int reg2) = 0;
    virtual void emit_setl(int reg) = 0;
    virtual void emit_setg(int reg) = 0;
    virtual void emit_sete(int reg) = 0;
    virtual void emit_setne(int reg) = 0;
    virtual void emit_setle(int reg) = 0;
    virtual void emit_setge(int reg) = 0;
    virtual void emit_and_reg_imm(int reg, int64_t value) = 0;
    virtual void emit_xor_reg_reg(int dst, int src) = 0;
    virtual void emit_call_reg(int reg) = 0;
    virtual void emit_label(const std::string& label) = 0;
    virtual void emit_goroutine_spawn(const std::string& function_name) = 0;
    virtual void emit_goroutine_spawn_with_args(const std::string& function_name, int arg_count) = 0;
    virtual void emit_goroutine_spawn_with_func_ptr() = 0;
    virtual void emit_goroutine_spawn_with_func_id() = 0;
    virtual void emit_goroutine_spawn_with_address(void* function_address) = 0;
    virtual void emit_promise_resolve(int value_reg) = 0;
    virtual void emit_promise_await(int promise_reg) = 0;
    
    // High-Performance Function Calls
    virtual void emit_call_fast(uint16_t func_id) = 0;
    virtual void emit_goroutine_spawn_fast(uint16_t func_id) = 0;
    virtual void emit_goroutine_spawn_direct(void* function_address) = 0;
    
    // Lock operations
    virtual void emit_lock_acquire(int lock_reg) = 0;
    virtual void emit_lock_release(int lock_reg) = 0;
    virtual void emit_lock_try_acquire(int lock_reg, int result_reg) = 0;
    virtual void emit_lock_try_acquire_timeout(int lock_reg, int timeout_reg, int result_reg) = 0;
    
    // Atomic operations
    virtual void emit_atomic_compare_exchange(int ptr_reg, int expected_reg, int desired_reg, int result_reg) = 0;
    virtual void emit_atomic_fetch_add(int ptr_reg, int value_reg, int result_reg) = 0;
    virtual void emit_atomic_store(int ptr_reg, int value_reg, int memory_order) = 0;
    virtual void emit_atomic_load(int ptr_reg, int result_reg, int memory_order) = 0;
    virtual void emit_memory_fence(int fence_type) = 0;
    
    virtual std::vector<uint8_t> get_code() const = 0;
    virtual void clear() = 0;
    virtual size_t get_current_offset() const = 0;
    virtual const std::unordered_map<std::string, int64_t>& get_label_offsets() const = 0;
    
    // Stack management for function frames
    virtual void set_function_stack_size(int64_t size) = 0;
    virtual int64_t get_function_stack_size() const = 0;
    
    // Runtime function call resolution
    virtual void resolve_runtime_function_calls() = 0;
    
    // Get offset for a specific label
    virtual int64_t get_label_offset(const std::string& label) const {
        const auto& offsets = get_label_offsets();
        auto it = offsets.find(label);
        return (it != offsets.end()) ? it->second : -1;
    }
};

// Factory function for creating X86 code generator
std::unique_ptr<CodeGenerator> create_x86_codegen();

