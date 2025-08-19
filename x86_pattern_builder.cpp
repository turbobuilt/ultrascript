#include "x86_instruction_builder.h"
#include <algorithm>



// =============================================================================
// X86PatternBuilder Implementation  
// =============================================================================

// =============================================================================
// Function Call Patterns
// =============================================================================

void X86PatternBuilder::setup_function_call(const std::vector<X86Reg>& args) {
    // x86-64 System V ABI calling convention
    // Arguments in: RDI, RSI, RDX, RCX, R8, R9, then stack
    
    const X86Reg arg_registers[] = { 
        X86Reg::RDI, X86Reg::RSI, X86Reg::RDX, 
        X86Reg::RCX, X86Reg::R8, X86Reg::R9 
    };
    
    // Handle first 6 arguments in registers
    size_t reg_args = std::min(args.size(), size_t(6));
    for (size_t i = 0; i < reg_args; i++) {
        if (args[i] != arg_registers[i]) {
            builder.mov(arg_registers[i], args[i]);
        }
    }
    
    // Handle remaining arguments on stack (right to left)
    for (size_t i = args.size(); i > 6; i--) {
        builder.push(args[i - 1]);
    }
    
    // Align stack to 16-byte boundary if odd number of stack args
    size_t stack_args = (args.size() > 6) ? args.size() - 6 : 0;
    if (stack_args % 2 == 1) {
        builder.sub(X86Reg::RSP, ImmediateOperand(8));
    }
}

void X86PatternBuilder::cleanup_function_call(size_t stack_bytes_used) {
    if (stack_bytes_used > 0) {
        builder.add(X86Reg::RSP, ImmediateOperand(static_cast<int64_t>(stack_bytes_used)));
    }
}

// =============================================================================
// Loop Patterns
// =============================================================================

void X86PatternBuilder::emit_counted_loop(X86Reg counter, const std::string& loop_body_label) {
    std::string loop_start = loop_body_label + "_start";
    std::string loop_end = loop_body_label + "_end";
    
    // test counter, counter
    builder.test(counter, counter);
    builder.jz(loop_end);
    
    // Loop start
    builder.resolve_label(loop_start, builder.get_current_position());
    
    // Decrement and test
    builder.sub(counter, ImmediateOperand(1));
    builder.jnz(loop_start);
    
    // Loop end
    builder.resolve_label(loop_end, builder.get_current_position());
}

void X86PatternBuilder::emit_memory_copy_loop(X86Reg dst, X86Reg src, X86Reg count) {
    // Optimized memory copy using rep movsb
    builder.mov(X86Reg::RDI, dst);
    builder.mov(X86Reg::RSI, src);
    builder.mov(X86Reg::RCX, count);
    
    // rep movsb
    builder.emit_byte(0xF3);  // REP prefix
    builder.emit_byte(0xA4);  // MOVSB
}

// =============================================================================
// Condition Evaluation Patterns
// =============================================================================

void X86PatternBuilder::emit_boolean_result(uint8_t condition_code, X86Reg result_reg) {
    // Set AL based on condition, then zero-extend to full register
    builder.setcc(condition_code, X86Reg::RAX);  // Sets AL
    builder.and_(X86Reg::RAX, ImmediateOperand(0xFF));  // Zero upper bits
    
    if (result_reg != X86Reg::RAX) {
        builder.mov(result_reg, X86Reg::RAX);
    }
}

void X86PatternBuilder::emit_three_way_comparison(X86Reg left, X86Reg right, X86Reg result) {
    // Compare and set result to -1, 0, or 1
    std::string less_label = "__cmp_less_" + std::to_string(builder.get_current_position());
    std::string greater_label = "__cmp_greater_" + std::to_string(builder.get_current_position());
    std::string end_label = "__cmp_end_" + std::to_string(builder.get_current_position());
    
    builder.cmp(left, right);
    builder.jl(less_label);
    builder.jg(greater_label);
    
    // Equal case
    builder.mov(result, ImmediateOperand(0));
    builder.jmp(end_label);
    
    // Less case
    builder.resolve_label(less_label, builder.get_current_position());
    builder.mov(result, ImmediateOperand(-1));
    builder.jmp(end_label);
    
    // Greater case
    builder.resolve_label(greater_label, builder.get_current_position());
    builder.mov(result, ImmediateOperand(1));
    
    // End
    builder.resolve_label(end_label, builder.get_current_position());
}

