#include <string>

// Utility: generate a unique label for codegen
static std::string generate_unique_label(const std::string& base) {
    static int label_counter = 0;
    return base + "_" + std::to_string(label_counter++);
}
#include "x86_codegen_v2.h"
#include "runtime.h"  // For runtime function declarations
#include "console_log_overhaul.h"  // For console.log runtime functions
#include "runtime_syscalls.h"  // For runtime syscalls
#include "free_runtime.h"  // For free runtime functions
#include "dynamic_properties.h"  // For dynamic property functions
#include <cassert>
#include <iostream>
#include <cstdlib>  // For malloc
#include <iomanip>
#include <cstdio>
#include <cstring>  // For strlen
#include <execinfo.h>

// Forward declarations for goroutine V2 functions
extern "C" {
    int64_t __gots_set_timeout_v2(void* function_address, int64_t delay_ms);
    int64_t __gots_set_interval_v2(void* function_address, int64_t delay_ms);
    bool __gots_clear_timeout_v2(int64_t timer_id);
    bool __gots_clear_interval_v2(int64_t timer_id);
    int64_t __gots_add_async_handle_v2(int64_t type, void* handle_data);
    void __gots_complete_async_handle_v2(int64_t async_id);
    void __gots_cancel_async_handle_v2(int64_t async_id);
    void __runtime_spawn_main_goroutine_v2(void* function_address);
    void __runtime_wait_for_main_goroutine_v2();
    void* __runtime_spawn_goroutine_v2(void* function_address);
    void* execute_ffi_call(void* current_goroutine, void* ffi_function, void* args);
    void* migrate_to_ffi_thread(void* goroutine, void* ffi_func, void* args);
    bool is_goroutine_ffi_bound(void* goroutine);
}

// Debug helper to convert X86Reg to string
static const char* register_name(X86Reg reg) {
    switch (reg) {
        case X86Reg::RAX: return "rax";
        case X86Reg::RCX: return "rcx";
        case X86Reg::RDX: return "rdx";
        case X86Reg::RBX: return "rbx";
        case X86Reg::RSP: return "rsp";
        case X86Reg::RBP: return "rbp";
        case X86Reg::RSI: return "rsi";
        case X86Reg::RDI: return "rdi";
        case X86Reg::R8: return "r8";
        case X86Reg::R9: return "r9";
        case X86Reg::R10: return "r10";
        case X86Reg::R11: return "r11";
        case X86Reg::R12: return "r12";
        case X86Reg::R13: return "r13";
        case X86Reg::R14: return "r14";
        case X86Reg::R15: return "r15";
        default: return "unknown";
    }
}  // For backtrace
#include <unistd.h>    // For STDOUT_FILENO

// Forward declarations for JIT object system functions
extern "C" {
void* __jit_object_create(void* class_name_ptr);
void* __jit_object_create_sized(void* class_name_ptr, size_t size);
int64_t __class_property_lookup(void* object, void* property_name_string, void* class_info_ptr);
bool __string_equals(void* str1_ptr, void* str2_ptr);
int64_t __string_compare(void* str1_ptr, void* str2_ptr);
void* __dynamic_value_extract_string(void* dynamic_value_ptr);
int64_t __dynamic_value_extract_int64(void* dynamic_value_ptr);
double __dynamic_value_extract_float64(void* dynamic_value_ptr);
}
#include <ostream>



// Forward declarations for goroutine functions that are not in runtime.h
extern "C" void* __goroutine_spawn_func_ptr(void* func_ptr, void* arg);
extern "C" void* __goroutine_spawn_func_ptr_with_scope(void* func_ptr, void* arg, void* parent_scope_addr);
extern "C" void* __goroutine_spawn_and_wait_direct(void* function_address);
extern "C" void* __goroutine_spawn_and_wait_fast(void* func_address);
extern "C" void* __goroutine_spawn_direct(void* function_address);

// =============================================================================
// Utility Functions  
// =============================================================================

// Helper function to map integer register IDs to X86Reg enum
// This is needed for the CodeGenerator interface which uses int register IDs
static X86Reg int_to_x86reg(int reg_id) {
    // Add comprehensive debugging to track register corruption
    static int call_count = 0;
    call_count++;
    
    if (reg_id < 0 || reg_id > 15) {
        std::cerr << "\n========== REGISTER CORRUPTION DETECTED ===========" << std::endl;
        std::cerr << "Call #" << call_count << ": CORRUPTED REGISTER ID: " << reg_id << std::endl;
        std::cerr << "Expected range: 0-15, got: " << reg_id << std::endl;
        std::cerr << "Hex value: 0x" << std::hex << reg_id << std::dec << std::endl;
        
        // Check if this looks like a pointer value
        if (reg_id > 100000) {
            std::cerr << "This looks like a memory address/pointer being used as register ID!" << std::endl;
        }
        
        // Print detailed stack trace
        void* array[20];
        size_t size = backtrace(array, 20);
        char** strings = backtrace_symbols(array, size);
        std::cerr << "Detailed call stack:" << std::endl;
        for (size_t i = 0; i < size; i++) {
            std::cerr << "  Frame " << i << ": " << strings[i] << std::endl;
        }
        free(strings);
        std::cerr << "=================================================" << std::endl;
        
        // Return RAX and continue to see if we can get more info
        return X86Reg::RAX;
    }
    
    // Add debug info for valid register calls when we're close to corruption
    if (call_count % 100 == 0) {
        std::cerr << "[REG_DEBUG] Call #" << call_count << ": Valid register " << reg_id << std::endl;
    }
    
    switch (reg_id) {
        case 0: return X86Reg::RAX;
        case 1: return X86Reg::RCX;
        case 2: return X86Reg::RDX;
        case 3: return X86Reg::RBX;
        case 4: return X86Reg::RSP;
        case 5: return X86Reg::RBP;
        case 6: return X86Reg::RSI;
        case 7: return X86Reg::RDI;
        case 8: return X86Reg::R8;
        case 9: return X86Reg::R9;
        case 10: return X86Reg::R10;
        case 11: return X86Reg::R11;
        case 12: return X86Reg::R12;
        case 13: return X86Reg::R13;
        case 14: return X86Reg::R14;
        case 15: return X86Reg::R15;
        default: return X86Reg::RAX;  // Should not reach here due to check above
    }
}

// =============================================================================
// X86CodeGenV2 Implementation
// =============================================================================

X86CodeGenV2::X86CodeGenV2() {
    instruction_builder = std::make_unique<X86InstructionBuilder>(code_buffer);
    pattern_builder = std::make_unique<X86PatternBuilder>(*instruction_builder);
    
    // CRITICAL: Initialize with clean label state to prevent cross-compilation pollution
    instruction_builder->clear_label_state();
}

X86Reg X86CodeGenV2::allocate_register() {
    if (!enable_register_allocation) {
        return X86Reg::RAX;  // Simple fallback
    }
    
    // Find first free register (excluding RSP and RBP)
    for (int i = 0; i < 16; i++) {
        if (i == static_cast<int>(X86Reg::RSP) || i == static_cast<int>(X86Reg::RBP)) {
            continue;
        }
        if (reg_state.is_free[i]) {
            reg_state.is_free[i] = false;
            reg_state.last_allocated = static_cast<X86Reg>(i);
            return static_cast<X86Reg>(i);
        }
    }
    
    // No free registers - spill something (simplified implementation)
    return X86Reg::RAX;
}

void X86CodeGenV2::free_register(X86Reg reg) {
    if (enable_register_allocation) {
        reg_state.is_free[static_cast<int>(reg)] = true;
    }
}

X86Reg X86CodeGenV2::get_register_for_int(int reg_id) {
    return int_to_x86reg(reg_id);
}

