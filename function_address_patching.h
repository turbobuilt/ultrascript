#ifndef FUNCTION_ADDRESS_PATCHING_H
#define FUNCTION_ADDRESS_PATCHING_H

#include <vector>
#include <cstdint>

// Forward declaration
struct FunctionDecl;

#pragma once
#include <vector>
#include <cstdint>
#include <cstddef>

// Forward declaration
struct FunctionDecl;

// X86 MOV instruction format constants for validation
namespace X86MovConstants {
    // Instruction lengths
    static constexpr size_t MOV_32BIT_IMM_LENGTH = 7;   // REX + 0xC7 + ModR/M + imm32
    static constexpr size_t MOV_64BIT_IMM_LENGTH = 10;  // REX + 0xB8-0xBF + imm64
    
    // Immediate field offsets within instruction
    static constexpr size_t MOV_32BIT_IMM_OFFSET = 3;   // After REX + 0xC7 + ModR/M
    static constexpr size_t MOV_64BIT_IMM_OFFSET = 2;   // After REX + 0xB8-0xBF
    
    // Immediate field sizes
    static constexpr size_t IMM32_SIZE = 4;
    static constexpr size_t IMM64_SIZE = 8;
    
    // Expected opcodes for validation
    static constexpr uint8_t REX_W = 0x48;              // REX.W prefix
    static constexpr uint8_t REX_WB = 0x49;             // REX.W + REX.B for R8-R15
    static constexpr uint8_t MOV_RM32_IMM32 = 0xC7;     // MOV r/m32, imm32
    static constexpr uint8_t MOV_R64_IMM64_BASE = 0xB8; // MOV r64, imm64 (base opcode)
    static constexpr uint8_t MODRM_REG_MASK = 0xF0;     // Mask for ModR/M register field
    static constexpr uint8_t MODRM_REG_DIRECT = 0xC0;   // Direct register addressing
}

// Patch information for function address resolution
struct FunctionPatchInfo {
    size_t patch_offset;         // Offset in machine code where patch should be applied
    FunctionDecl* function_ast;  // AST node pointer containing code_offset field
    size_t additional_offset;    // Additional offset within the patch location (default 0)
    size_t instruction_length;   // Length of the instruction (7 for 32-bit MOV, 10 for 64-bit MOV)
};

// Global patch list - populated during code generation
extern std::vector<FunctionPatchInfo> g_function_patches;

// Register a location that needs function address patching
void register_function_patch(size_t patch_offset, FunctionDecl* function_ast, size_t additional_offset = 0, size_t instruction_length = 10);

// Patch all function addresses in executable memory
void patch_all_function_addresses(void* executable_memory_base);

// Global patch list - populated during code generation
extern std::vector<FunctionPatchInfo> g_function_patches;

// Functions to manage function address patching
void register_function_patch(void* patch_location, FunctionDecl* function_ast, size_t offset = 0);
void patch_all_function_addresses(void* executable_memory_base);
void clear_function_patches();

#endif // FUNCTION_ADDRESS_PATCHING_H