// =============================================================================
// Type Conversion Patterns
// =============================================================================

void X86PatternBuilder::emit_int_to_float_conversion(X86Reg int_reg, X86Reg float_reg) {
    // CVTSI2SD - Convert scalar integer to scalar double
    builder.emit_byte(0xF2);  // CVTSI2SD prefix
    if (static_cast<uint8_t>(float_reg) >= 8 || static_cast<uint8_t>(int_reg) >= 8) {
        uint8_t rex = 0x48;  // REX.W
        if (static_cast<uint8_t>(float_reg) >= 8) rex |= 0x04;  // REX.R
        if (static_cast<uint8_t>(int_reg) >= 8) rex |= 0x01;   // REX.B
        builder.emit_byte(rex);
    }
    builder.emit_byte(0x0F);
    builder.emit_byte(0x2A);
    builder.emit_byte(0xC0 | ((static_cast<uint8_t>(float_reg) & 7) << 3) | (static_cast<uint8_t>(int_reg) & 7));
}

void X86PatternBuilder::emit_float_to_int_conversion(X86Reg float_reg, X86Reg int_reg) {
    // CVTTSD2SI - Convert with truncation scalar double to scalar integer
    builder.emit_byte(0xF2);  // CVTTSD2SI prefix
    if (static_cast<uint8_t>(int_reg) >= 8 || static_cast<uint8_t>(float_reg) >= 8) {
        uint8_t rex = 0x48;  // REX.W
        if (static_cast<uint8_t>(int_reg) >= 8) rex |= 0x04;   // REX.R
        if (static_cast<uint8_t>(float_reg) >= 8) rex |= 0x01; // REX.B
        builder.emit_byte(rex);
    }
    builder.emit_byte(0x0F);
    builder.emit_byte(0x2C);
    builder.emit_byte(0xC0 | ((static_cast<uint8_t>(int_reg) & 7) << 3) | (static_cast<uint8_t>(float_reg) & 7));
}

// =============================================================================
// Stack Frame Management
// =============================================================================

void X86PatternBuilder::emit_function_prologue(size_t local_stack_size, const std::vector<X86Reg>& saved_regs) {
    // Standard function prologue
    builder.push(X86Reg::RBP);
    builder.mov(X86Reg::RBP, X86Reg::RSP);
    
    // Save callee-saved registers
    for (X86Reg reg : saved_regs) {
        builder.push(reg);
    }
    
    // Allocate local stack space
    if (local_stack_size > 0) {
        // Ensure 16-byte alignment
        size_t aligned_size = (local_stack_size + 15) & ~15;
        builder.sub(X86Reg::RSP, ImmediateOperand(static_cast<int64_t>(aligned_size)));
    }
}

void X86PatternBuilder::emit_function_epilogue(size_t local_stack_size, const std::vector<X86Reg>& saved_regs) {
    // Deallocate local stack space
    if (local_stack_size > 0) {
        size_t aligned_size = (local_stack_size + 15) & ~15;
        builder.add(X86Reg::RSP, ImmediateOperand(static_cast<int64_t>(aligned_size)));
    }
    
    // Restore callee-saved registers (in reverse order)
    for (auto it = saved_regs.rbegin(); it != saved_regs.rend(); ++it) {
        builder.pop(*it);
    }
    
    builder.pop(X86Reg::RBP);
    builder.ret();
}

// =============================================================================
// Error Handling Patterns
// =============================================================================

void X86PatternBuilder::emit_bounds_check(X86Reg index, X86Reg limit, const std::string& error_label) {
    builder.cmp(index, limit);
    builder.jge(error_label);  // Jump if index >= limit
}

void X86PatternBuilder::emit_null_check(X86Reg pointer, const std::string& error_label) {
    builder.test(pointer, pointer);
    builder.jz(error_label);  // Jump if pointer is null
}

// =============================================================================
// String Operation Patterns
// =============================================================================