void X86CodeGenV2::clear() {
    code_buffer.clear();
    label_offsets.clear();
    unresolved_jumps.clear();
    reg_state = RegisterState();
    stack_frame = StackFrame();
    
    // CRITICAL: Clear label state in instruction builder to prevent label corruption
    if (instruction_builder) {
        instruction_builder->clear_label_state();
    }
}

// =============================================================================
// CodeGenerator Interface Implementation
// =============================================================================

void X86CodeGenV2::emit_prologue() {
    if (stack_frame.frame_established) {
        return;  // Already established
    }
    
    pattern_builder->emit_function_prologue(
        stack_frame.local_stack_size, 
        stack_frame.saved_registers
    );
    
    stack_frame.frame_established = true;
}

void X86CodeGenV2::emit_epilogue() {
    if (!stack_frame.frame_established) {
        return;  // No frame to tear down
    }
    
    pattern_builder->emit_function_epilogue(
        stack_frame.local_stack_size, 
        stack_frame.saved_registers
    );
    
    stack_frame.frame_established = false;
}

void X86CodeGenV2::emit_mov_reg_imm(int reg, int64_t value) {
    X86Reg dst = get_register_for_int(reg);
    instruction_builder->mov(dst, ImmediateOperand(value));
}

void X86CodeGenV2::emit_mov_reg_reg(int dst, int src) {
    X86Reg dst_reg = get_register_for_int(dst);
    X86Reg src_reg = get_register_for_int(src);
    
    // Optimization: eliminate unnecessary moves
    if (dst_reg == src_reg && enable_peephole_optimization) {
        return;  // No-op move
    }
    
    instruction_builder->mov(dst_reg, src_reg);
}

void X86CodeGenV2::emit_mov_mem_reg(int64_t offset, int reg) {
    X86Reg src_reg = get_register_for_int(reg);
    MemoryOperand dst(X86Reg::RBP, static_cast<int32_t>(offset));
    
    // DEBUG: Print the exact assembly operation being generated with sequence number
    static int seq = 0;
    std::cout << "[ASM_DEBUG " << ++seq << "] emit_mov_mem_reg: mov [rbp" 
              << (offset >= 0 ? "+" : "") << offset << "], " 
              << register_name(src_reg) << " (STORING TO STACK)" << std::endl;
    
    instruction_builder->mov(dst, src_reg);
}

void X86CodeGenV2::emit_mov_reg_mem(int reg, int64_t offset) {
    X86Reg dst_reg = get_register_for_int(reg);
    MemoryOperand src(X86Reg::RBP, static_cast<int32_t>(offset));
    
    // DEBUG: Print the exact assembly operation being generated with sequence number
    static int seq = 0;
    std::cout << "[ASM_DEBUG " << ++seq << "] emit_mov_reg_mem: mov " 
              << register_name(dst_reg) << ", [rbp" 
              << (offset >= 0 ? "+" : "") << offset << "] (LOADING FROM STACK)" << std::endl;
    
    instruction_builder->mov(dst_reg, src);
}

// Register-relative memory operations for direct object property access
void X86CodeGenV2::emit_mov_reg_reg_offset(int dst_reg, int src_reg, int64_t offset) {
    // dst = [src+offset] - load from memory at src_reg + offset
    X86Reg dst = get_register_for_int(dst_reg);
    X86Reg src = get_register_for_int(src_reg);
    MemoryOperand mem_operand(src, static_cast<int32_t>(offset));
    instruction_builder->mov(dst, mem_operand);
}

void X86CodeGenV2::emit_mov_reg_offset_reg(int dst_reg, int64_t offset, int src_reg) {
    // [dst+offset] = src - store to memory at dst_reg + offset
    X86Reg dst = get_register_for_int(dst_reg);
    X86Reg src = get_register_for_int(src_reg);
    MemoryOperand mem_operand(dst, static_cast<int32_t>(offset));
    instruction_builder->mov(mem_operand, src);
}

// RSP-relative memory operations for stack manipulation
void X86CodeGenV2::emit_mov_mem_rsp_reg(int64_t offset, int reg) {
    X86Reg src_reg = get_register_for_int(reg);
    MemoryOperand dst(X86Reg::RSP, static_cast<int32_t>(offset));
    instruction_builder->mov(dst, src_reg);
}

void X86CodeGenV2::emit_mov_reg_mem_rsp(int reg, int64_t offset) {
    X86Reg dst_reg = get_register_for_int(reg);
    MemoryOperand src(X86Reg::RSP, static_cast<int32_t>(offset));
    instruction_builder->mov(dst_reg, src);
}

void X86CodeGenV2::emit_add_reg_imm(int reg, int64_t value) {
    X86Reg target_reg = get_register_for_int(reg);
    
    // Optimization: eliminate add 0
    if (value == 0 && enable_peephole_optimization) {
        return;
    }
    
    instruction_builder->add(target_reg, ImmediateOperand(value));
}

void X86CodeGenV2::emit_add_reg_reg(int dst, int src) {
    X86Reg dst_reg = get_register_for_int(dst);
    X86Reg src_reg = get_register_for_int(src);
    instruction_builder->add(dst_reg, src_reg);
}

void X86CodeGenV2::emit_sub_reg_imm(int reg, int64_t value) {
    X86Reg target_reg = get_register_for_int(reg);
    
    // Optimization: eliminate sub 0
    if (value == 0 && enable_peephole_optimization) {
        return;
    }
    
    instruction_builder->sub(target_reg, ImmediateOperand(value));
}

void X86CodeGenV2::emit_sub_reg_reg(int dst, int src) {
    X86Reg dst_reg = get_register_for_int(dst);
    X86Reg src_reg = get_register_for_int(src);
    instruction_builder->sub(dst_reg, src_reg);
}

void X86CodeGenV2::emit_mul_reg_reg(int dst, int src) {
    X86Reg dst_reg = get_register_for_int(dst);
    X86Reg src_reg = get_register_for_int(src);
    instruction_builder->imul(dst_reg, src_reg);
}

void X86CodeGenV2::emit_div_reg_reg(int dst, int src) {
    // CRITICAL ERROR DETECTION WITH ENHANCED DEBUGGING
    std::cerr << "\n========== EMIT_DIV_REG_REG CALLED ===========" << std::endl;
    std::cerr << "[FATAL] emit_div_reg_reg called with dst=" << dst << " src=" << src << std::endl;
    std::cerr << "[FATAL] DST in hex: 0x" << std::hex << dst << std::dec << std::endl;
    std::cerr << "[FATAL] SRC in hex: 0x" << std::hex << src << std::dec << std::endl;
    
    // Check if these look like pointer values
    if (dst > 100000 || src > 100000) {
        std::cerr << "[FATAL] These look like MEMORY ADDRESSES, not register IDs!" << std::endl;
        if (dst > 100000) {
            std::cerr << "[FATAL] DST appears to be a pointer: " << std::hex << dst << std::dec << std::endl;
        }
        if (src > 100000) {
            std::cerr << "[FATAL] SRC appears to be a pointer: " << std::hex << src << std::dec << std::endl;
        }
    }
    
    std::cerr << "[FATAL] This indicates a serious bug - console.log should NEVER need division!" << std::endl;
    std::cerr << "[FATAL] Printing stack trace to find the source:" << std::endl;
    std::cerr.flush();
    
    // Print a comprehensive stack trace
    void *array[20];
    size_t size = backtrace(array, 20);
    char **strings = backtrace_symbols(array, size);
    if (strings != nullptr) {
        for (size_t i = 0; i < size; i++) {
            std::cerr << "[TRACE] Frame " << i << ": " << strings[i] << std::endl;
        }
        free(strings);
    }
    
    // ABORT INSTEAD OF GENERATING BAD CODE
    std::cerr << "[FATAL] Aborting to prevent code corruption!" << std::endl;
    std::cerr << "=============================================" << std::endl;
    std::cerr.flush();
    abort();
}

