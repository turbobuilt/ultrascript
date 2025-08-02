#include "x86_codegen_improved.h"
#include "runtime.h"
#include "console_log_overhaul.h"
#include "runtime_syscalls.h"
#include <cassert>
#include <stdexcept>
#include <iostream>
#include <iomanip>

namespace ultraScript {

// =============================================================================
// Utility Functions with Enhanced Validation
// =============================================================================

static X86Reg map_int_to_x86reg(int reg_id) {
    if (reg_id < 0 || reg_id > 15) {
        throw std::invalid_argument("Invalid register ID: " + std::to_string(reg_id));
    }
    return static_cast<X86Reg>(reg_id);
}

// =============================================================================
// X86CodeGenImproved Implementation
// =============================================================================

X86CodeGenImproved::X86CodeGenImproved() {
    std::vector<uint8_t> code_buffer;  // Local buffer for now
    builder = std::make_unique<X86InstructionBuilder>(code_buffer);
    patterns = std::make_unique<X86PatternBuilder>(*builder);
}

X86Reg X86CodeGenImproved::map_register(int reg_id) const {
    return map_int_to_x86reg(reg_id);
}

void X86CodeGenImproved::validate_register(int reg_id) const {
    if (reg_id < 0 || reg_id > 15) {
        throw std::invalid_argument("Invalid register ID: " + std::to_string(reg_id));
    }
}

void X86CodeGenImproved::validate_memory_operation(int64_t offset) const {
    if (!validate_memory_access) return;
    
    // Ensure offset is within reasonable bounds for stack operations
    if (offset < -32768 || offset > 32767) {
        std::cerr << "Warning: Large stack offset " << offset << " may indicate error" << std::endl;
    }
    
    // Check if this is a registered valid offset
    if (stack_frame.is_established && !stack_frame.is_valid_offset(offset)) {
        std::cerr << "Warning: Accessing unregistered stack offset " << offset << std::endl;
    }
}

bool X86CodeGenImproved::can_use_short_encoding(int64_t value) const {
    return value >= -128 && value <= 127;
}

// =============================================================================
// CodeGenerator Interface Implementation with Error Reduction
// =============================================================================

void X86CodeGenImproved::emit_prologue() {
    if (stack_frame.is_established) {
        return;  // Avoid double prologue
    }
    
    // Use pattern builder for consistent, optimized prologue generation
    std::vector<X86Reg> saved_regs = {X86Reg::RBX, X86Reg::R12, X86Reg::R13, X86Reg::R14, X86Reg::R15};
    patterns->emit_function_prologue(stack_frame.size, saved_regs);
    
    stack_frame.is_established = true;
    
    // Register common stack offsets
    for (int64_t offset = -8; offset >= -static_cast<int64_t>(stack_frame.size); offset -= 8) {
        stack_frame.register_offset(offset);
    }
}

void X86CodeGenImproved::emit_epilogue() {
    if (!stack_frame.is_established) {
        return;  // No prologue to match
    }
    
    std::vector<X86Reg> saved_regs = {X86Reg::RBX, X86Reg::R12, X86Reg::R13, X86Reg::R14, X86Reg::R15};
    patterns->emit_function_epilogue(stack_frame.size, saved_regs);
    
    stack_frame.is_established = false;
}

void X86CodeGenImproved::emit_mov_reg_imm(int reg, int64_t value) {
    validate_register(reg);
    X86Reg target = map_register(reg);
    
    // Optimization: use XOR for zero
    if (value == 0 && enable_peephole) {
        builder->xor_(target, target);
    } else {
        builder->mov(target, ImmediateOperand(value));
    }
    
    reg_tracker.mark_used(reg);
    reg_tracker.mark_clean(reg);  // Clean because we just loaded a known value
}

void X86CodeGenImproved::emit_mov_reg_reg(int dst, int src) {
    validate_register(dst);
    validate_register(src);
    
    // Optimization: eliminate self-moves
    if (dst == src && enable_peephole) {
        return;
    }
    
    X86Reg dst_reg = map_register(dst);
    X86Reg src_reg = map_register(src);
    builder->mov(dst_reg, src_reg);
    
    reg_tracker.mark_used(dst);
    // Propagate dirty state from source to destination
    if (reg_tracker.is_dirty(src)) {
        reg_tracker.mark_dirty(dst);
    } else {
        reg_tracker.mark_clean(dst);
    }
}

void X86CodeGenImproved::emit_mov_mem_reg(int64_t offset, int reg) {
    validate_register(reg);
    validate_memory_operation(offset);
    
    X86Reg src_reg = map_register(reg);
    MemoryOperand dst(X86Reg::RBP, static_cast<int32_t>(offset));
    builder->mov(dst, src_reg);
    
    reg_tracker.mark_used(reg);
}

void X86CodeGenImproved::emit_mov_reg_mem(int reg, int64_t offset) {
    validate_register(reg);
    validate_memory_operation(offset);
    
    X86Reg dst_reg = map_register(reg);
    MemoryOperand src(X86Reg::RBP, static_cast<int32_t>(offset));
    builder->mov(dst_reg, src);
    
    reg_tracker.mark_used(reg);
    reg_tracker.mark_dirty(reg);  // Memory loads are considered dirty
}

// RSP-relative memory operations for stack manipulation
void X86CodeGenImproved::emit_mov_mem_rsp_reg(int64_t offset, int reg) {
    validate_register(reg);
    
    X86Reg src_reg = map_register(reg);
    MemoryOperand dst(X86Reg::RSP, static_cast<int32_t>(offset));
    builder->mov(dst, src_reg);
    
    reg_tracker.mark_used(reg);
}

void X86CodeGenImproved::emit_mov_reg_mem_rsp(int reg, int64_t offset) {
    validate_register(reg);
    
    X86Reg dst_reg = map_register(reg);
    MemoryOperand src(X86Reg::RSP, static_cast<int32_t>(offset));
    builder->mov(dst_reg, src);
    
    reg_tracker.mark_used(reg);
    reg_tracker.mark_dirty(reg);
}

void X86CodeGenImproved::emit_add_reg_imm(int reg, int64_t value) {
    validate_register(reg);
    
    // Optimization: eliminate add 0
    if (value == 0 && enable_peephole) {
        return;
    }
    
    X86Reg target = map_register(reg);
    builder->add(target, ImmediateOperand(value));
    
    reg_tracker.mark_used(reg);
    reg_tracker.mark_dirty(reg);
}

void X86CodeGenImproved::emit_add_reg_reg(int dst, int src) {
    validate_register(dst);
    validate_register(src);
    
    X86Reg dst_reg = map_register(dst);
    X86Reg src_reg = map_register(src);
    builder->add(dst_reg, src_reg);
    
    reg_tracker.mark_used(dst);
    reg_tracker.mark_used(src);
    reg_tracker.mark_dirty(dst);
}

void X86CodeGenImproved::emit_sub_reg_imm(int reg, int64_t value) {
    validate_register(reg);
    
    // Optimization: eliminate sub 0
    if (value == 0 && enable_peephole) {
        return;
    }
    
    X86Reg target = map_register(reg);
    builder->sub(target, ImmediateOperand(value));
    
    reg_tracker.mark_used(reg);
    reg_tracker.mark_dirty(reg);
}

void X86CodeGenImproved::emit_sub_reg_reg(int dst, int src) {
    validate_register(dst);
    validate_register(src);
    
    X86Reg dst_reg = map_register(dst);
    X86Reg src_reg = map_register(src);
    builder->sub(dst_reg, src_reg);
    
    reg_tracker.mark_used(dst);
    reg_tracker.mark_used(src);
    reg_tracker.mark_dirty(dst);
}

void X86CodeGenImproved::emit_mul_reg_reg(int dst, int src) {
    validate_register(dst);
    validate_register(src);
    
    X86Reg dst_reg = map_register(dst);
    X86Reg src_reg = map_register(src);
    builder->imul(dst_reg, src_reg);
    
    reg_tracker.mark_used(dst);
    reg_tracker.mark_used(src);
    reg_tracker.mark_dirty(dst);
}

void X86CodeGenImproved::emit_div_reg_reg(int dst, int src) {
    validate_register(dst);
    validate_register(src);
    
    X86Reg dst_reg = map_register(dst);
    X86Reg src_reg = map_register(src);
    
    // Set up for division: ensure dividend is in RAX
    if (dst_reg != X86Reg::RAX) {
        builder->mov(X86Reg::RAX, dst_reg);
    }
    builder->cqo();  // Sign extend RAX into RDX:RAX
    builder->idiv(src_reg);
    
    // Move quotient back if needed
    if (dst_reg != X86Reg::RAX) {
        builder->mov(dst_reg, X86Reg::RAX);
    }
    
    reg_tracker.mark_used(dst);
    reg_tracker.mark_used(src);
    reg_tracker.mark_dirty(dst);
}

void X86CodeGenImproved::emit_mod_reg_reg(int dst, int src) {
    validate_register(dst);
    validate_register(src);
    
    X86Reg dst_reg = map_register(dst);
    X86Reg src_reg = map_register(src);
    
    // Set up for division: ensure dividend is in RAX
    if (dst_reg != X86Reg::RAX) {
        builder->mov(X86Reg::RAX, dst_reg);
    }
    builder->cqo();  // Sign extend RAX into RDX:RAX
    builder->idiv(src_reg);
    
    // Move remainder (in RDX) to destination
    if (dst_reg != X86Reg::RDX) {
        builder->mov(dst_reg, X86Reg::RDX);
    }
    
    reg_tracker.mark_used(dst);
    reg_tracker.mark_used(src);
    reg_tracker.mark_dirty(dst);
}

void X86CodeGenImproved::emit_call(const std::string& label) {
    // Try to resolve to direct function pointer for maximum performance
    static const std::unordered_map<std::string, void*> runtime_functions = {
        {"__console_log_float64", reinterpret_cast<void*>(__console_log_float64)},
        {"__console_log_final_newline", reinterpret_cast<void*>(__console_log_final_newline)},
        {"__console_log_any_value_inspect", reinterpret_cast<void*>(__console_log_any_value_inspect)},
        {"__dynamic_value_create_from_double", reinterpret_cast<void*>(__dynamic_value_create_from_double)},
        {"__runtime_time_now_millis", reinterpret_cast<void*>(__runtime_time_now_millis)},
        // Add more runtime functions as needed
    };
    
    auto it = runtime_functions.find(label);
    if (it != runtime_functions.end()) {
        builder->call(it->second);
    } else {
        builder->call(label);
    }
    
    // Mark caller-saved registers as dirty
    for (int reg : {0, 1, 2, 6, 7, 8, 9, 10, 11}) {  // RAX, RCX, RDX, RSI, RDI, R8-R11
        reg_tracker.mark_dirty(reg);
    }
}

void X86CodeGenImproved::emit_ret() {
    builder->ret();
}

void X86CodeGenImproved::emit_function_return() {
    emit_epilogue();  // Epilogue includes ret
}

void X86CodeGenImproved::emit_jump(const std::string& label) {
    builder->jmp(label);
}

void X86CodeGenImproved::emit_jump_if_zero(const std::string& label) {
    builder->jz(label);
}

void X86CodeGenImproved::emit_jump_if_not_zero(const std::string& label) {
    builder->jnz(label);
}

void X86CodeGenImproved::emit_compare(int reg1, int reg2) {
    validate_register(reg1);
    validate_register(reg2);
    
    X86Reg left = map_register(reg1);
    X86Reg right = map_register(reg2);
    builder->cmp(left, right);
    
    reg_tracker.mark_used(reg1);
    reg_tracker.mark_used(reg2);
}

void X86CodeGenImproved::emit_setl(int reg) {
    validate_register(reg);
    X86Reg target = map_register(reg);
    builder->setl(target);
    reg_tracker.mark_used(reg);
    reg_tracker.mark_dirty(reg);
}

void X86CodeGenImproved::emit_setg(int reg) {
    validate_register(reg);
    X86Reg target = map_register(reg);
    builder->setg(target);
    reg_tracker.mark_used(reg);
    reg_tracker.mark_dirty(reg);
}

void X86CodeGenImproved::emit_sete(int reg) {
    validate_register(reg);
    X86Reg target = map_register(reg);
    builder->setz(target);
    reg_tracker.mark_used(reg);
    reg_tracker.mark_dirty(reg);
}

void X86CodeGenImproved::emit_setne(int reg) {
    validate_register(reg);
    X86Reg target = map_register(reg);
    builder->setnz(target);
    reg_tracker.mark_used(reg);
    reg_tracker.mark_dirty(reg);
}

void X86CodeGenImproved::emit_setle(int reg) {
    validate_register(reg);
    X86Reg target = map_register(reg);
    builder->setle(target);
    reg_tracker.mark_used(reg);
    reg_tracker.mark_dirty(reg);
}

void X86CodeGenImproved::emit_setge(int reg) {
    validate_register(reg);
    X86Reg target = map_register(reg);
    builder->setge(target);
    reg_tracker.mark_used(reg);
    reg_tracker.mark_dirty(reg);
}

void X86CodeGenImproved::emit_and_reg_imm(int reg, int64_t value) {
    validate_register(reg);
    X86Reg target = map_register(reg);
    builder->and_(target, ImmediateOperand(value));
    reg_tracker.mark_used(reg);
    reg_tracker.mark_dirty(reg);
}

void X86CodeGenImproved::emit_xor_reg_reg(int dst, int src) {
    validate_register(dst);
    validate_register(src);
    
    X86Reg dst_reg = map_register(dst);
    X86Reg src_reg = map_register(src);
    
    // Optimization: XOR with self is zero
    if (dst == src && enable_peephole) {
        reg_tracker.mark_clean(dst);  // XOR reg, reg results in clean zero
    } else {
        reg_tracker.mark_dirty(dst);
    }
    
    builder->xor_(dst_reg, src_reg);
    reg_tracker.mark_used(dst);
    reg_tracker.mark_used(src);
}

void X86CodeGenImproved::emit_call_reg(int reg) {
    validate_register(reg);
    X86Reg target = map_register(reg);
    builder->call(target);
    
    // Mark caller-saved registers as dirty
    for (int r : {0, 1, 2, 6, 7, 8, 9, 10, 11}) {
        reg_tracker.mark_dirty(r);
    }
}

void X86CodeGenImproved::emit_label(const std::string& label) {
    size_t pos = builder->get_current_position();
    builder->resolve_label(label, pos);
}

// =============================================================================
// Stub implementations for remaining interface methods
// =============================================================================

void X86CodeGenImproved::emit_goroutine_spawn(const std::string& function_name) {
    patterns->setup_function_call({});
    builder->call("__goroutine_spawn_" + function_name);
    patterns->cleanup_function_call(0);
}

void X86CodeGenImproved::emit_goroutine_spawn_with_args(const std::string& function_name, int arg_count) {
    // Setup registers for arguments
    std::vector<X86Reg> args;
    const X86Reg arg_regs[] = {X86Reg::RDI, X86Reg::RSI, X86Reg::RDX, X86Reg::RCX, X86Reg::R8, X86Reg::R9};
    for (int i = 0; i < std::min(arg_count, 6); i++) {
        args.push_back(arg_regs[i]);
    }
    
    patterns->setup_function_call(args);
    builder->call("__goroutine_spawn_with_args_" + function_name);
    patterns->cleanup_function_call(arg_count > 6 ? (arg_count - 6) * 8 : 0);
}

void X86CodeGenImproved::emit_goroutine_spawn_with_func_ptr() {
    builder->call("__goroutine_spawn_func_ptr");
}

void X86CodeGenImproved::emit_goroutine_spawn_with_func_id() {
    builder->call("__goroutine_spawn_func_id");
}

void X86CodeGenImproved::emit_goroutine_spawn_with_address(void* function_address) {
    builder->mov(X86Reg::RDI, ImmediateOperand(reinterpret_cast<int64_t>(function_address)));
    builder->call("__goroutine_spawn_func_ptr");
}

void X86CodeGenImproved::emit_promise_resolve(int value_reg) {
    validate_register(value_reg);
    X86Reg value = map_register(value_reg);
    builder->mov(X86Reg::RDI, value);
    builder->call("__promise_resolve");
}

void X86CodeGenImproved::emit_promise_await(int promise_reg) {
    validate_register(promise_reg);
    X86Reg promise = map_register(promise_reg);
    builder->mov(X86Reg::RDI, promise);
    builder->call("__promise_await");
}

void X86CodeGenImproved::emit_call_fast(uint16_t func_id) {
    builder->mov(X86Reg::RDI, ImmediateOperand(func_id));
    builder->call("__call_fast_by_id");
}

void X86CodeGenImproved::emit_goroutine_spawn_fast(uint16_t func_id) {
    builder->mov(X86Reg::RDI, ImmediateOperand(func_id));
    builder->call("__goroutine_spawn_fast_by_id");
}

void X86CodeGenImproved::emit_goroutine_spawn_direct(void* function_address) {
    builder->mov(X86Reg::RDI, ImmediateOperand(reinterpret_cast<int64_t>(function_address)));
    builder->call("__goroutine_spawn_direct");
}

// Lock operations
void X86CodeGenImproved::emit_lock_acquire(int lock_reg) {
    validate_register(lock_reg);
    X86Reg lock_ptr = map_register(lock_reg);
    builder->mov(X86Reg::RDI, lock_ptr);
    builder->call("__lock_acquire");
}

void X86CodeGenImproved::emit_lock_release(int lock_reg) {
    validate_register(lock_reg);
    X86Reg lock_ptr = map_register(lock_reg);
    builder->mov(X86Reg::RDI, lock_ptr);
    builder->call("__lock_release");
}

void X86CodeGenImproved::emit_lock_try_acquire(int lock_reg, int result_reg) {
    validate_register(lock_reg);
    validate_register(result_reg);
    X86Reg lock_ptr = map_register(lock_reg);
    X86Reg result = map_register(result_reg);
    builder->mov(X86Reg::RDI, lock_ptr);
    builder->call("__lock_try_acquire");
    if (result != X86Reg::RAX) {
        builder->mov(result, X86Reg::RAX);
    }
}

void X86CodeGenImproved::emit_lock_try_acquire_timeout(int lock_reg, int timeout_reg, int result_reg) {
    validate_register(lock_reg);
    validate_register(timeout_reg);
    validate_register(result_reg);
    
    X86Reg lock_ptr = map_register(lock_reg);
    X86Reg timeout = map_register(timeout_reg);
    X86Reg result = map_register(result_reg);
    
    builder->mov(X86Reg::RDI, lock_ptr);
    builder->mov(X86Reg::RSI, timeout);
    builder->call("__lock_try_acquire_timeout");
    if (result != X86Reg::RAX) {
        builder->mov(result, X86Reg::RAX);
    }
}

// Atomic operations stubs
void X86CodeGenImproved::emit_atomic_compare_exchange(int ptr_reg, int expected_reg, int desired_reg, int result_reg) {
    // Implement atomic compare-exchange
    (void)ptr_reg; (void)expected_reg; (void)desired_reg; (void)result_reg;
}

void X86CodeGenImproved::emit_atomic_fetch_add(int ptr_reg, int value_reg, int result_reg) {
    // Implement atomic fetch-add
    (void)ptr_reg; (void)value_reg; (void)result_reg;
}

void X86CodeGenImproved::emit_atomic_store(int ptr_reg, int value_reg, int memory_order) {
    // Implement atomic store
    (void)ptr_reg; (void)value_reg; (void)memory_order;
}

void X86CodeGenImproved::emit_atomic_load(int ptr_reg, int result_reg, int memory_order) {
    // Implement atomic load
    (void)ptr_reg; (void)result_reg; (void)memory_order;
}

void X86CodeGenImproved::emit_memory_fence(int fence_type) {
    switch (fence_type) {
        case 0: builder->lfence(); break;  // Load fence
        case 1: builder->sfence(); break;  // Store fence
        default: builder->mfence(); break; // Full fence
    }
}

// CodeGenerator interface methods
std::vector<uint8_t> X86CodeGenImproved::get_code() const {
    // Need to get code from builder - this will require modification to builder interface
    return std::vector<uint8_t>();  // Placeholder
}

void X86CodeGenImproved::clear() {
    // Clear all state
    stack_frame = StackFrame{};
    reg_tracker = RegisterTracker{};
}

size_t X86CodeGenImproved::get_current_offset() const {
    return builder->get_current_position();
}

const std::unordered_map<std::string, int64_t>& X86CodeGenImproved::get_label_offsets() const {
    static std::unordered_map<std::string, int64_t> empty_map;
    return empty_map;  // Placeholder - need to implement in builder
}

void X86CodeGenImproved::set_function_stack_size(int64_t size) {
    stack_frame.size = static_cast<size_t>(size);
}

int64_t X86CodeGenImproved::get_function_stack_size() const {
    return static_cast<int64_t>(stack_frame.size);
}

void X86CodeGenImproved::resolve_runtime_function_calls() {
    // Runtime function resolution is handled in emit_call()
}

// Performance and debugging
size_t X86CodeGenImproved::get_instruction_count() const {
    return builder->get_current_position();  // Approximate
}

void X86CodeGenImproved::print_assembly_debug() const {
    std::cout << "Assembly debug information:" << std::endl;
    std::cout << "Stack frame size: " << stack_frame.size << std::endl;
    std::cout << "Valid offsets: ";
    for (int64_t offset : stack_frame.valid_offsets) {
        std::cout << offset << " ";
    }
    std::cout << std::endl;
}

// Advanced patterns
void X86CodeGenImproved::emit_bounds_check(int index_reg, int limit_reg) {
    validate_register(index_reg);
    validate_register(limit_reg);
    patterns->emit_bounds_check(map_register(index_reg), map_register(limit_reg), "__bounds_error");
}

void X86CodeGenImproved::emit_null_check(int pointer_reg) {
    validate_register(pointer_reg);
    patterns->emit_null_check(map_register(pointer_reg), "__null_error");
}

void X86CodeGenImproved::emit_type_check(int value_reg, int expected_type) {
    validate_register(value_reg);
    // Load type tag and compare
    X86Reg value = map_register(value_reg);
    builder->cmp(value, ImmediateOperand(expected_type));
    // Jump to error handler if not equal
    builder->jnz("__type_error");
}

void X86CodeGenImproved::emit_string_equals_optimized(int str1_reg, int str2_reg, int result_reg) {
    validate_register(str1_reg);
    validate_register(str2_reg);
    validate_register(result_reg);
    patterns->emit_string_comparison(map_register(str1_reg), map_register(str2_reg), map_register(result_reg));
}

void X86CodeGenImproved::emit_array_access_optimized(int array_reg, int index_reg, int result_reg) {
    validate_register(array_reg);
    validate_register(index_reg);
    validate_register(result_reg);
    patterns->emit_typed_array_access(map_register(array_reg), map_register(index_reg), map_register(result_reg), OpSize::QWORD);
}

// Factory function
std::unique_ptr<CodeGenerator> create_improved_x86_codegen() {
    return std::make_unique<X86CodeGenImproved>();
}

}  // namespace ultraScript