void X86PatternBuilder::emit_string_length_calculation(X86Reg string_ptr, X86Reg result) {
    // Simple strlen implementation
    std::string loop_label = "__strlen_loop_" + std::to_string(builder.get_current_position());
    std::string end_label = "__strlen_end_" + std::to_string(builder.get_current_position());
    
    builder.mov(result, string_ptr);  // result = string_ptr
    
    // Loop: check for null terminator
    builder.resolve_label(loop_label, builder.get_current_position());
    // Check if character is null terminator - use temp register
    X86Reg temp = X86Reg::R10;  // Use R10 as temporary
    builder.mov(temp, MemoryOperand(result), OpSize::BYTE);
    builder.cmp(temp, ImmediateOperand(0));
    builder.jz(end_label);
    builder.add(result, ImmediateOperand(1));
    builder.jmp(loop_label);
    
    builder.resolve_label(end_label, builder.get_current_position());
    builder.sub(result, string_ptr);  // result = end - start
}

void X86PatternBuilder::emit_string_comparison(X86Reg str1, X86Reg str2, X86Reg result) {
    // String comparison using repe cmpsb
    builder.mov(X86Reg::RSI, str1);
    builder.mov(X86Reg::RDI, str2);
    
    // Find length of first string
    emit_string_length_calculation(str1, X86Reg::RCX);
    
    // Compare strings
    builder.emit_byte(0xF3);  // REPE prefix
    builder.emit_byte(0xA6);  // CMPSB
    
    // Set result based on flags
    emit_boolean_result(0x94, result);  // SETE
}

void X86PatternBuilder::emit_string_concatenation(X86Reg str1, X86Reg str2, X86Reg result) {
    // This would typically call a runtime function for dynamic allocation
    // For now, just demonstrate the pattern
    
    // Get lengths
    emit_string_length_calculation(str1, X86Reg::RCX);
    builder.push(X86Reg::RCX);  // Save str1 length
    
    emit_string_length_calculation(str2, X86Reg::RDX);
    builder.pop(X86Reg::RCX);   // Restore str1 length
    
    // Allocate memory (would call runtime function)
    builder.add(X86Reg::RCX, X86Reg::RDX);  // Total length
    builder.add(X86Reg::RCX, ImmediateOperand(1));  // +1 for null terminator
    
    // Call allocation function (placeholder)
    setup_function_call({X86Reg::RCX});
    builder.call("__allocate_string");
    cleanup_function_call(0);
    
    builder.mov(result, X86Reg::RAX);  // Result from allocation
}

// =============================================================================
// Array Operation Patterns
// =============================================================================

void X86PatternBuilder::emit_array_bounds_check(X86Reg array, X86Reg index) {
    // Load array length and compare with index
    builder.mov(X86Reg::RCX, MemoryOperand(array, 8));  // Assume length at offset 8
    emit_bounds_check(index, X86Reg::RCX, "__array_bounds_error");
}

void X86PatternBuilder::emit_typed_array_access(X86Reg array, X86Reg index, X86Reg result, OpSize element_size) {
    // Bounds check
    emit_array_bounds_check(array, index);
    
    // Calculate element address: array_data + index * element_size
    builder.mov(X86Reg::RCX, MemoryOperand(array));  // Load array data pointer
    
    if (element_size == OpSize::QWORD) {
        // LEA result, [rcx + index * 8]
        builder.lea(result, MemoryOperand(X86Reg::RCX, index, 8));
    } else {
        uint8_t scale = static_cast<uint8_t>(element_size);
        builder.lea(result, MemoryOperand(X86Reg::RCX, index, scale));
    }
    
    // Load the actual value
    builder.mov(result, MemoryOperand(result), element_size);
}

void X86PatternBuilder::emit_array_push_operation(X86Reg array, X86Reg value, OpSize element_size) {
    // Load current length
    builder.mov(X86Reg::RCX, MemoryOperand(array, 8));  // length at offset 8
    
    // Check capacity (assume capacity at offset 16)
    builder.mov(X86Reg::RDX, MemoryOperand(array, 16));
    builder.cmp(X86Reg::RCX, X86Reg::RDX);
    builder.jge("__array_resize");  // Need to resize if length >= capacity
    
    // Calculate address for new element
    builder.mov(X86Reg::RDX, MemoryOperand(array));  // data pointer
    uint8_t scale = static_cast<uint8_t>(element_size);
    builder.lea(X86Reg::RDX, MemoryOperand(X86Reg::RDX, X86Reg::RCX, scale));
    
    // Store the value
    builder.mov(MemoryOperand(X86Reg::RDX), value, element_size);
    
    // Increment length - load, increment, store
    X86Reg temp = X86Reg::R11;
    builder.mov(temp, MemoryOperand(array, 8));
    builder.add(temp, ImmediateOperand(1));
    builder.mov(MemoryOperand(array, 8), temp);
}


