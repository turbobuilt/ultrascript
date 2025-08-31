#pragma once

#include <vector>
#include <cstdint>
#include <string>
#include <unordered_map>



// X86-64 register enumeration
enum class X86Reg : uint8_t {
    RAX = 0, RCX = 1, RDX = 2, RBX = 3,
    RSP = 4, RBP = 5, RSI = 6, RDI = 7,
    R8 = 8, R9 = 9, R10 = 10, R11 = 11,
    R12 = 12, R13 = 13, R14 = 14, R15 = 15,
    
    // Special markers
    NONE = 255
};

// X86-64 XMM register enumeration for floating-point operations
enum class X86XmmReg : uint8_t {
    XMM0 = 0, XMM1 = 1, XMM2 = 2, XMM3 = 3,
    XMM4 = 4, XMM5 = 5, XMM6 = 6, XMM7 = 7,
    XMM8 = 8, XMM9 = 9, XMM10 = 10, XMM11 = 11,
    XMM12 = 12, XMM13 = 13, XMM14 = 14, XMM15 = 15,
    
    // Special markers
    NONE = 255
};

// Operand size enumeration
enum class OpSize : uint8_t {
    BYTE = 1,
    WORD = 2,
    DWORD = 4,
    QWORD = 8
};

// Memory addressing modes
struct MemoryOperand {
    X86Reg base = X86Reg::NONE;
    X86Reg index = X86Reg::NONE;
    uint8_t scale = 1;  // 1, 2, 4, or 8
    int32_t displacement = 0;
    bool rip_relative = false;
    
    MemoryOperand() = default;
    MemoryOperand(X86Reg base_reg, int32_t disp = 0) 
        : base(base_reg), displacement(disp) {}
    MemoryOperand(X86Reg base_reg, X86Reg index_reg, uint8_t scale_val = 1, int32_t disp = 0)
        : base(base_reg), index(index_reg), scale(scale_val), displacement(disp) {}
};

// Immediate operand
struct ImmediateOperand {
    int64_t value;
    OpSize size;
    
    ImmediateOperand(int64_t val, OpSize sz = OpSize::QWORD) : value(val), size(sz) {}
    ImmediateOperand(int32_t val) : value(val), size(OpSize::DWORD) {}
    ImmediateOperand(int16_t val) : value(val), size(OpSize::WORD) {}
    ImmediateOperand(int8_t val) : value(val), size(OpSize::BYTE) {}
};

// Abstract instruction builder with validation and optimization
class X86InstructionBuilder {
private:
    std::vector<uint8_t>& code_buffer;
    
    // Instruction encoding helpers
    bool needs_rex_prefix(X86Reg reg1, X86Reg reg2 = X86Reg::NONE, OpSize size = OpSize::QWORD) const;
    bool needs_rex_prefix_xmm(X86XmmReg xmm_reg, X86Reg gpr_reg = X86Reg::NONE) const;
    uint8_t compute_rex_prefix(bool w, bool r, bool x, bool b) const;
    uint8_t compute_modrm(uint8_t mod, uint8_t reg, uint8_t rm) const;
    uint8_t compute_sib(uint8_t scale, uint8_t index, uint8_t base) const;
    
    void emit_rex_if_needed(X86Reg reg1, X86Reg reg2 = X86Reg::NONE, OpSize size = OpSize::QWORD);
    void emit_rex_if_needed_xmm(X86XmmReg xmm_reg, X86Reg gpr_reg = X86Reg::NONE, bool w = false);
    void emit_modrm_sib_disp(uint8_t reg_field, const MemoryOperand& mem);
    void emit_immediate(const ImmediateOperand& imm);
    
    // Validation helpers
    bool is_valid_scale(uint8_t scale) const { return scale == 1 || scale == 2 || scale == 4 || scale == 8; }
    bool is_valid_displacement_size(int32_t disp, bool has_base) const;
    bool requires_sib_byte(const MemoryOperand& mem) const;
    
public:
    explicit X86InstructionBuilder(std::vector<uint8_t>& buffer) : code_buffer(buffer) {}
    
    // High-level instruction builders with automatic encoding
    void mov(X86Reg dst, X86Reg src, OpSize size = OpSize::QWORD);
    void mov(X86Reg dst, const ImmediateOperand& imm);
    void mov(X86Reg dst, const MemoryOperand& src, OpSize size = OpSize::QWORD);
    void mov(const MemoryOperand& dst, X86Reg src, OpSize size = OpSize::QWORD);
    void mov(const MemoryOperand& dst, const ImmediateOperand& imm, OpSize size = OpSize::QWORD);
    
    void add(X86Reg dst, X86Reg src, OpSize size = OpSize::QWORD);
    void add(X86Reg dst, const ImmediateOperand& imm);
    void add(X86Reg dst, const MemoryOperand& src, OpSize size = OpSize::QWORD);
    
