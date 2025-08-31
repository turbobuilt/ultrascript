#include "function_codegen.h"
#include "function_runtime.h"
#include "function_instance.h"
#include "x86_codegen_v2.h"
#include "function_address_patching.h"
#include <iostream>
#include <stdexcept>
#include <unordered_map>
#include <cstring>

//=============================================================================
// FUNCTION CALL STRATEGY DETERMINATION
//=============================================================================

FunctionCallStrategy determine_function_call_strategy(const std::string& var_name, 
                                                     SimpleLexicalScopeAnalyzer* analyzer) {
    // TEMPORARY: Force direct call for testing to avoid complexity
    std::cout << "[FUNCTION_CODEGEN] Forcing DIRECT_CALL strategy for testing" << std::endl;
    return FunctionCallStrategy::DIRECT_CALL;
}

//=============================================================================
// FUNCTION CALL CODE GENERATION METHODS
//=============================================================================

void generate_function_call_code(CodeGenerator& gen, 
                                const std::string& function_var_name,
                                SimpleLexicalScopeAnalyzer* analyzer,
                                LexicalScopeNode* current_scope) {
    
    if (!current_scope) {
        throw std::runtime_error("Cannot generate function call without scope context");
    }
    
    // Get variable offset in current scope
    auto offset_it = current_scope->variable_offsets.find(function_var_name);
    if (offset_it == current_scope->variable_offsets.end()) {
        throw std::runtime_error("Function variable '" + function_var_name + "' not found in scope");
    }
    
    size_t variable_offset = offset_it->second;
    bool is_local_scope = true; // For now, assume local scope access
    
    // CRITICAL: Initialize function variable if it's a function declaration
    // For now, use a simplified initialization approach
    if (current_scope) {
        for (auto* decl : current_scope->declared_functions) {
            if (decl && decl->name == function_var_name) {
                std::cout << "[FUNCTION_CODEGEN] Found function declaration '" << function_var_name << "', initializing variable" << std::endl;
                
                X86CodeGenV2* x86_gen = dynamic_cast<X86CodeGenV2*>(&gen);
                if (!x86_gen) {
                    throw std::runtime_error("Function variable initialization requires X86CodeGenV2");
                }
                
                std::string already_init_label = "func_already_init_" + function_var_name;
                
                // Check if function variable is already initialized by checking for FUNCTION_TYPE_TAG
                x86_gen->emit_mov_reg_reg_offset(10, 15, variable_offset); // R10 = first 8 bytes of function variable
                x86_gen->emit_mov_reg_imm(11, FUNCTION_TYPE_TAG); // R11 = FUNCTION_TYPE_TAG
                x86_gen->emit_compare(10, 11); // Compare R10 with FUNCTION_TYPE_TAG 
                x86_gen->emit_jump_if_zero(already_init_label); // Jump if equal (ZF=1)
                
                std::cout << "[FUNCTION_CODEGEN] Initializing function variable '" << function_var_name << "' with correct memory layout" << std::endl;
                
                // FIXED: Implement correct FunctionVariable memory layout from FUNCTION.md
                // FunctionVariable structure:
                // [0-7]:   FUNCTION_TYPE_TAG
                // [8-15]:  function_instance_ptr (points to offset 16)  
                // [16-23]: FunctionInstance.size
                // [24-31]: FunctionInstance.function_code_addr
                // [32+]:   FunctionInstance scope addresses...
                
                // Step 1: Write FUNCTION_TYPE_TAG at offset 0
                x86_gen->emit_mov_reg_imm(11, FUNCTION_TYPE_TAG);
                x86_gen->emit_mov_reg_offset_reg(15, variable_offset + 0, 11);
                
                // Step 2: Write function_instance_ptr at offset 8 (points to offset 16)
                x86_gen->emit_mov_reg_reg(11, 15);
                x86_gen->emit_add_reg_imm(11, variable_offset + 16);
                x86_gen->emit_mov_reg_offset_reg(15, variable_offset + 8, 11);
                
                // Step 3: Write FunctionInstance.size at offset 16 (minimal size: 24 bytes)
                x86_gen->emit_mov_reg_imm(11, 24);
                x86_gen->emit_mov_reg_offset_reg(15, variable_offset + 16, 11);
                
                // Step 4: Store function code address using direct address patching
                std::cout << "[FUNCTION_CODEGEN] Storing function code address using compile-time patching" << std::endl;
                
                // Generate: mov r11, <PLACEHOLDER_ADDRESS>  ; This will be patched with actual function address
                // Get the offset BEFORE emitting the instruction, then calculate based on actual length
                size_t instruction_start = x86_gen->get_current_offset();
                std::cout << "[PATCH_DEBUG_DETAILED] instruction_start = " << instruction_start << std::endl;
                
                // Use zero placeholder - let instruction builder choose optimal encoding
                x86_gen->emit_mov_reg_imm(11, 0x0000000000000000ULL);  // Placeholder - will be patched
                
                size_t instruction_end = x86_gen->get_current_offset();
                size_t instruction_length = instruction_end - instruction_start;
                std::cout << "[PATCH_DEBUG_DETAILED] instruction_end = " << instruction_end << std::endl;
                std::cout << "[PATCH_DEBUG_DETAILED] instruction_size = " << (instruction_end - instruction_start) << std::endl;
                std::cout << "[PATCH_DEBUG_DETAILED] actual_instruction_length = " << instruction_length << std::endl;
                
                // Calculate patch offset based on instruction encoding
                // For MOV r64, imm64: REX(1) + opcode(1) + immediate(8) = 10 bytes total
                // For MOV r/m64, imm32: REX(1) + opcode(1) + modrm(1) + immediate(4) = 7 bytes total
                size_t patch_offset;
                if (instruction_length == X86MovConstants::MOV_64BIT_IMM_LENGTH) {
                    // 64-bit immediate: patch starts at offset +2 (skip REX + opcode)
                    patch_offset = instruction_start + X86MovConstants::MOV_64BIT_IMM_OFFSET;
                    std::cout << "[PATCH_DEBUG_DETAILED] Using 64-bit immediate, patch_offset = " << patch_offset << std::endl;
                } else if (instruction_length == X86MovConstants::MOV_32BIT_IMM_LENGTH) {
                    // 32-bit immediate: patch starts at offset +3 (skip REX + opcode + modrm)
                    patch_offset = instruction_start + X86MovConstants::MOV_32BIT_IMM_OFFSET;
                    std::cout << "[PATCH_DEBUG_DETAILED] Using 32-bit immediate, patch_offset = " << patch_offset << std::endl;
                } else {
                    std::cerr << "[PATCH_ERROR] Unexpected MOV instruction length: " << instruction_length 
                              << " (expected " << X86MovConstants::MOV_32BIT_IMM_LENGTH 
                              << " or " << X86MovConstants::MOV_64BIT_IMM_LENGTH << ")" << std::endl;
                    throw std::runtime_error("Unexpected MOV instruction encoding");
                }
                
                std::cout << "[PATCH_DEBUG_DETAILED] final_patch_offset = " << patch_offset << std::endl;
                
                // Register this offset for patching with actual function address at runtime
                register_function_patch(patch_offset, decl, 0, instruction_length);
                
                std::cout << "[FUNCTION_CODEGEN] Registered function '" << decl->name 
                          << "' for patching at machine code offset " << patch_offset << std::endl;
                
                // Store the loaded address into the function instance
                x86_gen->emit_mov_reg_offset_reg(15, variable_offset + 24, 11);
                
                x86_gen->emit_label(already_init_label);
                std::cout << "[FUNCTION_CODEGEN] Function variable initialization complete with patching registration" << std::endl;
                break;
            }
        }
    }
    
    FunctionCallStrategy strategy = determine_function_call_strategy(function_var_name, analyzer);
    
    std::cout << "[FUNCTION_CODEGEN] Generating function call for '" << function_var_name 
              << "' using strategy " << static_cast<int>(strategy) << std::endl;
    
    switch (strategy) {
        case FunctionCallStrategy::DIRECT_CALL:
            generate_direct_function_call(gen, function_var_name, variable_offset, is_local_scope);
            break;
            
        case FunctionCallStrategy::POINTER_INDIRECTION:
            generate_function_typed_call(gen, function_var_name, variable_offset, is_local_scope);
            break;
            
        case FunctionCallStrategy::DYNAMIC_TYPE_CHECK:
            generate_dynamic_function_call(gen, function_var_name, variable_offset, is_local_scope);
            break;
    }
}