void X86CodeGenV2::emit_mod_reg_reg(int dst, int src) {
    std::cout << "[DEBUG] ERROR: emit_mod_reg_reg called unexpectedly! dst=" << dst << " src=" << src << std::endl;
    std::cout.flush();
    
    X86Reg dst_reg = get_register_for_int(dst);
    X86Reg src_reg = get_register_for_int(src);
    
    // Set up division: move dividend to RAX, sign extend, divide
    if (dst_reg != X86Reg::RAX) {
        instruction_builder->mov(X86Reg::RAX, dst_reg);
    }
    instruction_builder->cqo();  // Sign extend RAX into RDX:RAX
    instruction_builder->idiv(src_reg);
    
    if (dst_reg != X86Reg::RDX) {
        instruction_builder->mov(dst_reg, X86Reg::RDX);  // Move remainder back
    }
}

void X86CodeGenV2::emit_call(const std::string& label) {
    // MAXIMUM PERFORMANCE: Check for direct function pointer first
    void* runtime_func_ptr = get_runtime_function_address(label);
    if (runtime_func_ptr) {
        // ZERO-OVERHEAD DIRECT CALL: MOV RAX, func_ptr; CALL RAX
        // This generates the fastest possible call sequence - no symbol resolution overhead
        instruction_builder->call(runtime_func_ptr);
    } else {
        // Fallback to label-based call for internal JIT labels only
        instruction_builder->call(label);
    }
}