    void sub(X86Reg dst, X86Reg src, OpSize size = OpSize::QWORD);
    void sub(X86Reg dst, const ImmediateOperand& imm);
    void sub(X86Reg dst, const MemoryOperand& src, OpSize size = OpSize::QWORD);
    
    void imul(X86Reg dst, X86Reg src, OpSize size = OpSize::QWORD);
    void idiv(X86Reg divisor, OpSize size = OpSize::QWORD);
    
    void cmp(X86Reg left, X86Reg right, OpSize size = OpSize::QWORD);
    void cmp(X86Reg left, const ImmediateOperand& right);
    void cmp(X86Reg left, const MemoryOperand& right, OpSize size = OpSize::QWORD);
    
    void test(X86Reg left, X86Reg right, OpSize size = OpSize::QWORD);
    void test(X86Reg reg, const ImmediateOperand& imm);
    
    // Conditional set instructions
    void setcc(uint8_t condition_code, X86Reg dst);
    void setz(X86Reg dst) { setcc(0x94, dst); }   // SETE
    void setnz(X86Reg dst) { setcc(0x95, dst); }  // SETNE
    void setl(X86Reg dst) { setcc(0x9C, dst); }   // SETL
    void setg(X86Reg dst) { setcc(0x9F, dst); }   // SETG
    void setle(X86Reg dst) { setcc(0x9E, dst); }  // SETLE
    void setge(X86Reg dst) { setcc(0x9D, dst); }  // SETGE
    
    // Jump instructions
    void jmp(const std::string& label);
    void jmp(int32_t relative_offset);
    void jcc(uint8_t condition_code, const std::string& label);
    void jcc(uint8_t condition_code, int32_t relative_offset);
    void jz(const std::string& label) { jcc(0x84, label); }
    void jnz(const std::string& label) { jcc(0x85, label); }
    void jl(const std::string& label) { jcc(0x8C, label); }
    void jg(const std::string& label) { jcc(0x8F, label); }
    void jle(const std::string& label) { jcc(0x8E, label); }
    void jge(const std::string& label) { jcc(0x8D, label); }
    
    // Call and return
    void call(X86Reg target);
    void call(const MemoryOperand& target);
    void call(const std::string& label);
    void call(void* function_ptr);
    void ret();
    
    // Stack operations
    void push(X86Reg reg);
    void push(const ImmediateOperand& imm);
    void push(const MemoryOperand& mem);
    void pop(X86Reg reg);
    void pop(const MemoryOperand& mem);
    
    // Logical operations
    void and_(X86Reg dst, X86Reg src, OpSize size = OpSize::QWORD);
    void and_(X86Reg dst, const ImmediateOperand& imm);
    void or_(X86Reg dst, X86Reg src, OpSize size = OpSize::QWORD);
    void or_(X86Reg dst, const ImmediateOperand& imm);
    void xor_(X86Reg dst, X86Reg src, OpSize size = OpSize::QWORD);
    void xor_(X86Reg dst, const ImmediateOperand& imm);
    
    // Bit manipulation
    void shl(X86Reg dst, const ImmediateOperand& count);
    void shr(X86Reg dst, const ImmediateOperand& count);
    void sar(X86Reg dst, const ImmediateOperand& count);
    void rol(X86Reg dst, const ImmediateOperand& count);
    void ror(X86Reg dst, const ImmediateOperand& count);
    
    // Advanced instructions
    void lea(X86Reg dst, const MemoryOperand& src);
    void cdq();  // Sign extend EAX into EDX:EAX
    void cqo();  // Sign extend RAX into RDX:RAX
    
    // SIMD operations (basic)
    void movdqa(X86Reg dst, X86Reg src);  // SSE2 aligned move
    void movdqu(X86Reg dst, const MemoryOperand& src);  // SSE2 unaligned move
    
    // XMM register operations for high-performance floating-point
    void movq(X86XmmReg dst, X86Reg src);  // Move 64-bit from GPR to XMM
    void movq(X86Reg dst, X86XmmReg src);  // Move 64-bit from XMM to GPR
    void movsd(X86XmmReg dst, X86XmmReg src);  // Move scalar double-precision
    void movsd(X86XmmReg dst, const MemoryOperand& src);  // Load scalar double
    void movsd(const MemoryOperand& dst, X86XmmReg src);  // Store scalar double
    void movapd(X86XmmReg dst, X86XmmReg src);  // Move aligned packed double
    void cvtsi2sd(X86XmmReg dst, X86Reg src);  // Convert int64 to double
    void cvtsd2si(X86Reg dst, X86XmmReg src);  // Convert double to int64
    
    // Atomic operations
    void lock_prefix();
    void cmpxchg(const MemoryOperand& dst, X86Reg src, OpSize size = OpSize::QWORD);
    void xadd(const MemoryOperand& dst, X86Reg src, OpSize size = OpSize::QWORD);
    