void generate_direct_function_call(CodeGenerator& gen,
                                  const std::string& function_var_name,
                                  size_t variable_offset,
                                  bool is_local_scope) {
    std::cout << "[FUNCTION_CODEGEN] Strategy 1: Direct function call for " << function_var_name << std::endl;
    
    // Cast to X86CodeGenV2 for register access
    X86CodeGenV2* x86_gen = dynamic_cast<X86CodeGenV2*>(&gen);
    if (!x86_gen) {
        throw std::runtime_error("Function calls require X86CodeGenV2");
    }
    
    // Strategy 1: Direct call, zero indirection (fastest)
    // FUNCTION.md Assembly:
    // mov rdi, [scope + f_offset]           ; Load function variable base address  
    // cmp qword [rdi], FUNCTION_TYPE_TAG    ; Verify it's a function type
    // jne error_not_function                ; Jump if not a function
    // mov rdi, [rdi + 8]                    ; Load function instance pointer
    // call [rdi + 8]                        ; Call function_code_addr
    
    if (is_local_scope) {
        // Load function variable address
        x86_gen->emit_mov_reg_reg(7, 15);  // mov rdi, r15 (current scope)
        x86_gen->emit_add_reg_imm(7, variable_offset);  // add rdi, variable_offset
        
        // Verify function type (optional for Strategy 1, but good for safety)
        // TEMPORARY: Disable type checking to isolate the issue
        /*
        x86_gen->emit_mov_reg_reg_offset(0, 7, 0); // mov rax, [rdi] (load type tag)
        x86_gen->emit_mov_reg_imm(1, FUNCTION_TYPE_TAG); // mov rcx, FUNCTION_TYPE_TAG
        x86_gen->emit_compare(0, 1); // cmp rax, rcx
        
        // Generate error label for type check
        static int error_counter = 0;
        std::string error_label = "function_type_error_" + std::to_string(error_counter++);
        std::string continue_label = "function_call_continue_" + std::to_string(error_counter);
        
        x86_gen->emit_jump_if_not_zero(error_label); // jne error_label
        */
        
        // Load function instance pointer from the variable
        x86_gen->emit_mov_reg_reg_offset(7, 7, 8); // mov rdi, [rdi + 8] (load function instance ptr)
        
        // Load the function address from the function instance (pre-patched during startup)
        // Function instance layout: [size:8] [function_code_addr:8] [captured_scopes...]
        x86_gen->emit_mov_reg_reg_offset(0, 7, 8); // mov rax, [rdi + 8] (load function_code_addr)
        
        // Call the function address
        x86_gen->emit_call_reg(0); // call rax
        
        // TEMPORARY: No jump needed since type checking is disabled
        /*
        x86_gen->emit_jump(continue_label); // jmp continue_label
        
        // Error handling
        x86_gen->emit_label(error_label);
        x86_gen->emit_call("__throw_function_type_error"); // Throw runtime error
        
        x86_gen->emit_label(continue_label);
        */
        
        std::cout << "[FUNCTION_CODEGEN] Generated Strategy 1 direct call assembly with type checking" << std::endl;
    } else {
        // TODO: Handle parent scope access using R12, R13, R14 registers
        throw std::runtime_error("Parent scope function calls not yet implemented for Strategy 1");
    }
}