void* X86CodeGenV2::get_runtime_function_address(const std::string& function_name) {
    // HIGH-PERFORMANCE DIRECT FUNCTION POINTER LOOKUP
    // Lazily initialized to avoid static initialization order issues
    static std::unique_ptr<std::unordered_map<std::string, void*>> runtime_functions;
    
    if (!runtime_functions) {
        runtime_functions = std::make_unique<std::unordered_map<std::string, void*>>();
        auto& rf = *runtime_functions;
        
        // Core runtime functions
        rf["__dynamic_value_create_from_double"] = reinterpret_cast<void*>(__dynamic_value_create_from_double);
        rf["__dynamic_value_create_from_int64"] = reinterpret_cast<void*>(__dynamic_value_create_from_int64);
        rf["__dynamic_value_create_from_uint64"] = reinterpret_cast<void*>(__dynamic_value_create_from_uint64);
        rf["__dynamic_value_create_from_bool"] = reinterpret_cast<void*>(__dynamic_value_create_from_bool);
        rf["__dynamic_value_create_from_string"] = reinterpret_cast<void*>(__dynamic_value_create_from_string);
        rf["__dynamic_value_create_from_object"] = reinterpret_cast<void*>(__dynamic_value_create_from_object);
        rf["__dynamic_value_create_from_array"] = reinterpret_cast<void*>(__dynamic_value_create_from_array);
        rf["__get_executable_memory_base"] = reinterpret_cast<void*>(__get_executable_memory_base);
        rf["__goroutine_spawn_func_ptr"] = reinterpret_cast<void*>(__goroutine_spawn_func_ptr);
        rf["__goroutine_spawn_func_ptr_with_scope"] = reinterpret_cast<void*>(__goroutine_spawn_func_ptr_with_scope);
        rf["__goroutine_spawn_and_wait_direct"] = reinterpret_cast<void*>(__goroutine_spawn_and_wait_direct);
        (*runtime_functions)["__goroutine_spawn_and_wait_fast"] = reinterpret_cast<void*>(__goroutine_spawn_and_wait_fast);
        (*runtime_functions)["__goroutine_spawn_direct"] = reinterpret_cast<void*>(__goroutine_spawn_direct);
        (*runtime_functions)["__string_intern"] = reinterpret_cast<void*>(__string_intern);
        
        // Console.log runtime functions for maximum performance
        (*runtime_functions)["__console_log_int8"] = reinterpret_cast<void*>(__console_log_int8);
        (*runtime_functions)["__console_log_int16"] = reinterpret_cast<void*>(__console_log_int16);
        (*runtime_functions)["__console_log_int32"] = reinterpret_cast<void*>(__console_log_int32);
        (*runtime_functions)["__console_log_int64"] = reinterpret_cast<void*>(__console_log_int64);
        (*runtime_functions)["__console_log_uint8"] = reinterpret_cast<void*>(__console_log_uint8);
        (*runtime_functions)["__console_log_uint16"] = reinterpret_cast<void*>(__console_log_uint16);
        (*runtime_functions)["__console_log_uint32"] = reinterpret_cast<void*>(__console_log_uint32);
        (*runtime_functions)["__console_log_uint64"] = reinterpret_cast<void*>(__console_log_uint64);
        (*runtime_functions)["__console_log_float32"] = reinterpret_cast<void*>(__console_log_float32);
        (*runtime_functions)["__console_log_float64"] = reinterpret_cast<void*>(__console_log_float64);
        (*runtime_functions)["__console_log_boolean"] = reinterpret_cast<void*>(__console_log_boolean);
        (*runtime_functions)["__console_log_string_ptr"] = reinterpret_cast<void*>(__console_log_string_ptr);
        (*runtime_functions)["__console_log_array_ptr"] = reinterpret_cast<void*>(__console_log_array_ptr);
        (*runtime_functions)["__console_log_object_ptr"] = reinterpret_cast<void*>(__console_log_object_ptr);
        (*runtime_functions)["__console_log_function_ptr"] = reinterpret_cast<void*>(__console_log_function_ptr);
        (*runtime_functions)["__console_log_space_separator"] = reinterpret_cast<void*>(__console_log_space_separator);
        (*runtime_functions)["__console_log_final_newline"] = reinterpret_cast<void*>(__console_log_final_newline);
        (*runtime_functions)["__console_log_any_value_inspect"] = reinterpret_cast<void*>(__console_log_any_value_inspect);
        (*runtime_functions)["__console_time"] = reinterpret_cast<void*>(__console_time);
        (*runtime_functions)["__console_timeEnd"] = reinterpret_cast<void*>(__console_timeEnd);
        
        // String functions
        (*runtime_functions)["__string_concat"] = reinterpret_cast<void*>(__string_concat);
        (*runtime_functions)["__string_match"] = reinterpret_cast<void*>(__string_match);
        (*runtime_functions)["__string_create_with_length"] = reinterpret_cast<void*>(__string_create_with_length);
        (*runtime_functions)["__string_equals"] = reinterpret_cast<void*>(__string_equals);
        (*runtime_functions)["__string_compare"] = reinterpret_cast<void*>(__string_compare);
        (*runtime_functions)["__dynamic_value_extract_string"] = reinterpret_cast<void*>(__dynamic_value_extract_string);
        (*runtime_functions)["__dynamic_value_extract_int64"] = reinterpret_cast<void*>(__dynamic_value_extract_int64);
        (*runtime_functions)["__dynamic_value_extract_float64"] = reinterpret_cast<void*>(__dynamic_value_extract_float64);
        
        // Standard C library functions
        (*runtime_functions)["strlen"] = reinterpret_cast<void*>(strlen);
        
        // Array functions (legacy - use type-aware versions below)
        (*runtime_functions)["__array_create"] = reinterpret_cast<void*>(__array_create);
        (*runtime_functions)["__array_push"] = reinterpret_cast<void*>(__array_push);
        (*runtime_functions)["__array_pop"] = reinterpret_cast<void*>(__array_pop);
        (*runtime_functions)["__array_size"] = reinterpret_cast<void*>(__array_size);
        (*runtime_functions)["__array_access"] = reinterpret_cast<void*>(__array_access);
        
        // Typed array access functions for maximum performance
        (*runtime_functions)["__array_access_int64"] = reinterpret_cast<void*>(__array_access_int64);
        (*runtime_functions)["__array_access_float64"] = reinterpret_cast<void*>(__array_access_float64);
        
        (*runtime_functions)["__array_access_int32"] = reinterpret_cast<void*>(__array_access_int32);
        (*runtime_functions)["__array_access_float32"] = reinterpret_cast<void*>(__array_access_float32);
        
        // Class property lookup for optimized bracket access
        (*runtime_functions)["__class_property_lookup"] = reinterpret_cast<void*>(__class_property_lookup);
        
        // Dynamic property functions
        (*runtime_functions)["__dynamic_property_set"] = reinterpret_cast<void*>(__dynamic_property_set);
        (*runtime_functions)["__dynamic_property_get"] = reinterpret_cast<void*>(__dynamic_property_get);
        (*runtime_functions)["__dynamic_property_has"] = reinterpret_cast<void*>(__dynamic_property_has);
        (*runtime_functions)["__dynamic_property_delete"] = reinterpret_cast<void*>(__dynamic_property_delete);
        (*runtime_functions)["__dynamic_property_keys"] = reinterpret_cast<void*>(__dynamic_property_keys);
        (*runtime_functions)["__dynamic_value_create_any"] = reinterpret_cast<void*>(__dynamic_value_create_any);
        
        // For-in loop support functions
        (*runtime_functions)["__get_class_property_count"] = reinterpret_cast<void*>(__get_class_property_count);
        (*runtime_functions)["__get_class_property_name"] = reinterpret_cast<void*>(__get_class_property_name);
        (*runtime_functions)["__debug_reached_static_loop_body"] = reinterpret_cast<void*>(__debug_reached_static_loop_body);
        (*runtime_functions)["__debug_reached_static_loop_body_with_values"] = reinterpret_cast<void*>(__debug_reached_static_loop_body_with_values);
        (*runtime_functions)["__debug_about_to_call_property_name"] = reinterpret_cast<void*>(__debug_about_to_call_property_name);
        (*runtime_functions)["__debug_loop_compare"] = reinterpret_cast<void*>(__debug_loop_compare);
        (*runtime_functions)["__get_dynamic_map"] = reinterpret_cast<void*>(__get_dynamic_map);
        (*runtime_functions)["__get_dynamic_property_count"] = reinterpret_cast<void*>(__get_dynamic_property_count);
        (*runtime_functions)["__get_dynamic_property_name"] = reinterpret_cast<void*>(__get_dynamic_property_name);
        
        // Reference counting functions
        (*runtime_functions)["__object_add_ref"] = reinterpret_cast<void*>(__object_add_ref);
        (*runtime_functions)["__object_release"] = reinterpret_cast<void*>(__object_release);
        (*runtime_functions)["__object_destruct"] = reinterpret_cast<void*>(__object_destruct);
        (*runtime_functions)["__object_free_direct"] = reinterpret_cast<void*>(__object_free_direct);
        (*runtime_functions)["__object_get_ref_count"] = reinterpret_cast<void*>(__object_get_ref_count);
        
        // Stack debugging functions
        (*runtime_functions)["__debug_stack_store"] = reinterpret_cast<void*>(__debug_stack_store);
        (*runtime_functions)["__debug_stack_load"] = reinterpret_cast<void*>(__debug_stack_load);
        
        // Advanced dynamic value reference counting functions
        (*runtime_functions)["__dynamic_value_release_if_object"] = reinterpret_cast<void*>(__dynamic_value_release_if_object);
        (*runtime_functions)["__dynamic_value_copy_with_refcount"] = reinterpret_cast<void*>(__dynamic_value_copy_with_refcount);
        (*runtime_functions)["__dynamic_value_extract_object_with_refcount"] = reinterpret_cast<void*>(__dynamic_value_extract_object_with_refcount);
        
        // Type-aware array creation functions  
        (*runtime_functions)["__array_create_dynamic"] = reinterpret_cast<void*>(__array_create_dynamic);
        (*runtime_functions)["__array_create_int64"] = reinterpret_cast<void*>(__array_create_int64);
        (*runtime_functions)["__array_create_float64"] = reinterpret_cast<void*>(__array_create_float64);
        (*runtime_functions)["__array_create_int32"] = reinterpret_cast<void*>(__array_create_int32);
        (*runtime_functions)["__array_create_float32"] = reinterpret_cast<void*>(__array_create_float32);
        
        // Type-aware array push functions
        (*runtime_functions)["__array_push_dynamic"] = reinterpret_cast<void*>(__array_push_dynamic);
        (*runtime_functions)["__array_push_int64_typed"] = reinterpret_cast<void*>(__array_push_int64_typed);
        (*runtime_functions)["__array_push_float64_typed"] = reinterpret_cast<void*>(__array_push_float64_typed);
        (*runtime_functions)["__array_push_int32_typed"] = reinterpret_cast<void*>(__array_push_int32_typed);
        (*runtime_functions)["__array_push_float32_typed"] = reinterpret_cast<void*>(__array_push_float32_typed);
        
        // Array factory functions
        (*runtime_functions)["__array_zeros_typed"] = reinterpret_cast<void*>(__array_zeros_typed);
        (*runtime_functions)["__array_ones_dynamic"] = reinterpret_cast<void*>(__array_ones_dynamic);
        (*runtime_functions)["__array_ones_int64"] = reinterpret_cast<void*>(__array_ones_int64);
        (*runtime_functions)["__array_ones_float64"] = reinterpret_cast<void*>(__array_ones_float64);
        (*runtime_functions)["__array_ones_int32"] = reinterpret_cast<void*>(__array_ones_int32);
        (*runtime_functions)["__array_ones_float32"] = reinterpret_cast<void*>(__array_ones_float32);
        
        // Object functions
        (*runtime_functions)["__object_create"] = reinterpret_cast<void*>(__object_create);
        // Property access functions removed - will be reimplemented according to new architecture
        
        // JIT Object system functions
        (*runtime_functions)["__jit_object_create"] = reinterpret_cast<void*>(__jit_object_create);
        (*runtime_functions)["__jit_object_create_sized"] = reinterpret_cast<void*>(__jit_object_create_sized);
        
        // Promise functions
        (*runtime_functions)["__promise_all"] = reinterpret_cast<void*>(__promise_all);
        (*runtime_functions)["__promise_await"] = reinterpret_cast<void*>(__promise_await);
        
        // Regex functions
        (*runtime_functions)["__register_regex_pattern"] = reinterpret_cast<void*>(__register_regex_pattern);
        (*runtime_functions)["__regex_create_by_id"] = reinterpret_cast<void*>(__regex_create_by_id);
        
        // Runtime syscalls for time
        (*runtime_functions)["__runtime_time_now_millis"] = reinterpret_cast<void*>(__runtime_time_now_millis);
        
        // Goroutine functions (dynamic name patterns need special handling)
        // All console.log functions resolved to direct pointers for ZERO overhead
        
        // Free runtime functions for ultra-fast memory management
        (*runtime_functions)["__free_class_instance_shallow"] = reinterpret_cast<void*>(__free_class_instance_shallow);
        (*runtime_functions)["__free_class_instance_deep"] = reinterpret_cast<void*>(__free_class_instance_deep);
        (*runtime_functions)["__free_array_shallow"] = reinterpret_cast<void*>(__free_array_shallow);
        (*runtime_functions)["__free_array_deep"] = reinterpret_cast<void*>(__free_array_deep);
        (*runtime_functions)["__free_string"] = reinterpret_cast<void*>(__free_string);
        (*runtime_functions)["__free_dynamic_value"] = reinterpret_cast<void*>(__free_dynamic_value);
        (*runtime_functions)["__debug_log_primitive_free_ignored"] = reinterpret_cast<void*>(__debug_log_primitive_free_ignored);
        (*runtime_functions)["__throw_deep_free_not_implemented"] = reinterpret_cast<void*>(__throw_deep_free_not_implemented);
        
        // Debug and introspection functions
        (*runtime_functions)["__debug_get_ref_count"] = reinterpret_cast<void*>(__debug_get_ref_count);
        (*runtime_functions)["__object_get_memory_address"] = reinterpret_cast<void*>(__object_get_memory_address);
        (*runtime_functions)["__runtime_get_ref_count"] = reinterpret_cast<void*>(__runtime_get_ref_count);
        
        // Goroutine System V2 functions
        (*runtime_functions)["__gots_set_timeout"] = reinterpret_cast<void*>(__gots_set_timeout_v2);
        (*runtime_functions)["__gots_set_interval"] = reinterpret_cast<void*>(__gots_set_interval_v2);
        (*runtime_functions)["__gots_clear_timeout"] = reinterpret_cast<void*>(__gots_clear_timeout_v2);
        (*runtime_functions)["__gots_clear_interval"] = reinterpret_cast<void*>(__gots_clear_interval_v2);
        (*runtime_functions)["__gots_add_async_handle"] = reinterpret_cast<void*>(__gots_add_async_handle_v2);
        (*runtime_functions)["__gots_complete_async_handle"] = reinterpret_cast<void*>(__gots_complete_async_handle_v2);
        (*runtime_functions)["__gots_cancel_async_handle"] = reinterpret_cast<void*>(__gots_cancel_async_handle_v2);
        (*runtime_functions)["__runtime_spawn_main_goroutine"] = reinterpret_cast<void*>(__runtime_spawn_main_goroutine_v2);
        (*runtime_functions)["__runtime_wait_for_main_goroutine"] = reinterpret_cast<void*>(__runtime_wait_for_main_goroutine_v2);
        (*runtime_functions)["__runtime_spawn_goroutine"] = reinterpret_cast<void*>(__runtime_spawn_goroutine_v2);
        
        // FFI integration functions
        (*runtime_functions)["execute_ffi_call"] = reinterpret_cast<void*>(execute_ffi_call);
        (*runtime_functions)["migrate_to_ffi_thread"] = reinterpret_cast<void*>(migrate_to_ffi_thread);
        (*runtime_functions)["is_goroutine_ffi_bound"] = reinterpret_cast<void*>(is_goroutine_ffi_bound);
    }
    
    auto it = (*runtime_functions).find(function_name);
    if (it != (*runtime_functions).end()) {
        std::cout << "[DEBUG] DIRECT FUNCTION POINTER: " << function_name 
                  << " -> " << it->second << " (0x" << std::hex << 
                  reinterpret_cast<uintptr_t>(it->second) << std::dec << ")" << std::endl;
        return it->second;
    }
    
    // Handle dynamic goroutine spawn functions
    if (function_name.find("__goroutine_spawn_with_args_") == 0) {
        // This is a dynamically generated goroutine spawn function
        // We need to use the general goroutine spawn mechanism
        std::cout << "[DEBUG] Using generic goroutine spawn for: " << function_name << std::endl;
        return reinterpret_cast<void*>(__goroutine_spawn_func_ptr);
    }
    
    // Handle goroutine spawn with scope
    if (function_name == "__goroutine_spawn_func_ptr_with_scope") {
        return reinterpret_cast<void*>(__goroutine_spawn_func_ptr_with_scope);
    }
    
    return nullptr;
}

