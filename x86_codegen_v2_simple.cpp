#include "x86_codegen_v2.h"
#include <iostream>



// Simple stub implementations for X86CodeGenV2 to get build working
void X86CodeGenV2::emit_byte(uint8_t byte) {
    code_buffer.push_back(byte);
}

void X86CodeGenV2::emit_u32(uint32_t value) {
    code_buffer.push_back(value & 0xFF);
    code_buffer.push_back((value >> 8) & 0xFF);
    code_buffer.push_back((value >> 16) & 0xFF);
    code_buffer.push_back((value >> 24) & 0xFF);
}

void X86CodeGenV2::emit_comment(const char* comment) {
    if (debug_mode_enabled) {
        std::cout << "Debug: " << comment << std::endl;
    }
}

void X86CodeGenV2::emit_call_external(const char* func_name) {
    // Stub for external function calls
    unresolved_jumps.push_back({std::string(func_name), code_buffer.size()});
    
    // Emit placeholder call instruction
    emit_byte(0xE8);  // call relative
    emit_u32(0x00000000);  // placeholder offset
    
    if (debug_mode_enabled) {
        emit_comment(("Call external function: " + std::string(func_name)).c_str());
    }
}

int X86CodeGenV2::create_label() {
    return next_label_id++;
}

void X86CodeGenV2::mark_jump_location(int label_id) {
    pending_label_patches.push_back({label_id, code_buffer.size()});
    
    if (debug_mode_enabled) {
        emit_comment(("Label " + std::to_string(label_id) + " at position " + std::to_string(code_buffer.size())).c_str());
    }
}

void X86CodeGenV2::patch_jump(int label_id) {
    size_t current_pos = code_buffer.size();
    
    for (auto it = pending_label_patches.begin(); it != pending_label_patches.end();) {
        if (it->first == label_id) {
            int32_t offset = static_cast<int32_t>(current_pos - it->second);
            // Patch the offset in the code buffer
            size_t patch_pos = it->second - 4;  // 4 bytes before the stored position
            code_buffer[patch_pos] = offset & 0xFF;
            code_buffer[patch_pos + 1] = (offset >> 8) & 0xFF;
            code_buffer[patch_pos + 2] = (offset >> 16) & 0xFF;
            code_buffer[patch_pos + 3] = (offset >> 24) & 0xFF;
            
            it = pending_label_patches.erase(it);
        } else {
            ++it;
        }
    }
}

void X86CodeGenV2::emit_jump_back(int label_id) {
    // Find the target position for this label
    size_t target_pos = 0;
    bool found = false;
    
    // For now, just emit a placeholder jump
    emit_byte(0xE9);  // jmp relative
    emit_u32(0x00000000);  // placeholder offset
    
    if (debug_mode_enabled) {
        emit_comment(("Jump back to label " + std::to_string(label_id)).c_str());
    }
}

// Constructor
X86CodeGenV2::X86CodeGenV2() : debug_mode_enabled(false), next_label_id(1) {
    code_buffer.reserve(1024);  // Pre-allocate some space
}

// Enable debug mode for testing
void X86CodeGenV2::enable_debug_mode(bool enabled) {
    debug_mode_enabled = enabled;
}