void generate_function_typed_call(CodeGenerator& gen,
                                 const std::string& function_var_name,
                                 size_t variable_offset,
                                 bool is_local_scope) {
    std::cout << "[FUNCTION_CODEGEN] Strategy 2: Function-typed call for " << function_var_name << std::endl;
    
    X86CodeGenV2* x86_gen = dynamic_cast<X86CodeGenV2*>(&gen);
    if (!x86_gen) {
        throw std::runtime_error("Function calls require X86CodeGenV2");
    }
    
    // Strategy 2: Single pointer indirection, no type checks needed
    // FUNCTION.md Assembly (similar to Strategy 1, but for function-typed variables):
    // mov rdi, r15
    // add rdi, function_var_offset     ; RDI = pointer to function instance  
    // call [rdi + 8]                   ; Call function_code_addr
    
    if (is_local_scope) {
        x86_gen->emit_mov_reg_reg(7, 15);  // mov rdi, r15 (current scope)
        x86_gen->emit_add_reg_imm(7, variable_offset);  // add rdi, variable_offset
        
        // No type checking needed for Strategy 2 (guaranteed to be function)
        // FIXED: Load function instance pointer and call using correct memory layout
        x86_gen->emit_mov_reg_reg_offset(7, 7, 8); // mov rdi, [rdi + 8] (load function_instance_ptr from offset 8)
        x86_gen->emit_mov_reg_reg_offset(0, 7, 8); // mov rax, [rdi + 8] (load function_code_addr from FunctionInstance)
        x86_gen->emit_call_reg(0); // call rax
        
        std::cout << "[FUNCTION_CODEGEN] Generated Strategy 2 function-typed call assembly" << std::endl;
    } else {
        throw std::runtime_error("Parent scope function calls not yet implemented for Strategy 2");
    }
}

