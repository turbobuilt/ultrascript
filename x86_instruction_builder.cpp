#include "x86_instruction_builder.h"
#include <cassert>
#include <algorithm>
#include <cstring>

namespace ultraScript {

// =============================================================================
// X86InstructionBuilder Implementation
// =============================================================================

// Helper functions for instruction encoding
bool X86InstructionBuilder::needs_rex_prefix(X86Reg reg1, X86Reg reg2, OpSize size) const {
    return size == OpSize::QWORD || 
           static_cast<uint8_t>(reg1) >= 8 || 
           (reg2 != X86Reg::NONE && static_cast<uint8_t>(reg2) >= 8);
}

uint8_t X86InstructionBuilder::compute_rex_prefix(bool w, bool r, bool x, bool b) const {
    return 0x40 | (w ? 8 : 0) | (r ? 4 : 0) | (x ? 2 : 0) | (b ? 1 : 0);
}

uint8_t X86InstructionBuilder::compute_modrm(uint8_t mod, uint8_t reg, uint8_t rm) const {
    return (mod << 6) | ((reg & 7) << 3) | (rm & 7);
}

uint8_t X86InstructionBuilder::compute_sib(uint8_t scale, uint8_t index, uint8_t base) const {
    uint8_t scale_bits = 0;
    switch (scale) {
        case 1: scale_bits = 0; break;
        case 2: scale_bits = 1; break;
        case 4: scale_bits = 2; break;
        case 8: scale_bits = 3; break;
        default: assert(false && "Invalid scale"); break;
    }
    return (scale_bits << 6) | ((index & 7) << 3) | (base & 7);
}

void X86InstructionBuilder::emit_rex_if_needed(X86Reg reg1, X86Reg reg2, OpSize size) {
    if (needs_rex_prefix(reg1, reg2, size)) {
        bool w = (size == OpSize::QWORD);
        bool r = static_cast<uint8_t>(reg1) >= 8;
        bool x = false;  // Set by caller for index registers
        bool b = (reg2 != X86Reg::NONE) && (static_cast<uint8_t>(reg2) >= 8);
        code_buffer.push_back(compute_rex_prefix(w, r, x, b));
    }
}

bool X86InstructionBuilder::is_valid_displacement_size(int32_t disp, bool has_base) const {
    if (!has_base && disp == 0) return false;  // Need either base or displacement
    return true;  // All displacement sizes are valid in x86-64
}

bool X86InstructionBuilder::requires_sib_byte(const MemoryOperand& mem) const {
    return mem.index != X86Reg::NONE || 
           mem.base == X86Reg::RSP || 
           mem.base == X86Reg::R12;
}

void X86InstructionBuilder::emit_modrm_sib_disp(uint8_t reg_field, const MemoryOperand& mem) {
    uint8_t mod, rm;
    bool needs_sib = requires_sib_byte(mem);
    
    // Determine addressing mode
    if (mem.displacement == 0 && mem.base != X86Reg::RBP && mem.base != X86Reg::R13) {
        mod = 0;  // No displacement
    } else if (mem.displacement >= -128 && mem.displacement <= 127) {
        mod = 1;  // 8-bit displacement
    } else {
        mod = 2;  // 32-bit displacement
    }
    
    if (needs_sib) {
        rm = 4;  // Indicates SIB byte follows
        code_buffer.push_back(compute_modrm(mod, reg_field, rm));
        
        uint8_t index = (mem.index != X86Reg::NONE) ? static_cast<uint8_t>(mem.index) : 4;
        uint8_t base = static_cast<uint8_t>(mem.base);
        code_buffer.push_back(compute_sib(mem.scale, index, base));
    } else {
        rm = static_cast<uint8_t>(mem.base);
        code_buffer.push_back(compute_modrm(mod, reg_field, rm));
    }
    
    // Emit displacement
    if (mod == 1) {
        code_buffer.push_back(static_cast<uint8_t>(mem.displacement));
    } else if (mod == 2) {
        uint32_t disp = static_cast<uint32_t>(mem.displacement);
        code_buffer.push_back(disp & 0xFF);
        code_buffer.push_back((disp >> 8) & 0xFF);
        code_buffer.push_back((disp >> 16) & 0xFF);
        code_buffer.push_back((disp >> 24) & 0xFF);
    }
}

void X86InstructionBuilder::emit_immediate(const ImmediateOperand& imm) {
    switch (imm.size) {
        case OpSize::BYTE:
            code_buffer.push_back(static_cast<uint8_t>(imm.value));
            break;
        case OpSize::WORD:
            code_buffer.push_back(imm.value & 0xFF);
            code_buffer.push_back((imm.value >> 8) & 0xFF);
            break;
        case OpSize::DWORD:
            for (int i = 0; i < 4; i++) {
                code_buffer.push_back((imm.value >> (i * 8)) & 0xFF);
            }
            break;
        case OpSize::QWORD:
            for (int i = 0; i < 8; i++) {
                code_buffer.push_back((imm.value >> (i * 8)) & 0xFF);
            }
            break;
    }
}

// =============================================================================
// MOV Instructions
// =============================================================================

void X86InstructionBuilder::mov(X86Reg dst, X86Reg src, OpSize size) {
    emit_rex_if_needed(dst, src, size);
    
    if (size == OpSize::BYTE) {
        code_buffer.push_back(0x88);  // MOV r/m8, r8
    } else {
        code_buffer.push_back(0x89);  // MOV r/m, r
    }
    
    code_buffer.push_back(compute_modrm(3, 
        static_cast<uint8_t>(src) & 7, 
        static_cast<uint8_t>(dst) & 7));
}

void X86InstructionBuilder::mov(X86Reg dst, const ImmediateOperand& imm) {
    if (imm.size == OpSize::QWORD && 
        imm.value >= -2147483648LL && imm.value <= 2147483647LL) {
        // Use 32-bit immediate that gets sign-extended
        emit_rex_if_needed(dst, X86Reg::NONE, OpSize::QWORD);
        code_buffer.push_back(0xC7);  // MOV r/m64, imm32
        code_buffer.push_back(0xC0 | (static_cast<uint8_t>(dst) & 7));
        emit_immediate(ImmediateOperand(static_cast<int32_t>(imm.value)));
    } else {
        // Use full 64-bit immediate
        uint8_t rex = 0x48;  // REX.W
        if (static_cast<uint8_t>(dst) >= 8) rex |= 1;  // REX.B
        code_buffer.push_back(rex);
        code_buffer.push_back(0xB8 | (static_cast<uint8_t>(dst) & 7));  // MOV r64, imm64
        emit_immediate(imm);
    }
}

void X86InstructionBuilder::mov(X86Reg dst, const MemoryOperand& src, OpSize size) {
    emit_rex_if_needed(dst, src.base, size);
    
    if (size == OpSize::BYTE) {
        code_buffer.push_back(0x8A);  // MOV r8, r/m8
    } else {
        code_buffer.push_back(0x8B);  // MOV r, r/m
    }
    
    emit_modrm_sib_disp(static_cast<uint8_t>(dst) & 7, src);
}

void X86InstructionBuilder::mov(const MemoryOperand& dst, X86Reg src, OpSize size) {
    emit_rex_if_needed(src, dst.base, size);
    
    if (size == OpSize::BYTE) {
        code_buffer.push_back(0x88);  // MOV r/m8, r8
    } else {
        code_buffer.push_back(0x89);  // MOV r/m, r
    }
    
    emit_modrm_sib_disp(static_cast<uint8_t>(src) & 7, dst);
}

void X86InstructionBuilder::mov(const MemoryOperand& dst, const ImmediateOperand& imm, OpSize size) {
    emit_rex_if_needed(X86Reg::NONE, dst.base, size);
    
    if (size == OpSize::BYTE) {
        code_buffer.push_back(0xC6);  // MOV r/m8, imm8
        emit_modrm_sib_disp(0, dst);
        emit_immediate(ImmediateOperand(static_cast<int8_t>(imm.value)));
    } else {
        code_buffer.push_back(0xC7);  // MOV r/m, imm32
        emit_modrm_sib_disp(0, dst);
        emit_immediate(ImmediateOperand(static_cast<int32_t>(imm.value)));
    }
}

// =============================================================================
// Arithmetic Instructions
// =============================================================================

void X86InstructionBuilder::add(X86Reg dst, X86Reg src, OpSize size) {
    emit_rex_if_needed(dst, src, size);
    code_buffer.push_back(0x01);  // ADD r/m, r
    code_buffer.push_back(compute_modrm(3, 
        static_cast<uint8_t>(src) & 7, 
        static_cast<uint8_t>(dst) & 7));
}

void X86InstructionBuilder::add(X86Reg dst, const ImmediateOperand& imm) {
    if (dst == X86Reg::RAX && imm.size == OpSize::DWORD) {
        code_buffer.push_back(0x48);  // REX.W
        code_buffer.push_back(0x05);  // ADD RAX, imm32
        emit_immediate(imm);
    } else if (imm.value >= -128 && imm.value <= 127) {
        emit_rex_if_needed(dst, X86Reg::NONE, OpSize::QWORD);
        code_buffer.push_back(0x83);  // ADD r/m, imm8
        code_buffer.push_back(0xC0 | (static_cast<uint8_t>(dst) & 7));
        code_buffer.push_back(static_cast<uint8_t>(imm.value));
    } else {
        emit_rex_if_needed(dst, X86Reg::NONE, OpSize::QWORD);
        code_buffer.push_back(0x81);  // ADD r/m, imm32
        code_buffer.push_back(0xC0 | (static_cast<uint8_t>(dst) & 7));
        emit_immediate(ImmediateOperand(static_cast<int32_t>(imm.value)));
    }
}

void X86InstructionBuilder::add(X86Reg dst, const MemoryOperand& src, OpSize size) {
    emit_rex_if_needed(dst, src.base, size);
    code_buffer.push_back(0x03);  // ADD r, r/m
    emit_modrm_sib_disp(static_cast<uint8_t>(dst) & 7, src);
}

void X86InstructionBuilder::sub(X86Reg dst, X86Reg src, OpSize size) {
    emit_rex_if_needed(dst, src, size);
    code_buffer.push_back(0x29);  // SUB r/m, r
    code_buffer.push_back(compute_modrm(3, 
        static_cast<uint8_t>(src) & 7, 
        static_cast<uint8_t>(dst) & 7));
}

void X86InstructionBuilder::sub(X86Reg dst, const ImmediateOperand& imm) {
    if (dst == X86Reg::RAX && imm.size == OpSize::DWORD) {
        code_buffer.push_back(0x48);  // REX.W
        code_buffer.push_back(0x2D);  // SUB RAX, imm32
        emit_immediate(imm);
    } else if (imm.value >= -128 && imm.value <= 127) {
        emit_rex_if_needed(dst, X86Reg::NONE, OpSize::QWORD);
        code_buffer.push_back(0x83);  // SUB r/m, imm8
        code_buffer.push_back(0xE8 | (static_cast<uint8_t>(dst) & 7));
        code_buffer.push_back(static_cast<uint8_t>(imm.value));
    } else {
        emit_rex_if_needed(dst, X86Reg::NONE, OpSize::QWORD);
        code_buffer.push_back(0x81);  // SUB r/m, imm32
        code_buffer.push_back(0xE8 | (static_cast<uint8_t>(dst) & 7));
        emit_immediate(ImmediateOperand(static_cast<int32_t>(imm.value)));
    }
}

void X86InstructionBuilder::sub(X86Reg dst, const MemoryOperand& src, OpSize size) {
    emit_rex_if_needed(dst, src.base, size);
    code_buffer.push_back(0x2B);  // SUB r, r/m
    emit_modrm_sib_disp(static_cast<uint8_t>(dst) & 7, src);
}

void X86InstructionBuilder::imul(X86Reg dst, X86Reg src, OpSize size) {
    emit_rex_if_needed(dst, src, size);
    code_buffer.push_back(0x0F);  // Two-byte opcode prefix
    code_buffer.push_back(0xAF);  // IMUL r, r/m
    code_buffer.push_back(compute_modrm(3, 
        static_cast<uint8_t>(dst) & 7, 
        static_cast<uint8_t>(src) & 7));
}

void X86InstructionBuilder::idiv(X86Reg divisor, OpSize size) {
    emit_rex_if_needed(X86Reg::NONE, divisor, size);
    code_buffer.push_back(0xF7);  // IDIV r/m
    code_buffer.push_back(0xF8 | (static_cast<uint8_t>(divisor) & 7));
}

// =============================================================================
// Compare and Test Instructions
// =============================================================================

void X86InstructionBuilder::cmp(X86Reg left, X86Reg right, OpSize size) {
    emit_rex_if_needed(left, right, size);
    code_buffer.push_back(0x39);  // CMP r/m, r
    code_buffer.push_back(compute_modrm(3, 
        static_cast<uint8_t>(right) & 7, 
        static_cast<uint8_t>(left) & 7));
}

void X86InstructionBuilder::cmp(X86Reg left, const ImmediateOperand& right) {
    if (left == X86Reg::RAX && right.size == OpSize::DWORD) {
        code_buffer.push_back(0x48);  // REX.W
        code_buffer.push_back(0x3D);  // CMP RAX, imm32
        emit_immediate(right);
    } else if (right.value >= -128 && right.value <= 127) {
        emit_rex_if_needed(left, X86Reg::NONE, OpSize::QWORD);
        code_buffer.push_back(0x83);  // CMP r/m, imm8
        code_buffer.push_back(0xF8 | (static_cast<uint8_t>(left) & 7));
        code_buffer.push_back(static_cast<uint8_t>(right.value));
    } else {
        emit_rex_if_needed(left, X86Reg::NONE, OpSize::QWORD);
        code_buffer.push_back(0x81);  // CMP r/m, imm32
        code_buffer.push_back(0xF8 | (static_cast<uint8_t>(left) & 7));
        emit_immediate(ImmediateOperand(static_cast<int32_t>(right.value)));
    }
}

void X86InstructionBuilder::cmp(X86Reg left, const MemoryOperand& right, OpSize size) {
    emit_rex_if_needed(left, right.base, size);
    code_buffer.push_back(0x3B);  // CMP r, r/m
    emit_modrm_sib_disp(static_cast<uint8_t>(left) & 7, right);
}

void X86InstructionBuilder::test(X86Reg left, X86Reg right, OpSize size) {
    emit_rex_if_needed(left, right, size);
    code_buffer.push_back(0x85);  // TEST r/m, r
    code_buffer.push_back(compute_modrm(3, 
        static_cast<uint8_t>(right) & 7, 
        static_cast<uint8_t>(left) & 7));
}

void X86InstructionBuilder::test(X86Reg reg, const ImmediateOperand& imm) {
    if (reg == X86Reg::RAX) {
        emit_rex_if_needed(reg, X86Reg::NONE, OpSize::QWORD);
        code_buffer.push_back(0xA9);  // TEST RAX, imm32
        emit_immediate(ImmediateOperand(static_cast<int32_t>(imm.value)));
    } else {
        emit_rex_if_needed(reg, X86Reg::NONE, OpSize::QWORD);
        code_buffer.push_back(0xF7);  // TEST r/m, imm32
        code_buffer.push_back(0xC0 | (static_cast<uint8_t>(reg) & 7));
        emit_immediate(ImmediateOperand(static_cast<int32_t>(imm.value)));
    }
}

// =============================================================================
// Conditional Set Instructions
// =============================================================================

void X86InstructionBuilder::setcc(uint8_t condition_code, X86Reg dst) {
    if (static_cast<uint8_t>(dst) >= 8) {
        code_buffer.push_back(0x41);  // REX.B
    }
    code_buffer.push_back(0x0F);
    code_buffer.push_back(condition_code);
    code_buffer.push_back(0xC0 | (static_cast<uint8_t>(dst) & 7));
}

// =============================================================================
// Jump Instructions  
// =============================================================================

void X86InstructionBuilder::jmp(const std::string& label) {
    code_buffer.push_back(0xE9);  // JMP rel32
    // Store placeholder for label resolution
    emit_label_placeholder(label);
}

void X86InstructionBuilder::jmp(int32_t relative_offset) {
    if (relative_offset >= -128 && relative_offset <= 127) {
        code_buffer.push_back(0xEB);  // JMP rel8
        code_buffer.push_back(static_cast<uint8_t>(relative_offset));
    } else {
        code_buffer.push_back(0xE9);  // JMP rel32
        for (int i = 0; i < 4; i++) {
            code_buffer.push_back((relative_offset >> (i * 8)) & 0xFF);
        }
    }
}

void X86InstructionBuilder::jcc(uint8_t condition_code, const std::string& label) {
    code_buffer.push_back(0x0F);
    code_buffer.push_back(condition_code);
    emit_label_placeholder(label);
}

void X86InstructionBuilder::jcc(uint8_t condition_code, int32_t relative_offset) {
    if (relative_offset >= -128 && relative_offset <= 127) {
        code_buffer.push_back(condition_code - 0x10);  // Short form
        code_buffer.push_back(static_cast<uint8_t>(relative_offset));
    } else {
        code_buffer.push_back(0x0F);
        code_buffer.push_back(condition_code);
        for (int i = 0; i < 4; i++) {
            code_buffer.push_back((relative_offset >> (i * 8)) & 0xFF);
        }
    }
}

// =============================================================================
// Call and Return Instructions
// =============================================================================

void X86InstructionBuilder::call(X86Reg target) {
    if (static_cast<uint8_t>(target) >= 8) {
        code_buffer.push_back(0x41);  // REX.B
    }
    code_buffer.push_back(0xFF);  // CALL r/m
    code_buffer.push_back(0xD0 | (static_cast<uint8_t>(target) & 7));
}

void X86InstructionBuilder::call(const MemoryOperand& target) {
    emit_rex_if_needed(X86Reg::NONE, target.base, OpSize::QWORD);
    code_buffer.push_back(0xFF);  // CALL r/m
    emit_modrm_sib_disp(2, target);  // /2 for CALL
}

void X86InstructionBuilder::call(const std::string& label) {
    code_buffer.push_back(0xE8);  // CALL rel32
    emit_label_placeholder(label);
}

void X86InstructionBuilder::call(void* function_ptr) {
    // MOV RAX, function_ptr; CALL RAX
    mov(X86Reg::RAX, ImmediateOperand(reinterpret_cast<int64_t>(function_ptr)));
    call(X86Reg::RAX);
}

void X86InstructionBuilder::ret() {
    code_buffer.push_back(0xC3);  // RET
}

// =============================================================================
// Stack Operations
// =============================================================================

void X86InstructionBuilder::push(X86Reg reg) {
    if (static_cast<uint8_t>(reg) >= 8) {
        code_buffer.push_back(0x41);  // REX.B
    }
    code_buffer.push_back(0x50 | (static_cast<uint8_t>(reg) & 7));
}

void X86InstructionBuilder::push(const ImmediateOperand& imm) {
    if (imm.value >= -128 && imm.value <= 127) {
        code_buffer.push_back(0x6A);  // PUSH imm8
        code_buffer.push_back(static_cast<uint8_t>(imm.value));
    } else {
        code_buffer.push_back(0x68);  // PUSH imm32
        emit_immediate(ImmediateOperand(static_cast<int32_t>(imm.value)));
    }
}

void X86InstructionBuilder::push(const MemoryOperand& mem) {
    emit_rex_if_needed(X86Reg::NONE, mem.base, OpSize::QWORD);
    code_buffer.push_back(0xFF);  // PUSH r/m
    emit_modrm_sib_disp(6, mem);  // /6 for PUSH
}

void X86InstructionBuilder::pop(X86Reg reg) {
    if (static_cast<uint8_t>(reg) >= 8) {
        code_buffer.push_back(0x41);  // REX.B
    }
    code_buffer.push_back(0x58 | (static_cast<uint8_t>(reg) & 7));
}

void X86InstructionBuilder::pop(const MemoryOperand& mem) {
    emit_rex_if_needed(X86Reg::NONE, mem.base, OpSize::QWORD);
    code_buffer.push_back(0x8F);  // POP r/m
    emit_modrm_sib_disp(0, mem);  // /0 for POP
}

// =============================================================================
// Logical Operations
// =============================================================================

void X86InstructionBuilder::and_(X86Reg dst, X86Reg src, OpSize size) {
    emit_rex_if_needed(dst, src, size);
    code_buffer.push_back(0x21);  // AND r/m, r
    code_buffer.push_back(compute_modrm(3, 
        static_cast<uint8_t>(src) & 7, 
        static_cast<uint8_t>(dst) & 7));
}

void X86InstructionBuilder::and_(X86Reg dst, const ImmediateOperand& imm) {
    if (dst == X86Reg::RAX) {
        emit_rex_if_needed(dst, X86Reg::NONE, OpSize::QWORD);
        code_buffer.push_back(0x25);  // AND RAX, imm32
        emit_immediate(ImmediateOperand(static_cast<int32_t>(imm.value)));
    } else if (imm.value >= -128 && imm.value <= 127) {
        emit_rex_if_needed(dst, X86Reg::NONE, OpSize::QWORD);
        code_buffer.push_back(0x83);  // AND r/m, imm8
        code_buffer.push_back(0xE0 | (static_cast<uint8_t>(dst) & 7));
        code_buffer.push_back(static_cast<uint8_t>(imm.value));
    } else {
        emit_rex_if_needed(dst, X86Reg::NONE, OpSize::QWORD);
        code_buffer.push_back(0x81);  // AND r/m, imm32
        code_buffer.push_back(0xE0 | (static_cast<uint8_t>(dst) & 7));
        emit_immediate(ImmediateOperand(static_cast<int32_t>(imm.value)));
    }
}

void X86InstructionBuilder::or_(X86Reg dst, X86Reg src, OpSize size) {
    emit_rex_if_needed(dst, src, size);
    code_buffer.push_back(0x09);  // OR r/m, r
    code_buffer.push_back(compute_modrm(3, 
        static_cast<uint8_t>(src) & 7, 
        static_cast<uint8_t>(dst) & 7));
}

void X86InstructionBuilder::or_(X86Reg dst, const ImmediateOperand& imm) {
    if (dst == X86Reg::RAX) {
        emit_rex_if_needed(dst, X86Reg::NONE, OpSize::QWORD);
        code_buffer.push_back(0x0D);  // OR RAX, imm32
        emit_immediate(ImmediateOperand(static_cast<int32_t>(imm.value)));
    } else if (imm.value >= -128 && imm.value <= 127) {
        emit_rex_if_needed(dst, X86Reg::NONE, OpSize::QWORD);
        code_buffer.push_back(0x83);  // OR r/m, imm8
        code_buffer.push_back(0xC8 | (static_cast<uint8_t>(dst) & 7));
        code_buffer.push_back(static_cast<uint8_t>(imm.value));
    } else {
        emit_rex_if_needed(dst, X86Reg::NONE, OpSize::QWORD);
        code_buffer.push_back(0x81);  // OR r/m, imm32
        code_buffer.push_back(0xC8 | (static_cast<uint8_t>(dst) & 7));
        emit_immediate(ImmediateOperand(static_cast<int32_t>(imm.value)));
    }
}

void X86InstructionBuilder::xor_(X86Reg dst, X86Reg src, OpSize size) {
    emit_rex_if_needed(dst, src, size);
    code_buffer.push_back(0x31);  // XOR r/m, r
    code_buffer.push_back(compute_modrm(3, 
        static_cast<uint8_t>(src) & 7, 
        static_cast<uint8_t>(dst) & 7));
}

void X86InstructionBuilder::xor_(X86Reg dst, const ImmediateOperand& imm) {
    if (dst == X86Reg::RAX) {
        emit_rex_if_needed(dst, X86Reg::NONE, OpSize::QWORD);
        code_buffer.push_back(0x35);  // XOR RAX, imm32
        emit_immediate(ImmediateOperand(static_cast<int32_t>(imm.value)));
    } else if (imm.value >= -128 && imm.value <= 127) {
        emit_rex_if_needed(dst, X86Reg::NONE, OpSize::QWORD);
        code_buffer.push_back(0x83);  // XOR r/m, imm8
        code_buffer.push_back(0xF0 | (static_cast<uint8_t>(dst) & 7));
        code_buffer.push_back(static_cast<uint8_t>(imm.value));
    } else {
        emit_rex_if_needed(dst, X86Reg::NONE, OpSize::QWORD);
        code_buffer.push_back(0x81);  // XOR r/m, imm32
        code_buffer.push_back(0xF0 | (static_cast<uint8_t>(dst) & 7));
        emit_immediate(ImmediateOperand(static_cast<int32_t>(imm.value)));
    }
}

// =============================================================================
// Bit Manipulation
// =============================================================================

void X86InstructionBuilder::shl(X86Reg dst, const ImmediateOperand& count) {
    emit_rex_if_needed(dst, X86Reg::NONE, OpSize::QWORD);
    if (count.value == 1) {
        code_buffer.push_back(0xD1);  // SHL r/m, 1
        code_buffer.push_back(0xE0 | (static_cast<uint8_t>(dst) & 7));
    } else {
        code_buffer.push_back(0xC1);  // SHL r/m, imm8
        code_buffer.push_back(0xE0 | (static_cast<uint8_t>(dst) & 7));
        code_buffer.push_back(static_cast<uint8_t>(count.value));
    }
}

void X86InstructionBuilder::shr(X86Reg dst, const ImmediateOperand& count) {
    emit_rex_if_needed(dst, X86Reg::NONE, OpSize::QWORD);
    if (count.value == 1) {
        code_buffer.push_back(0xD1);  // SHR r/m, 1
        code_buffer.push_back(0xE8 | (static_cast<uint8_t>(dst) & 7));
    } else {
        code_buffer.push_back(0xC1);  // SHR r/m, imm8
        code_buffer.push_back(0xE8 | (static_cast<uint8_t>(dst) & 7));
        code_buffer.push_back(static_cast<uint8_t>(count.value));
    }
}

void X86InstructionBuilder::sar(X86Reg dst, const ImmediateOperand& count) {
    emit_rex_if_needed(dst, X86Reg::NONE, OpSize::QWORD);
    if (count.value == 1) {
        code_buffer.push_back(0xD1);  // SAR r/m, 1
        code_buffer.push_back(0xF8 | (static_cast<uint8_t>(dst) & 7));
    } else {
        code_buffer.push_back(0xC1);  // SAR r/m, imm8
        code_buffer.push_back(0xF8 | (static_cast<uint8_t>(dst) & 7));
        code_buffer.push_back(static_cast<uint8_t>(count.value));
    }
}

void X86InstructionBuilder::rol(X86Reg dst, const ImmediateOperand& count) {
    emit_rex_if_needed(dst, X86Reg::NONE, OpSize::QWORD);
    if (count.value == 1) {
        code_buffer.push_back(0xD1);  // ROL r/m, 1
        code_buffer.push_back(0xC0 | (static_cast<uint8_t>(dst) & 7));
    } else {
        code_buffer.push_back(0xC1);  // ROL r/m, imm8
        code_buffer.push_back(0xC0 | (static_cast<uint8_t>(dst) & 7));
        code_buffer.push_back(static_cast<uint8_t>(count.value));
    }
}

void X86InstructionBuilder::ror(X86Reg dst, const ImmediateOperand& count) {
    emit_rex_if_needed(dst, X86Reg::NONE, OpSize::QWORD);
    if (count.value == 1) {
        code_buffer.push_back(0xD1);  // ROR r/m, 1
        code_buffer.push_back(0xC8 | (static_cast<uint8_t>(dst) & 7));
    } else {
        code_buffer.push_back(0xC1);  // ROR r/m, imm8
        code_buffer.push_back(0xC8 | (static_cast<uint8_t>(dst) & 7));
        code_buffer.push_back(static_cast<uint8_t>(count.value));
    }
}

// =============================================================================
// Advanced Instructions
// =============================================================================

void X86InstructionBuilder::lea(X86Reg dst, const MemoryOperand& src) {
    emit_rex_if_needed(dst, src.base, OpSize::QWORD);
    code_buffer.push_back(0x8D);  // LEA r, m
    emit_modrm_sib_disp(static_cast<uint8_t>(dst) & 7, src);
}

void X86InstructionBuilder::cdq() {
    code_buffer.push_back(0x99);  // CDQ
}

void X86InstructionBuilder::cqo() {
    code_buffer.push_back(0x48);  // REX.W
    code_buffer.push_back(0x99);  // CQO
}

// =============================================================================
// SIMD Operations (Basic)
// =============================================================================

void X86InstructionBuilder::movdqa(X86Reg dst, X86Reg src) {
    code_buffer.push_back(0x66);  // Operand size prefix
    emit_rex_if_needed(dst, src, OpSize::QWORD);
    code_buffer.push_back(0x0F);  // Two-byte opcode
    code_buffer.push_back(0x6F);  // MOVDQA
    code_buffer.push_back(compute_modrm(3, 
        static_cast<uint8_t>(dst) & 7, 
        static_cast<uint8_t>(src) & 7));
}

void X86InstructionBuilder::movdqu(X86Reg dst, const MemoryOperand& src) {
    code_buffer.push_back(0xF3);  // REP prefix for MOVDQU
    emit_rex_if_needed(dst, src.base, OpSize::QWORD);
    code_buffer.push_back(0x0F);  // Two-byte opcode
    code_buffer.push_back(0x6F);  // MOVDQU
    emit_modrm_sib_disp(static_cast<uint8_t>(dst) & 7, src);
}

// =============================================================================
// Atomic Operations
// =============================================================================

void X86InstructionBuilder::lock_prefix() {
    code_buffer.push_back(0xF0);  // LOCK prefix
}

void X86InstructionBuilder::cmpxchg(const MemoryOperand& dst, X86Reg src, OpSize size) {
    lock_prefix();
    emit_rex_if_needed(src, dst.base, size);
    code_buffer.push_back(0x0F);  // Two-byte opcode
    code_buffer.push_back(0xB1);  // CMPXCHG
    emit_modrm_sib_disp(static_cast<uint8_t>(src) & 7, dst);
}

void X86InstructionBuilder::xadd(const MemoryOperand& dst, X86Reg src, OpSize size) {
    lock_prefix();
    emit_rex_if_needed(src, dst.base, size);
    code_buffer.push_back(0x0F);  // Two-byte opcode
    code_buffer.push_back(0xC1);  // XADD
    emit_modrm_sib_disp(static_cast<uint8_t>(src) & 7, dst);
}

// =============================================================================
// Memory Barriers
// =============================================================================

void X86InstructionBuilder::mfence() {
    code_buffer.push_back(0x0F);  // Two-byte opcode
    code_buffer.push_back(0xAE);  // Memory barrier group
    code_buffer.push_back(0xF0);  // MFENCE
}

void X86InstructionBuilder::lfence() {
    code_buffer.push_back(0x0F);  // Two-byte opcode
    code_buffer.push_back(0xAE);  // Memory barrier group
    code_buffer.push_back(0xE8);  // LFENCE
}

void X86InstructionBuilder::sfence() {
    code_buffer.push_back(0x0F);  // Two-byte opcode
    code_buffer.push_back(0xAE);  // Memory barrier group
    code_buffer.push_back(0xF8);  // SFENCE
}

// =============================================================================
// Utility Instructions
// =============================================================================

void X86InstructionBuilder::nop() {
    code_buffer.push_back(0x90);  // NOP
}

void X86InstructionBuilder::int3() {
    code_buffer.push_back(0xCC);  // INT3 (debugger breakpoint)
}

void X86InstructionBuilder::emit_bytes(const std::vector<uint8_t>& bytes) {
    code_buffer.insert(code_buffer.end(), bytes.begin(), bytes.end());
}

// =============================================================================
// Label Management
// =============================================================================

// Simple label management - would need proper implementation for production
static std::unordered_map<std::string, size_t> g_label_addresses;
static std::unordered_map<std::string, std::vector<size_t>> g_unresolved_labels;

void X86InstructionBuilder::emit_label_placeholder(const std::string& label) {
    auto it = g_label_addresses.find(label);
    if (it != g_label_addresses.end()) {
        // Label already resolved
        int32_t offset = static_cast<int32_t>(it->second - (code_buffer.size() + 4));
        for (int i = 0; i < 4; i++) {
            code_buffer.push_back((offset >> (i * 8)) & 0xFF);
        }
    } else {
        // Store location for later resolution
        g_unresolved_labels[label].push_back(code_buffer.size());
        code_buffer.push_back(0x00);
        code_buffer.push_back(0x00);
        code_buffer.push_back(0x00);
        code_buffer.push_back(0x00);
    }
}

void X86InstructionBuilder::resolve_label(const std::string& label, size_t address) {
    g_label_addresses[label] = address;
    
    // Patch all unresolved references
    auto it = g_unresolved_labels.find(label);
    if (it != g_unresolved_labels.end()) {
        for (size_t location : it->second) {
            int32_t offset = static_cast<int32_t>(address - (location + 4));
            code_buffer[location] = offset & 0xFF;
            code_buffer[location + 1] = (offset >> 8) & 0xFF;
            code_buffer[location + 2] = (offset >> 16) & 0xFF;
            code_buffer[location + 3] = (offset >> 24) & 0xFF;
        }
        g_unresolved_labels.erase(it);
    }
}

// =============================================================================
// XMM Register Operations for High-Performance Floating-Point
// =============================================================================

bool X86InstructionBuilder::needs_rex_prefix_xmm(X86XmmReg xmm_reg, X86Reg gpr_reg) const {
    return static_cast<uint8_t>(xmm_reg) >= 8 || 
           (gpr_reg != X86Reg::NONE && static_cast<uint8_t>(gpr_reg) >= 8);
}

void X86InstructionBuilder::emit_rex_if_needed_xmm(X86XmmReg xmm_reg, X86Reg gpr_reg, bool w) {
    if (needs_rex_prefix_xmm(xmm_reg, gpr_reg) || w) {  // Always emit REX if W=1 for 64-bit operations
        bool r = static_cast<uint8_t>(xmm_reg) >= 8;
        bool b = (gpr_reg != X86Reg::NONE) && (static_cast<uint8_t>(gpr_reg) >= 8);
        code_buffer.push_back(compute_rex_prefix(w, r, false, b));
    }
}

void X86InstructionBuilder::movq(X86XmmReg dst, X86Reg src) {
    // MOVQ xmm, r64 - Move 64-bit from GPR to XMM register
    // Encoding: 66 REX.W 0F 6E /r
    code_buffer.push_back(0x66);  // Operand size override prefix
    emit_rex_if_needed_xmm(dst, src, true);  // REX.W = 1 for 64-bit
    code_buffer.push_back(0x0F);
    code_buffer.push_back(0x6E);
    code_buffer.push_back(compute_modrm(3, static_cast<uint8_t>(dst) & 7, static_cast<uint8_t>(src) & 7));
}

void X86InstructionBuilder::movq(X86Reg dst, X86XmmReg src) {
    // MOVQ r64, xmm - Move 64-bit from XMM to GPR register
    // Encoding: 66 REX.W 0F 7E /r
    code_buffer.push_back(0x66);  // Operand size override prefix
    emit_rex_if_needed_xmm(src, dst, true);  // REX.W = 1 for 64-bit
    code_buffer.push_back(0x0F);
    code_buffer.push_back(0x7E);
    code_buffer.push_back(compute_modrm(3, static_cast<uint8_t>(src) & 7, static_cast<uint8_t>(dst) & 7));
}

void X86InstructionBuilder::movsd(X86XmmReg dst, X86XmmReg src) {
    // MOVSD xmm1, xmm2 - Move scalar double-precision floating-point
    // Encoding: F2 0F 10 /r
    code_buffer.push_back(0xF2);  // REPNE prefix for MOVSD
    if (static_cast<uint8_t>(dst) >= 8 || static_cast<uint8_t>(src) >= 8) {
        bool r = static_cast<uint8_t>(dst) >= 8;
        bool b = static_cast<uint8_t>(src) >= 8;
        code_buffer.push_back(compute_rex_prefix(false, r, false, b));
    }
    code_buffer.push_back(0x0F);
    code_buffer.push_back(0x10);
    code_buffer.push_back(compute_modrm(3, static_cast<uint8_t>(dst) & 7, static_cast<uint8_t>(src) & 7));
}

void X86InstructionBuilder::movsd(X86XmmReg dst, const MemoryOperand& src) {
    // MOVSD xmm, m64 - Load scalar double from memory
    // Encoding: F2 0F 10 /r
    code_buffer.push_back(0xF2);  // REPNE prefix for MOVSD
    
    // Handle REX prefix for XMM + memory operations
    bool needs_rex = static_cast<uint8_t>(dst) >= 8 || 
                     (src.base != X86Reg::NONE && static_cast<uint8_t>(src.base) >= 8) ||
                     (src.index != X86Reg::NONE && static_cast<uint8_t>(src.index) >= 8);
    
    if (needs_rex) {
        bool r = static_cast<uint8_t>(dst) >= 8;
        bool x = (src.index != X86Reg::NONE) && (static_cast<uint8_t>(src.index) >= 8);
        bool b = (src.base != X86Reg::NONE) && (static_cast<uint8_t>(src.base) >= 8);
        code_buffer.push_back(compute_rex_prefix(false, r, x, b));
    }
    
    code_buffer.push_back(0x0F);
    code_buffer.push_back(0x10);
    emit_modrm_sib_disp(static_cast<uint8_t>(dst) & 7, src);
}

void X86InstructionBuilder::movsd(const MemoryOperand& dst, X86XmmReg src) {
    // MOVSD m64, xmm - Store scalar double to memory  
    // Encoding: F2 0F 11 /r
    code_buffer.push_back(0xF2);  // REPNE prefix for MOVSD
    
    // Handle REX prefix for XMM + memory operations
    bool needs_rex = static_cast<uint8_t>(src) >= 8 || 
                     (dst.base != X86Reg::NONE && static_cast<uint8_t>(dst.base) >= 8) ||
                     (dst.index != X86Reg::NONE && static_cast<uint8_t>(dst.index) >= 8);
    
    if (needs_rex) {
        bool r = static_cast<uint8_t>(src) >= 8;
        bool x = (dst.index != X86Reg::NONE) && (static_cast<uint8_t>(dst.index) >= 8);
        bool b = (dst.base != X86Reg::NONE) && (static_cast<uint8_t>(dst.base) >= 8);
        code_buffer.push_back(compute_rex_prefix(false, r, x, b));
    }
    
    code_buffer.push_back(0x0F);
    code_buffer.push_back(0x11);
    emit_modrm_sib_disp(static_cast<uint8_t>(src) & 7, dst);
}

void X86InstructionBuilder::cvtsi2sd(X86XmmReg dst, X86Reg src) {
    // CVTSI2SD xmm, r64 - Convert signed integer to scalar double
    // Encoding: F2 REX.W 0F 2A /r
    code_buffer.push_back(0xF2);  // REPNE prefix
    emit_rex_if_needed_xmm(dst, src, true);  // REX.W = 1 for 64-bit integer
    code_buffer.push_back(0x0F);
    code_buffer.push_back(0x2A);
    code_buffer.push_back(compute_modrm(3, static_cast<uint8_t>(dst) & 7, static_cast<uint8_t>(src) & 7));
}

void X86InstructionBuilder::cvtsd2si(X86Reg dst, X86XmmReg src) {
    // CVTSD2SI r64, xmm - Convert scalar double to signed integer
    // Encoding: F2 REX.W 0F 2D /r
    code_buffer.push_back(0xF2);  // REPNE prefix
    emit_rex_if_needed_xmm(src, dst, true);  // REX.W = 1 for 64-bit integer
    code_buffer.push_back(0x0F);
    code_buffer.push_back(0x2D);
    code_buffer.push_back(compute_modrm(3, static_cast<uint8_t>(dst) & 7, static_cast<uint8_t>(src) & 7));
}

// =============================================================================
// Validation and Optimization
// =============================================================================

bool X86InstructionBuilder::validate_instruction_stream() const {
    // Basic validation - check for valid instruction sequences
    // This would be expanded for production use
    return code_buffer.size() > 0;
}

void X86InstructionBuilder::optimize_instruction_sequence() {
    // Peephole optimizations would go here
    // Examples:
    // - MOV reg, reg -> eliminate
    // - ADD reg, 0 -> eliminate
    // - Combine adjacent similar operations
}

}  // namespace ultraScript