void X86CodeGenV2::emit_ret() {
    instruction_builder->ret();
}

void X86CodeGenV2::emit_function_return() {
    emit_epilogue();  // The epilogue already includes ret instruction
}

void X86CodeGenV2::emit_jump(const std::string& label) {
    instruction_builder->jmp(label);
}

void X86CodeGenV2::emit_jump_if_zero(const std::string& label) {
    instruction_builder->jz(label);
}

void X86CodeGenV2::emit_jump_if_not_zero(const std::string& label) {
    instruction_builder->jnz(label);
}

void X86CodeGenV2::emit_jump_if_greater_equal(const std::string& label) {
    instruction_builder->jge(label);
}

void X86CodeGenV2::emit_compare(int reg1, int reg2) {
    X86Reg left = get_register_for_int(reg1);
    X86Reg right = get_register_for_int(reg2);
    instruction_builder->cmp(left, right);
}

void X86CodeGenV2::emit_setl(int reg) {
    X86Reg target = get_register_for_int(reg);
    instruction_builder->setl(target);
}

void X86CodeGenV2::emit_setg(int reg) {
    X86Reg target = get_register_for_int(reg);
    instruction_builder->setg(target);
}

void X86CodeGenV2::emit_sete(int reg) {
    X86Reg target = get_register_for_int(reg);
    instruction_builder->setz(target);
}

void X86CodeGenV2::emit_setne(int reg) {
    X86Reg target = get_register_for_int(reg);
    instruction_builder->setnz(target);
}

void X86CodeGenV2::emit_setle(int reg) {
    X86Reg target = get_register_for_int(reg);
    instruction_builder->setle(target);
}

void X86CodeGenV2::emit_setge(int reg) {
    X86Reg target = get_register_for_int(reg);
    instruction_builder->setge(target);
}

void X86CodeGenV2::emit_and_reg_imm(int reg, int64_t value) {
    X86Reg target = get_register_for_int(reg);
    instruction_builder->and_(target, ImmediateOperand(value));
}

void X86CodeGenV2::emit_xor_reg_reg(int dst, int src) {
    X86Reg dst_reg = get_register_for_int(dst);
    X86Reg src_reg = get_register_for_int(src);
    instruction_builder->xor_(dst_reg, src_reg);
}

void X86CodeGenV2::emit_call_reg(int reg) {
    X86Reg target = get_register_for_int(reg);
    instruction_builder->call(target);
}

void X86CodeGenV2::emit_label(const std::string& label) {
    size_t current_pos = instruction_builder->get_current_position();
    instruction_builder->resolve_label(label, current_pos);
    label_offsets[label] = static_cast<int64_t>(current_pos);
    
    // Register method offsets for runtime lookup
    if (label.find("__method_") == 0) {
        // Extract the method name (everything after "__method_")
        std::string method_name = label.substr(9); // Skip "__method_"
        
        // Call the runtime function to register this method
        __register_method_offset(label.c_str(), current_pos);
    }
}

// =============================================================================
// Goroutine and Concurrency Operations
// =============================================================================

void X86CodeGenV2::emit_goroutine_spawn(const std::string& function_name) {
    // MAXIMUM PERFORMANCE - NO FALLBACKS
    // If the function isn't found, that's a compilation error that should fail immediately
    
    pattern_builder->setup_function_call({X86Reg::RDI, X86Reg::RSI});
    
    // Function MUST be already resolved - no fallbacks, no compromises
    auto it = label_offsets.find(function_name);
    if (it == label_offsets.end()) {
        // FAIL FAST: This is a compilation error, not something to work around
        printf("[FATAL] Function '%s' not found for goroutine spawn - this is a compilation bug!\n", 
               function_name.c_str());
        abort();
    }
    
    int64_t func_offset = it->second;
    
    // Sanity check: ensure offset is reasonable
    if (func_offset < 0 || func_offset >= 1024*1024) {
        printf("[FATAL] Invalid function offset %ld for '%s' - compilation corrupted!\n", 
               func_offset, function_name.c_str());
        abort();
    }
    
    // MAXIMUM PERFORMANCE PATH: Direct address calculation
    emit_call("__get_executable_memory_base");  // Returns base in RAX
    instruction_builder->add(X86Reg::RAX, ImmediateOperand(func_offset));
    instruction_builder->mov(X86Reg::RDI, X86Reg::RAX);
    
    // Set RSI to null (no arguments)
    instruction_builder->mov(X86Reg::RSI, ImmediateOperand(static_cast<int64_t>(0)));
    
    // Direct call to goroutine spawn
    emit_call("__goroutine_spawn_func_ptr");
    
    pattern_builder->cleanup_function_call(0);
}

