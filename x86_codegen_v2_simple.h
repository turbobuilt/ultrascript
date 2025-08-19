#pragma once

#include "codegen_forward.h"
#include <vector>
#include <unordered_map>
#include <string>



// Simplified X86 code generator for free keyword implementation
class X86CodeGenV2 : public CodeGenerator {
private:
    std::vector<uint8_t> code_buffer;
    std::vector<std::pair<std::string, size_t>> unresolved_jumps;
    std::vector<std::pair<int, size_t>> pending_label_patches;
    bool debug_mode_enabled = false;
    int next_label_id = 1;
    
public:
    X86CodeGenV2();
    virtual ~X86CodeGenV2() = default;
    
    // Basic code emission methods for free keyword
    void emit_byte(uint8_t byte) override;
    void emit_u32(uint32_t value) override;
    void emit_comment(const char* comment) override;
    void emit_call_external(const char* func_name) override;
    
    // Label management for control flow
    int create_label() override;
    void mark_jump_location(int label_id) override;
    void patch_jump(int label_id) override;
    void emit_jump_back(int label_id) override;
    
    // Debug mode
    void enable_debug_mode(bool enabled);
    
    // Get generated code
    const std::vector<uint8_t>& get_code_buffer() const { return code_buffer; }
};