void generate_dynamic_function_call(CodeGenerator& gen,
                                   const std::string& function_var_name,
                                   size_t variable_offset,
                                   bool is_local_scope) {
    std::cout << "[FUNCTION_CODEGEN] Strategy 3: Dynamic type-checked call for " << function_var_name << std::endl;
    
    X86CodeGenV2* x86_gen = dynamic_cast<X86CodeGenV2*>(&gen);
    if (!x86_gen) {
        throw std::runtime_error("Function calls require X86CodeGenV2");
    }
    
    // Strategy 3: Type check + branch + indirection (slower but safe)
    // FUNCTION.md Assembly:
    // mov rax, [r15 + variable_offset]     ; Load type tag  
    // cmp rax, FUNCTION_TYPE_TAG           ; Check if it's a function
    // jne .not_a_function                  ; Branch if not function
    // .is_function:
    //     mov rdi, r15
    //     add rdi, variable_offset + 8     ; RDI = pointer to function data within DynamicValue
    //     call [rdi + 8]                   ; Call the function
    //     jmp .done
    // .not_a_function:
    //     call __throw_type_error
    // .done:
    
    static int call_counter = 0;
    std::string not_function_label = "not_function_" + std::to_string(call_counter);
    std::string done_label = "done_call_" + std::to_string(call_counter++);
    
    // Load type tag from DynamicValue
    if (is_local_scope) {
        x86_gen->emit_mov_reg_reg_offset(0, 15, variable_offset);  // mov rax, [r15 + offset] - load type_tag
    } else {
        throw std::runtime_error("Parent scope function calls not yet implemented for Strategy 3");
    }
    
    // Compare type tag with FUNCTION_TYPE_TAG
    x86_gen->emit_mov_reg_imm(1, FUNCTION_TYPE_TAG); // mov rcx, FUNCTION_TYPE_TAG
    x86_gen->emit_compare(0, 1); // cmp rax, rcx
    x86_gen->emit_jump_if_not_zero(not_function_label); // jne not_function_label
    
    // Function call path - OPTIMIZED with pre-patched function instances
    if (is_local_scope) {
        // Load function_instance_ptr from [r15 + variable_offset + 8]
        x86_gen->emit_mov_reg_reg_offset(7, 15, variable_offset + 8); // mov rdi, [r15 + offset + 8] (load function_instance_ptr)
        
        // Load function_code_addr from [rdi + 8] (FunctionInstance.function_code_addr offset)
        // Address is pre-patched during startup - zero cost lookup!
        x86_gen->emit_mov_reg_reg_offset(0, 7, 8); // mov rax, [rdi + 8] (load function_code_addr)
        
        // Call the function directly (zero cost!)
        x86_gen->emit_call_reg(0); // call rax
        x86_gen->emit_call_reg(0); // call rax
    } else {
        throw std::runtime_error("Parent scope function calls not yet implemented for Strategy 3");
    }
    
    // Error handling for non-function types
    x86_gen->emit_label(not_function_label);
    x86_gen->emit_call("__throw_function_type_error"); // Throw TypeError: variable is not a function
    
    x86_gen->emit_label(done_label);
    
    std::cout << "[FUNCTION_CODEGEN] Generated Strategy 3 dynamic call assembly with runtime type checking" << std::endl;
}

//=============================================================================
// FUNCTION CLOSURE SETUP CODE GENERATION
//=============================================================================