void X86CodeGenV2::emit_goroutine_spawn_with_args(const std::string& function_name, int arg_count) {
    // More complex goroutine spawning with arguments
    std::vector<X86Reg> args;
    const X86Reg arg_regs[] = {X86Reg::RDI, X86Reg::RSI, X86Reg::RDX, X86Reg::RCX, X86Reg::R8, X86Reg::R9};
    
    for (int i = 0; i < std::min(arg_count, 6); i++) {
        args.push_back(arg_regs[i]);
    }
    
    pattern_builder->setup_function_call(args);
    instruction_builder->call("__goroutine_spawn_with_args_" + function_name);
    pattern_builder->cleanup_function_call(arg_count > 6 ? (arg_count - 6) * 8 : 0);
}

void X86CodeGenV2::emit_goroutine_spawn_with_func_ptr() {
    // Function pointer is expected to be in RDI
    instruction_builder->call("__goroutine_spawn_func_ptr");
}

void X86CodeGenV2::emit_goroutine_spawn_with_func_id() {
    // Function ID is expected to be in RDI
    instruction_builder->call("__goroutine_spawn_func_id");
}

void X86CodeGenV2::emit_goroutine_spawn_with_address(void* function_address) {
    instruction_builder->mov(X86Reg::RDI, ImmediateOperand(reinterpret_cast<int64_t>(function_address)));
    instruction_builder->call("__goroutine_spawn_func_ptr");
}

void X86CodeGenV2::emit_promise_resolve(int value_reg) {
    X86Reg value = get_register_for_int(value_reg);
    instruction_builder->mov(X86Reg::RDI, value);
    instruction_builder->call("__promise_resolve");
}

void X86CodeGenV2::emit_promise_await(int promise_reg) {
    X86Reg promise = get_register_for_int(promise_reg);
    instruction_builder->mov(X86Reg::RDI, promise);
    instruction_builder->call("__promise_await");
}

// =============================================================================
// High-Performance Function Calls
// =============================================================================

void X86CodeGenV2::emit_call_fast(uint16_t func_id) {
    instruction_builder->mov(X86Reg::RDI, ImmediateOperand(func_id));
    instruction_builder->call("__call_fast_by_id");
}

void X86CodeGenV2::emit_goroutine_spawn_fast(uint16_t func_id) {
    instruction_builder->mov(X86Reg::RDI, ImmediateOperand(func_id));
    instruction_builder->call("__goroutine_spawn_fast_by_id");
}

void X86CodeGenV2::emit_goroutine_spawn_direct(void* function_address) {
    // Ultra-fast direct address spawning
    instruction_builder->mov(X86Reg::RDI, ImmediateOperand(reinterpret_cast<int64_t>(function_address)));
    instruction_builder->call("__goroutine_spawn_direct");
}

void X86CodeGenV2::emit_goroutine_spawn_and_wait_direct(void* function_address) {
    // Spawn goroutine with direct address and wait for result
    instruction_builder->mov(X86Reg::RDI, ImmediateOperand(reinterpret_cast<int64_t>(function_address)));
    instruction_builder->call("__goroutine_spawn_and_wait_direct");
}

void X86CodeGenV2::emit_goroutine_spawn_and_wait_fast(uint16_t func_id) {
    // Spawn goroutine with function ID and wait for result
    instruction_builder->mov(X86Reg::RDI, ImmediateOperand(func_id));
    instruction_builder->call("__goroutine_spawn_and_wait_fast");
}

// =============================================================================
// Lock and Atomic Operations
// =============================================================================

void X86CodeGenV2::emit_lock_acquire(int lock_reg) {
    X86Reg lock_ptr = get_register_for_int(lock_reg);
    instruction_builder->mov(X86Reg::RDI, lock_ptr);
    instruction_builder->call("__lock_acquire");
}

void X86CodeGenV2::emit_lock_release(int lock_reg) {
    X86Reg lock_ptr = get_register_for_int(lock_reg);
    instruction_builder->mov(X86Reg::RDI, lock_ptr);
    instruction_builder->call("__lock_release");
}

void X86CodeGenV2::emit_lock_try_acquire(int lock_reg, int result_reg) {
    X86Reg lock_ptr = get_register_for_int(lock_reg);
    X86Reg result = get_register_for_int(result_reg);
    instruction_builder->mov(X86Reg::RDI, lock_ptr);
    instruction_builder->call("__lock_try_acquire");
    if (result != X86Reg::RAX) {
        instruction_builder->mov(result, X86Reg::RAX);
    }
}

void X86CodeGenV2::emit_lock_try_acquire_timeout(int lock_reg, int timeout_reg, int result_reg) {
    X86Reg lock_ptr = get_register_for_int(lock_reg);
    X86Reg timeout = get_register_for_int(timeout_reg);
    X86Reg result = get_register_for_int(result_reg);
    
    instruction_builder->mov(X86Reg::RDI, lock_ptr);
    instruction_builder->mov(X86Reg::RSI, timeout);
    instruction_builder->call("__lock_try_acquire_timeout");
    
    if (result != X86Reg::RAX) {
        instruction_builder->mov(result, X86Reg::RAX);
    }
}

void X86CodeGenV2::emit_atomic_compare_exchange(int ptr_reg, int expected_reg, int desired_reg, int result_reg) {
    X86Reg ptr = get_register_for_int(ptr_reg);
    X86Reg expected = get_register_for_int(expected_reg);
    X86Reg desired = get_register_for_int(desired_reg);
    X86Reg result = get_register_for_int(result_reg);
    
    // Set up for CMPXCHG: RAX = expected, desired in desired_reg
    if (expected != X86Reg::RAX) {
        instruction_builder->mov(X86Reg::RAX, expected);
    }
    
    instruction_builder->cmpxchg(MemoryOperand(ptr), desired);
    
    // Set result based on success (ZF)
    pattern_builder->emit_boolean_result(0x94, result);  // SETE
}

void X86CodeGenV2::emit_atomic_fetch_add(int ptr_reg, int value_reg, int result_reg) {
    X86Reg ptr = get_register_for_int(ptr_reg);
    X86Reg value = get_register_for_int(value_reg);
    X86Reg result = get_register_for_int(result_reg);
    
    if (result != value) {
        instruction_builder->mov(result, value);
    }
    
    instruction_builder->xadd(MemoryOperand(ptr), result);
}

void X86CodeGenV2::emit_atomic_store(int ptr_reg, int value_reg, int memory_order) {
    X86Reg ptr = get_register_for_int(ptr_reg);
    X86Reg value = get_register_for_int(value_reg);
    
    // Simple atomic store using MOV (x86-64 guarantees atomicity for aligned stores)
    instruction_builder->mov(MemoryOperand(ptr), value);
    
    // Add memory fence if needed based on memory order
    if (memory_order > 0) {  // Simplified: any non-relaxed ordering gets mfence
        instruction_builder->mfence();
    }
}

void X86CodeGenV2::emit_atomic_load(int ptr_reg, int result_reg, int memory_order) {
    X86Reg ptr = get_register_for_int(ptr_reg);
    X86Reg result = get_register_for_int(result_reg);
    
    // Simple atomic load using MOV (x86-64 guarantees atomicity for aligned loads)
    instruction_builder->mov(result, MemoryOperand(ptr));
    
    // Add memory fence if needed based on memory order
    if (memory_order > 0) {  // Simplified: any non-relaxed ordering gets mfence
        instruction_builder->mfence();
    }
}

void X86CodeGenV2::emit_memory_fence(int fence_type) {
    switch (fence_type) {
        case 0: /* relaxed - no fence */ break;
        case 1: instruction_builder->lfence(); break;  // Acquire
        case 2: instruction_builder->sfence(); break;  // Release
        case 3: instruction_builder->mfence(); break;  // AcqRel
        case 4: instruction_builder->mfence(); break;  // SeqCst
        default: instruction_builder->mfence(); break; // Default to full fence
    }
}