    // Atomic increment/decrement operations for reference counting
    void inc(X86Reg dst, OpSize size = OpSize::QWORD);
    void inc(const MemoryOperand& dst, OpSize size = OpSize::QWORD);
    void dec(X86Reg dst, OpSize size = OpSize::QWORD);
    void dec(const MemoryOperand& dst, OpSize size = OpSize::QWORD);
    void lock_inc(const MemoryOperand& dst, OpSize size = OpSize::QWORD);
    void lock_dec(const MemoryOperand& dst, OpSize size = OpSize::QWORD);
    void lock_add(const MemoryOperand& dst, const ImmediateOperand& imm, OpSize size = OpSize::QWORD);
    void lock_xadd(const MemoryOperand& dst, X86Reg src, OpSize size = OpSize::QWORD);
    
    // Memory barriers
    void mfence();  // Full memory barrier
    void lfence();  // Load fence
    void sfence();  // Store fence
    
    // No-op and debugging
    void nop();
    void int3();  // Debug breakpoint
    
    // Label management - instance-based for thread safety and reliability
    void emit_label_placeholder(const std::string& label);
    void resolve_label(const std::string& label, size_t address);
    void clear_label_state();  // Clear all label state for new compilation
    bool validate_all_labels_resolved() const;  // Validation before execution
    
    // Direct byte emission for special cases
    void emit_byte(uint8_t byte) { code_buffer.push_back(byte); }
    void emit_bytes(const std::vector<uint8_t>& bytes);
    
    // Validation and optimization
    bool validate_instruction_stream() const;
    void optimize_instruction_sequence();  // Peephole optimizations
    
    // Get current position for label resolution
    size_t get_current_position() const { return code_buffer.size(); }
    
    // Utility methods for instruction length tracking and patching
    size_t get_last_instruction_length() const { return last_instruction_length_; }
    
    // Robust patching API - returns exact offset where immediate field was placed
    struct PatchInfo {
        size_t immediate_offset;    // Exact byte offset where immediate field is located
        size_t instruction_length;  // Total length of the instruction
        size_t immediate_size;      // Size of immediate field (4 or 8 bytes)
    };
    
    // Enhanced MOV with patch information
    PatchInfo mov_with_patch_info(X86Reg dst, const ImmediateOperand& imm);
    
    // Dedicated function address MOV - ALWAYS uses 64-bit immediate for function pointers
    PatchInfo mov_function_address(X86Reg dst, uint64_t placeholder_address = 0);

private:
    // Instance-based label management for thread safety and reliability
    std::unordered_map<std::string, size_t> label_addresses_;
    std::unordered_map<std::string, std::vector<size_t>> unresolved_labels_;
    
    // Instruction length tracking
    mutable size_t last_instruction_length_ = 0;
    mutable size_t instruction_start_pos_ = 0;
    
    // Helper method
    void mark_instruction_start() { instruction_start_pos_ = code_buffer.size(); }
};

// High-level instruction patterns for common operations
class X86PatternBuilder {
private:
    X86InstructionBuilder& builder;
    
public:
    explicit X86PatternBuilder(X86InstructionBuilder& instr_builder) : builder(instr_builder) {}
    
    // Function call patterns
    void setup_function_call(const std::vector<X86Reg>& args);
    void cleanup_function_call(size_t stack_bytes_used);
    
    // Loop patterns
    void emit_counted_loop(X86Reg counter, const std::string& loop_body_label);
    void emit_memory_copy_loop(X86Reg dst, X86Reg src, X86Reg count);
    
    // Condition evaluation patterns
    void emit_boolean_result(uint8_t condition_code, X86Reg result_reg);
    void emit_three_way_comparison(X86Reg left, X86Reg right, X86Reg result);
    
    // Type conversion patterns
    void emit_int_to_float_conversion(X86Reg int_reg, X86Reg float_reg);
    void emit_float_to_int_conversion(X86Reg float_reg, X86Reg int_reg);
    
    // Stack frame management
    void emit_function_prologue(size_t local_stack_size, const std::vector<X86Reg>& saved_regs);
    void emit_function_epilogue(size_t local_stack_size, const std::vector<X86Reg>& saved_regs);
    
    // Error handling patterns
    void emit_bounds_check(X86Reg index, X86Reg limit, const std::string& error_label);
    void emit_null_check(X86Reg pointer, const std::string& error_label);
    
    // String operation patterns
    void emit_string_length_calculation(X86Reg string_ptr, X86Reg result);
    void emit_string_comparison(X86Reg str1, X86Reg str2, X86Reg result);
    void emit_string_concatenation(X86Reg str1, X86Reg str2, X86Reg result);
    
    // Array operation patterns
    void emit_array_bounds_check(X86Reg array, X86Reg index);
    void emit_typed_array_access(X86Reg array, X86Reg index, X86Reg result, OpSize element_size);
    void emit_array_push_operation(X86Reg array, X86Reg value, OpSize element_size);
};