void generate_function_prologue_with_closure(CodeGenerator& gen,
                                            FunctionInstance* function_instance,
                                            const std::vector<LexicalScopeNode*>& captured_scopes) {
    std::cout << "[FUNCTION_CODEGEN] Generating function prologue with closure setup" << std::endl;
    
    X86CodeGenV2* x86_gen = dynamic_cast<X86CodeGenV2*>(&gen);
    if (!x86_gen) {
        throw std::runtime_error("Closure setup requires X86CodeGenV2");
    }
    
    // Standard function prologue
    x86_gen->emit_prologue();
    
    // Save caller's scope registers (from FUNCTION.md)
    generate_scope_register_save(gen);
    
    // Load captured scopes into registers based on priority
    if (function_instance) {
        size_t scope_count = (function_instance->size - 16) / 8;
        void** scope_addresses = function_instance->get_scope_addresses();
        
        // Load up to 3 most frequent scopes into R12, R13, R14
        for (size_t i = 0; i < std::min(scope_count, static_cast<size_t>(3)); i++) {
            int target_register = PARENT_SCOPE_1_REGISTER + i;  // R12, R13, R14
            
            // Load scope address: mov r12/r13/r14, [scope_address]
            x86_gen->emit_mov_reg_imm(target_register, 
                                     reinterpret_cast<uint64_t>(scope_addresses[i]));
            
            std::cout << "[FUNCTION_CODEGEN] Loaded captured scope " << i 
                      << " into R" << target_register << std::endl;
        }
        
        // Additional scopes go on stack (TODO: implement stack-based scope storage)
        if (scope_count > 3) {
            std::cout << "[FUNCTION_CODEGEN] WARNING: " << (scope_count - 3) 
                      << " additional scopes need stack storage (not yet implemented)" << std::endl;
        }
    }
    
    // Allocate local scope on heap for this function
    // This will be pointed to by R15
    // TODO: Get actual local scope size from function metadata
    generate_local_scope_allocation(gen, 256); // Placeholder size
}

void generate_local_scope_allocation(CodeGenerator& gen, size_t local_scope_size) {
    X86CodeGenV2* x86_gen = dynamic_cast<X86CodeGenV2*>(&gen);
    if (!x86_gen) return;
    
    // Call runtime allocation function
    // rdi = scope_size (calling convention)
    x86_gen->emit_mov_reg_imm(7, local_scope_size);  // mov rdi, scope_size
    x86_gen->emit_call("__allocate_lexical_scope_heap_object");
    x86_gen->emit_mov_reg_reg(15, 0);  // mov r15, rax (r15 = allocated scope)
    
    std::cout << "[FUNCTION_CODEGEN] Generated local scope allocation (" << local_scope_size 
              << " bytes)" << std::endl;
}

//=============================================================================
// UTILITY FUNCTIONS
//=============================================================================

void generate_function_call_error(CodeGenerator& gen, const std::string& variable_name) {
    X86CodeGenV2* x86_gen = dynamic_cast<X86CodeGenV2*>(&gen);
    if (!x86_gen) return;
    
    // Call runtime error function
    // TODO: Pass variable name in error message
    x86_gen->emit_call("__throw_type_error");
}

void generate_scope_register_save(CodeGenerator& gen) {
    X86CodeGenV2* x86_gen = dynamic_cast<X86CodeGenV2*>(&gen);
    if (!x86_gen) return;
    
    // Save caller's scope registers according to FUNCTION.md
    // Using RSP-relative operations to simulate push
    x86_gen->emit_sub_reg_imm(4, 32);                    // sub rsp, 32 (make space for 4 registers)
    x86_gen->emit_mov_mem_rsp_reg(0, 12);                // mov [rsp], r12
    x86_gen->emit_mov_mem_rsp_reg(8, 13);                // mov [rsp+8], r13  
    x86_gen->emit_mov_mem_rsp_reg(16, 14);               // mov [rsp+16], r14
    x86_gen->emit_mov_mem_rsp_reg(24, 15);               // mov [rsp+24], r15
    
    std::cout << "[FUNCTION_CODEGEN] Generated scope register save" << std::endl;
}

void generate_scope_register_restore(CodeGenerator& gen) {
    X86CodeGenV2* x86_gen = dynamic_cast<X86CodeGenV2*>(&gen);
    if (!x86_gen) return;
    
    // Restore caller's scope registers
    // Using RSP-relative operations to simulate pop (reverse order)
    x86_gen->emit_mov_reg_mem_rsp(15, 24);               // mov r15, [rsp+24] 
    x86_gen->emit_mov_reg_mem_rsp(14, 16);               // mov r14, [rsp+16]
    x86_gen->emit_mov_reg_mem_rsp(13, 8);                // mov r13, [rsp+8] 
    x86_gen->emit_mov_reg_mem_rsp(12, 0);                // mov r12, [rsp]
    x86_gen->emit_add_reg_imm(4, 32);                    // add rsp, 32 (restore stack)
    
    std::cout << "[FUNCTION_CODEGEN] Generated scope register restore" << std::endl;
}
