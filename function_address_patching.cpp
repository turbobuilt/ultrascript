#include "function_address_patching.h"
#include "compiler.h"
#include <iostream>

// Global patch list
std::vector<FunctionPatchInfo> g_function_patches;

void register_function_patch(size_t patch_offset, void* function_ast, size_t additional_offset, size_t instruction_length) {
    g_function_patches.emplace_back();
    auto& patch_info = g_function_patches.back();
    
    patch_info.patch_offset = patch_offset;
    patch_info.function_ast = function_ast;
    patch_info.additional_offset = additional_offset;
    patch_info.instruction_length = instruction_length;
    
    std::cout << "[PATCH_REGISTER] Registered patch at offset " << patch_offset 
              << " for function '" << ((FunctionDecl*)function_ast)->name 
              << " with additional_offset " << additional_offset 
              << " and instruction_length " << instruction_length << std::endl;
}

void patch_all_function_addresses(void* executable_memory_base) {
    std::cout << "[PATCH_SYSTEM] Patching " << g_function_patches.size() 
              << " function addresses in executable memory at " << executable_memory_base << std::endl;
              
    for (const auto& patch_info : g_function_patches) {
        std::cout << "[PATCH_DEBUG] Processing patch for function '" << ((FunctionDecl*)patch_info.function_ast)->name << "'" << std::endl;
        std::cout << "[PATCH_DEBUG]   function code_offset: " << ((FunctionDecl*)patch_info.function_ast)->code_offset << std::endl;
        std::cout << "[PATCH_DEBUG]   patch_offset: " << patch_info.patch_offset << std::endl;
        std::cout << "[PATCH_DEBUG]   additional_offset: " << patch_info.additional_offset << std::endl;
        
        // Calculate actual function address: base + function's code offset
        void* actual_function_address = reinterpret_cast<void*>(
            reinterpret_cast<uintptr_t>(executable_memory_base) + ((FunctionDecl*)patch_info.function_ast)->code_offset
        );
        
        std::cout << "[PATCH_DEBUG]   calculated function address: " << actual_function_address << std::endl;
        
        // Calculate patch location in executable memory
        void* patch_location = reinterpret_cast<void*>(
            reinterpret_cast<uintptr_t>(executable_memory_base) + patch_info.patch_offset + patch_info.additional_offset
        );
        
        std::cout << "[PATCH_DEBUG]   calculated patch location: " << patch_location << std::endl;
        
        // Verify patch location is within executable memory bounds
        // Assuming typical executable memory size, let's add a basic bounds check
        uintptr_t base_addr = reinterpret_cast<uintptr_t>(executable_memory_base);
        uintptr_t patch_addr = reinterpret_cast<uintptr_t>(patch_location);
        
        if (patch_addr < base_addr || patch_addr > base_addr + 1024*1024) {  // 1MB max
            std::cerr << "[PATCH_ERROR] Patch location " << patch_location 
                      << " is outside reasonable executable memory bounds!" << std::endl;
            continue;
        }
        
        std::cout << "[PATCH_DEBUG] About to write function address " << actual_function_address 
                  << " to patch location " << patch_location 
                  << " (instruction_length=" << patch_info.instruction_length << ")" << std::endl;
        
        // Determine immediate size based on instruction length
        bool is_32bit_immediate = (patch_info.instruction_length == X86MovConstants::MOV_32BIT_IMM_LENGTH);
        bool is_64bit_immediate = (patch_info.instruction_length == X86MovConstants::MOV_64BIT_IMM_LENGTH);
        
        if (!is_32bit_immediate && !is_64bit_immediate) {
            std::cerr << "[PATCH_ERROR] Unexpected instruction length: " << patch_info.instruction_length 
                      << " (expected " << X86MovConstants::MOV_32BIT_IMM_LENGTH 
                      << " for 32-bit or " << X86MovConstants::MOV_64BIT_IMM_LENGTH << " for 64-bit MOV)" << std::endl;
            continue;
        }
        
        size_t immediate_size = is_32bit_immediate ? X86MovConstants::IMM32_SIZE : X86MovConstants::IMM64_SIZE;
        
        std::cout << "[PATCH_DEBUG] Using " << (is_32bit_immediate ? "32-bit" : "64-bit") 
                  << " immediate (instruction_length=" << patch_info.instruction_length << ")" << std::endl;
        
        // Check placeholder value for debugging (optional warning)
        if (is_32bit_immediate) {
            uint32_t current_value = *reinterpret_cast<uint32_t*>(patch_location);
            if (current_value != 0x00000000) {
                std::cout << "[PATCH_NOTE] Non-zero placeholder found: " 
                          << std::hex << current_value << std::dec << std::endl;
            }
        } else {
            uint64_t current_value = *reinterpret_cast<uint64_t*>(patch_location);
            if (current_value != 0x0000000000000000ULL) {
                std::cout << "[PATCH_NOTE] Non-zero placeholder found: " 
                          << std::hex << current_value << std::dec << std::endl;
            }
        }
        
        // Debug: Show bytes before patching
        std::cout << "[PATCH_DEBUG] Bytes before patching (" << immediate_size << " bytes): ";
        unsigned char* bytes = reinterpret_cast<unsigned char*>(patch_location);
        for (size_t i = 0; i < immediate_size; i++) {
            printf("%02x ", bytes[i]);
        }
        std::cout << std::endl;
        
        // Patch the executable memory with the actual function address
        try {
            if (is_32bit_immediate) {
                // For 32-bit immediate, verify address fits and write only 4 bytes
                uintptr_t addr_value = reinterpret_cast<uintptr_t>(actual_function_address);
                if (addr_value > 0xFFFFFFFFULL) {
                    std::cerr << "[PATCH_ERROR] Function address " << std::hex << addr_value 
                              << std::dec << " too large for 32-bit immediate!" << std::endl;
                    continue;
                }
                uint32_t addr_32 = static_cast<uint32_t>(addr_value);
                *reinterpret_cast<uint32_t*>(patch_location) = addr_32;
                std::cout << "[PATCH_SUCCESS] Patched with 32-bit immediate: " << std::hex << addr_32 << std::dec << std::endl;
            } else {
                // For 64-bit immediate, write 8 bytes
                *reinterpret_cast<void**>(patch_location) = actual_function_address;
                std::cout << "[PATCH_SUCCESS] Patched with 64-bit immediate" << std::endl;
            }
        } catch (...) {
            std::cerr << "[PATCH_ERROR] Exception during memory write!" << std::endl;
            continue;
        }
        
        // Debug: Show bytes after patching
        std::cout << "[PATCH_DEBUG] Bytes after patching (" << immediate_size << " bytes): ";
        for (size_t i = 0; i < immediate_size; i++) {
            printf("%02x ", bytes[i]);
        }
        std::cout << std::endl;
        
        std::cout << "[PATCH_SYSTEM] Patched function '" << ((FunctionDecl*)patch_info.function_ast)->name 
                  << "' at patch location " << patch_location
                  << " with address " << actual_function_address 
                  << " (base+" << ((FunctionDecl*)patch_info.function_ast)->code_offset << ")" << std::endl;
    }
    
    std::cout << "[PATCH_SYSTEM] All function addresses patched successfully!" << std::endl;
}