void X86CodeGenV2::emit_ref_count_increment(int object_reg) {
    X86Reg obj = get_register_for_int(object_reg);
    
    // Ultra-high performance atomic increment of ref_count at offset 16
    // Uses the x86 LOCK INC instruction for maximum performance
    // LOCK INC [obj + OBJECT_REF_COUNT_OFFSET] - atomic increment, fastest possible
    MemoryOperand ref_count_addr(obj, OBJECT_REF_COUNT_OFFSET);
    
    // Emit: lock inc qword ptr [obj + 16] - This is the fastest atomic increment possible
    instruction_builder->lock_inc(ref_count_addr, OpSize::QWORD);
}

void X86CodeGenV2::emit_ref_count_decrement(int object_reg, int result_reg) {
    X86Reg obj = get_register_for_int(object_reg);
    // Inline: lock dec [obj + OBJECT_REF_COUNT_OFFSET]
    MemoryOperand ref_count_addr(obj, OBJECT_REF_COUNT_OFFSET);
    instruction_builder->lock_dec(ref_count_addr, OpSize::QWORD);

    // Inline: jnz skip_destruct
    std::string skip_label = generate_unique_label("skip_destruct");
    instruction_builder->jnz(skip_label);

    // Inline: call destructor function (shared routine)
    // rdi should hold the object pointer for the destructor ABI
    instruction_builder->mov(X86Reg::RDI, obj); // Move object pointer to rdi
    emit_call("__object_destruct");

    // skip_destruct:
    emit_label(skip_label);
}

// Additional ultra-fast reference counting operations for specific use cases
void X86CodeGenV2::emit_ref_count_increment_simple(int object_reg) {
    // Fastest possible increment - no return value needed
    X86Reg obj = get_register_for_int(object_reg);
    MemoryOperand ref_count_addr(obj, OBJECT_REF_COUNT_OFFSET);
    instruction_builder->lock_inc(ref_count_addr, OpSize::QWORD);
}

void X86CodeGenV2::emit_ref_count_decrement_simple(int object_reg) {
    // Fastest possible decrement - no return value needed
    X86Reg obj = get_register_for_int(object_reg);
    MemoryOperand ref_count_addr(obj, OBJECT_REF_COUNT_OFFSET);
    instruction_builder->lock_dec(ref_count_addr, OpSize::QWORD);
}

void X86CodeGenV2::emit_ref_count_check_zero_and_free(int object_reg, const std::string& free_label) {
    // Ultra-optimized: decrement and jump to free if zero
    X86Reg obj = get_register_for_int(object_reg);
    MemoryOperand ref_count_addr(obj, OBJECT_REF_COUNT_OFFSET);
    
    // Atomic decrement
    instruction_builder->lock_dec(ref_count_addr, OpSize::QWORD);
    
    // Jump to free_label if zero flag is set (ref_count became 0)
    instruction_builder->jz(free_label);
}

// =============================================================================
// Advanced High-Level APIs
// =============================================================================

void X86CodeGenV2::emit_function_call(const std::string& function_name, const std::vector<int>& args) {
    std::vector<X86Reg> arg_regs;
    for (int arg : args) {
        arg_regs.push_back(get_register_for_int(arg));
    }
    
    pattern_builder->setup_function_call(arg_regs);
    instruction_builder->call(function_name);
    pattern_builder->cleanup_function_call(args.size() > 6 ? (args.size() - 6) * 8 : 0);
}

void X86CodeGenV2::emit_typed_array_access(int array_reg, int index_reg, int result_reg, OpSize element_size) {
    X86Reg array = get_register_for_int(array_reg);
    X86Reg index = get_register_for_int(index_reg);
    X86Reg result = get_register_for_int(result_reg);
    
    pattern_builder->emit_typed_array_access(array, index, result, element_size);
}

void X86CodeGenV2::emit_string_operation(const std::string& operation, int str1_reg, int str2_reg, int result_reg) {
    X86Reg str1 = get_register_for_int(str1_reg);
    X86Reg str2 = get_register_for_int(str2_reg);
    X86Reg result = get_register_for_int(result_reg);
    
    if (operation == "length") {
        pattern_builder->emit_string_length_calculation(str1, result);
    } else if (operation == "compare") {
        pattern_builder->emit_string_comparison(str1, str2, result);
    } else if (operation == "concat") {
        pattern_builder->emit_string_concatenation(str1, str2, result);
    }
}

void X86CodeGenV2::emit_bounds_check(int index_reg, int limit_reg) {
    X86Reg index = get_register_for_int(index_reg);
    X86Reg limit = get_register_for_int(limit_reg);
    pattern_builder->emit_bounds_check(index, limit, "__bounds_error");
}

void X86CodeGenV2::emit_null_check(int pointer_reg) {
    X86Reg pointer = get_register_for_int(pointer_reg);
    pattern_builder->emit_null_check(pointer, "__null_pointer_error");
}

// =============================================================================
// High-Performance Floating-Point Operations  
// =============================================================================

void X86CodeGenV2::emit_movq_xmm_gpr(int xmm_reg, int gpr_reg) {
    // Move 64-bit from GPR to XMM register for high-performance floating-point
    X86Reg gpr = get_register_for_int(gpr_reg);
    X86XmmReg xmm = static_cast<X86XmmReg>(xmm_reg);
    instruction_builder->movq(xmm, gpr);
}

void X86CodeGenV2::emit_movq_gpr_xmm(int gpr_reg, int xmm_reg) {
    // Move 64-bit from XMM to GPR register
    X86Reg gpr = get_register_for_int(gpr_reg);
    X86XmmReg xmm = static_cast<X86XmmReg>(xmm_reg);
    instruction_builder->movq(gpr, xmm);
}

void X86CodeGenV2::emit_movsd_xmm_xmm(int dst_xmm, int src_xmm) {
    // Move scalar double between XMM registers
    X86XmmReg dst = static_cast<X86XmmReg>(dst_xmm);
    X86XmmReg src = static_cast<X86XmmReg>(src_xmm);
    instruction_builder->movsd(dst, src);
}

void X86CodeGenV2::emit_cvtsi2sd(int xmm_reg, int gpr_reg) {
    // Convert signed integer to double precision floating-point
    X86XmmReg xmm = static_cast<X86XmmReg>(xmm_reg);
    X86Reg gpr = get_register_for_int(gpr_reg);
    instruction_builder->cvtsi2sd(xmm, gpr);
}

void X86CodeGenV2::emit_cvtsd2si(int gpr_reg, int xmm_reg) {
    // Convert double precision floating-point to signed integer
    X86Reg gpr = get_register_for_int(gpr_reg);
    X86XmmReg xmm = static_cast<X86XmmReg>(xmm_reg);
    instruction_builder->cvtsd2si(gpr, xmm);
}

void X86CodeGenV2::emit_call_with_double_arg(const std::string& function_name, int value_gpr_reg) {
    // High-performance floating-point function call with proper x86-64 calling convention
    // Convert integer bit pattern in GPR to proper double in XMM0
    X86Reg value_gpr = get_register_for_int(value_gpr_reg);
    
    // Move integer bit pattern to XMM0 (first floating-point argument register)
    instruction_builder->movq(X86XmmReg::XMM0, value_gpr);
    
    // Call the function - floating-point argument is now properly in XMM0
    emit_call(function_name);
}

void X86CodeGenV2::emit_call_with_xmm_arg(const std::string& function_name, int xmm_reg) {
    // Call function with floating-point argument already in XMM register
    if (xmm_reg != 0) {
        // Move to XMM0 if not already there (x86-64 calling convention)
        X86XmmReg src = static_cast<X86XmmReg>(xmm_reg);
        instruction_builder->movsd(X86XmmReg::XMM0, src);
    }
    
    // Call the function
    emit_call(function_name);
}

// =============================================================================
// Performance and Debugging
// =============================================================================

size_t X86CodeGenV2::get_instruction_count() const {
    // Rough estimate - would need more sophisticated analysis for accurate count
    return code_buffer.size() / 3;  // Average instruction length
}

void X86CodeGenV2::print_assembly_debug() const {
    std::cout << "=== COMPLETE MACHINE CODE DEBUG ===" << std::endl;
    std::cout << "Total size: " << code_buffer.size() << " bytes" << std::endl;
    
    for (size_t i = 0; i < code_buffer.size(); i += 16) {
        std::cout << std::hex << std::setfill('0') << std::setw(8) << i << ": ";
        for (size_t j = i; j < std::min(i + 16, code_buffer.size()); j++) {
            std::cout << std::hex << std::setfill('0') << std::setw(2) << static_cast<unsigned>(code_buffer[j]) << " ";
        }
        
        // Add instruction analysis for key sequences
        if (i == 0) {
            std::cout << " <- Function prologue";
        }
        
        std::cout << "\n";
    }
    std::cout << std::dec;  // Reset to decimal
    std::cout << "=== END MACHINE CODE DEBUG ===" << std::endl;
}

const std::unordered_map<std::string, int64_t>& X86CodeGenV2::get_label_offsets() const {
    return label_offsets;
}

// =============================================================================
// Factory and Testing
// =============================================================================

std::unique_ptr<CodeGenerator> create_optimized_x86_codegen() {
    return std::make_unique<X86CodeGenV2>();
}

bool X86CodeGenTester::validate_instruction_encoding(const std::vector<uint8_t>& code) {
    // Basic validation - check for common encoding errors
    if (code.empty()) return false;
    
    // Check for valid instruction starts (simplified)
    for (size_t i = 0; i < code.size(); i++) {
        uint8_t byte = code[i];
        // Skip REX prefixes
        if (byte >= 0x40 && byte <= 0x4F) continue;
        // Check for invalid opcodes
        if (byte == 0x00 && i > 0) return false;  // Unexpected null byte
    }
    
    return true;
}

bool X86CodeGenV2::validate_code_generation() const {
    printf("[VALIDATION] Validating code generation...\n");
    
    // Critical validation: Ensure all labels are resolved
    if (!instruction_builder->validate_all_labels_resolved()) {
        printf("ERROR: Unresolved labels detected in code generation!\n");
        return false;
    }
    
    // Validate code buffer is not empty
    if (code_buffer.empty()) {
        printf("ERROR: Code buffer is empty after generation!\n");
        return false;
    }
    
    // Validate instruction stream integrity
    if (!instruction_builder->validate_instruction_stream()) {
        printf("ERROR: Invalid instruction stream detected!\n");
        return false;
    }
    
    printf("[VALIDATION] Code generation validation successful - %zu bytes generated\n", 
           code_buffer.size());
    return true;
}

void X86CodeGenV2::resolve_runtime_function_calls() {
    // Runtime function resolution is handled during code generation
    // through the get_runtime_function_address() method in emit_call()
    // No post-processing needed for the new system
}

// Factory function implementation
std::unique_ptr<CodeGenerator> create_x86_codegen() {
    return std::make_unique<X86CodeGenV2>();
}

void X86CodeGenTester::benchmark_code_generation_speed() {
    // Performance benchmarking would go here
    std::cout << "Benchmarking code generation speed...\n";
    // Implementation would time various code generation patterns
}

// HIGH-PERFORMANCE LEXICAL SCOPE REGISTER MANAGEMENT
void X86CodeGenV2::emit_scope_register_setup(int scope_level) {
    std::cout << "[DEBUG] X86CodeGenV2: Setting up scope registers for level " << scope_level << std::endl;
    
    // For now, just emit a basic setup - this should be integrated with StaticScopeAnalyzer later
    // Save R12 (our primary scope register)
    emit_scope_register_save(12);
    
    // Initialize R12 to null for now - actual scope setup would happen elsewhere
    X86Reg reg = X86Reg::R12;
    ImmediateOperand zero_imm(0);
    instruction_builder->mov(reg, zero_imm);
    std::cout << "[DEBUG] X86CodeGenV2: Initialized scope register R12 to null" << std::endl;
}

void X86CodeGenV2::emit_scope_register_save(int reg_id) {
    // Save callee-saved register to stack
    X86Reg reg = get_register_for_int(reg_id);
    instruction_builder->push(reg);
    stack_frame.saved_registers.push_back(reg);
    std::cout << "[DEBUG] X86CodeGenV2: Saved scope register " << reg_id << " to stack" << std::endl;
}

void X86CodeGenV2::emit_scope_register_restore(int reg_id) {
    // Restore callee-saved register from stack
    X86Reg reg = get_register_for_int(reg_id);
    instruction_builder->pop(reg);
    std::cout << "[DEBUG] X86CodeGenV2: Restored scope register " << reg_id << " from stack" << std::endl;
}

void X86CodeGenV2::emit_scope_pointer_load(int reg_id, int scope_level) {
    // Load scope pointer for a specific scope level into the designated register
    // This would typically load from parent function's stack frame or heap
    X86Reg scope_reg = get_register_for_int(reg_id);
    
    // For now, load from a stack offset based on scope level
    // Real implementation would use actual scope management
    int64_t scope_offset = -16 - (scope_level * 8);  // Stack-based fallback
    MemoryOperand mem_op(X86Reg::RBP, scope_offset);
    instruction_builder->mov(scope_reg, mem_op);
    
    std::cout << "[DEBUG] X86CodeGenV2: Loaded scope level " << scope_level 
              << " pointer into register " << reg_id << " from offset " << scope_offset << std::endl;
}

void X86CodeGenV2::emit_variable_load_from_scope_register(int dst_reg, int scope_reg, int64_t offset) {
    // ULTRA-FAST: Direct load from scope register + offset (1 instruction)
    X86Reg dst = get_register_for_int(dst_reg);
    X86Reg scope = get_register_for_int(scope_reg);
    
    MemoryOperand mem_op(scope, offset);
    instruction_builder->mov(dst, mem_op);
    
    std::cout << "[DEBUG] X86CodeGenV2: ULTRA-FAST variable access: R" << dst_reg 
              << " = [R" << scope_reg << " + " << offset << "]" << std::endl;
}

// INLINE HEAP ALLOCATION FOR LEXICAL SCOPES (ultra-fast malloc alternative)
void X86CodeGenV2::emit_inline_heap_alloc(size_t size, int result_reg) {
    std::cout << "[HEAP_ALLOC_DEBUG] Emitting inline heap allocation for " << size << " bytes" << std::endl;
    
    X86Reg result = get_register_for_int(result_reg);
    
    // Ultra-fast inline heap allocation using a simple bump allocator
    // This avoids expensive malloc() calls by using pre-allocated heap space
    
    // For now, use malloc() via system call (can optimize later with bump allocator)
    // mov rdi, size        ; First argument: size
    instruction_builder->mov(X86Reg::RDI, static_cast<int64_t>(size));
    
    // Call malloc - we'll use the C runtime malloc for now
    // Note: This requires linking with libc, but gives us proper memory management
    instruction_builder->mov(X86Reg::RAX, reinterpret_cast<int64_t>(malloc));
    instruction_builder->call(X86Reg::RAX);
    
    // Result is in RAX, move to requested result register if different
    if (result != X86Reg::RAX) {
        instruction_builder->mov(result, X86Reg::RAX);
    }
    
    std::cout << "[HEAP_ALLOC_DEBUG] Allocated " << size << " bytes, address in register R" << result_reg << std::endl;
}


