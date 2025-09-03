#include "function_codegen.h"
#include "compiler.h"
#include "simple_lexical_scope.h"
#include "x86_codegen_v2.h"
#include "function_instance.h"
#include "function_address_patching.h"
#include <iostream>

// SIMPLIFIED version of function codegen for getting basic functionality working
// This bypasses the complex function instance system temporarily

//=============================================================================
// FUNCTION CALL STRATEGY DETERMINATION  
//=============================================================================

FunctionCallStrategy determine_function_call_strategy(const std::string& function_var_name, 
                                                     const SimpleLexicalScopeAnalyzer* analyzer) {
    (void)function_var_name; (void)analyzer; // Suppress unused warnings
    
    // TEMPORARY: Always use direct calls for simplicity
    std::cout << "[FUNCTION_CODEGEN] SIMPLIFIED: Using DIRECT_CALL strategy for all functions" << std::endl;
    return FunctionCallStrategy::DIRECT_CALL;
}

//=============================================================================
// FUNCTION CALL CODE GENERATION METHODS
//=============================================================================

// Generate function call code using new hidden parameter approach from FUNCTION.md
void generate_function_call_code(CodeGenerator& gen, 
                                const std::string& function_var_name,
                                SimpleLexicalScopeAnalyzer* analyzer,
                                LexicalScopeNode* current_scope) {
    std::cout << "[NEW_FUNCTION_SYSTEM] Generating function call for '" << function_var_name << "' with hidden parameters" << std::endl;
    
    X86CodeGenV2* x86_gen = dynamic_cast<X86CodeGenV2*>(&gen);
    if (!x86_gen) {
        throw std::runtime_error("New function system requires X86CodeGenV2");
    }
    
    // NEW SYSTEM: For now, use direct call until full function registry is implemented
    std::cout << "[NEW_FUNCTION_SYSTEM] Using direct call for '" << function_var_name << "' (temporary until full registry)" << std::endl;
    x86_gen->emit_call(function_var_name);
    return;
    
    /* TODO: Implement full function registry lookup and hidden parameter passing
    // Find the function declaration to determine what scopes it needs
    auto function_decl_it = function_registry.find(function_var_name);
    if (function_decl_it == function_registry.end()) {
        std::cout << "[NEW_FUNCTION_SYSTEM] Function '" << function_var_name << "' not found in lookup, using direct call" << std::endl;
        x86_gen->emit_call(function_var_name);
        return;
    }*/
    
    /* TODO: Restore once function registry is implemented
    FunctionDecl* target_function = function_decl_it->second;
    if (!target_function->lexical_scope) {
        std::cout << "[NEW_FUNCTION_SYSTEM] Function '" << function_var_name << "' has no lexical scope, using direct call" << std::endl;
        x86_gen->emit_call(function_var_name);
        return;
    }
    
    // Get the scopes the target function needs, sorted by access frequency
    const std::vector<int>& required_scopes = target_function->lexical_scope->priority_sorted_parent_scopes;
    */ // End of TODO section - commented out until function registry is ready
    
    /* TODO: Continue once function registry is implemented
    std::cout << "[NEW_FUNCTION_SYSTEM] Function '" << function_var_name << "' needs " 
              << required_scopes.size() << " parent scopes as hidden parameters" << std::endl;
    
    // Pass required scope addresses as hidden parameters after regular arguments
    // We need to push them in reverse order so the function receives them in correct order
    for (int i = required_scopes.size() - 1; i >= 0; i--) {
        int required_depth = required_scopes[i];
        
        // Determine where to get this scope address from current context
        if (required_depth == current_scope->scope_depth) {
            // Current scope - use R15
            std::cout << "[NEW_FUNCTION_SYSTEM] Passing current scope (depth " << required_depth << ") from R15" << std::endl;
            // Load r15 into next available parameter register or push to stack
            if (i < 6) {
                // Use parameter registers for first 6 hidden parameters (after regular args)
                // This assumes regular arguments are already loaded into registers
                // We need to be careful about register allocation here
                x86_gen->emit_mov_reg_reg(7 + i, 15); // Use RDI, RSI, etc. for hidden params
            } else {
                x86_gen->emit_push_reg(15); // push r15 for additional hidden params
            }
        } else {
            // DISABLED: Parent scope passing via runtime lookup violates FUNCTION.md
            std::cerr << "ERROR: Function call scope parameter passing disabled!" << std::endl;
            std::cerr << "ERROR: Cannot pass parent scope parameters to function '" << function_var_name << "'" << std::endl;
            throw std::runtime_error("Function call scope parameter passing disabled - FUNCTION.md violation");
            
            // OLD CODE (DISABLED):
            // // Parent scope - find which register contains it or get from stack
            // // In the new system, we should have parent scope addresses in parameter registers
            // // from our own function's hidden parameters
            // 
            // // For now, assume parent scopes are accessible via runtime lookup
            // // This will be optimized once the full system is in place
            // std::cout << "[NEW_FUNCTION_SYSTEM] Passing parent scope (depth " << required_depth << ")" << std::endl;
            // x86_gen->emit_mov_reg_imm(0, required_depth); // RAX = scope depth
            // x86_gen->emit_call("__get_scope_address_for_depth"); // Returns address in RAX
            // 
            // if (i < 6) {
            //     x86_gen->emit_mov_reg_reg(7 + i, 0); // Move scope address to parameter register
            // } else {
            //     x86_gen->emit_push_reg(0); // Push scope address for additional hidden params
            // }
        }
    }
    
    // Make the function call
    std::cout << "[NEW_FUNCTION_SYSTEM] Calling function '" << function_var_name 
              << "' with " << required_scopes.size() << " hidden scope parameters" << std::endl;
    x86_gen->emit_call(function_var_name);
    
    // Clean up stack if we pushed additional hidden parameters
    size_t stack_params = (required_scopes.size() > 6) ? required_scopes.size() - 6 : 0;
    if (stack_params > 0) {
        int stack_cleanup = stack_params * 8; // 8 bytes per scope address
        std::cout << "[NEW_FUNCTION_SYSTEM] Cleaning up " << stack_cleanup << " bytes from stack" << std::endl;
        x86_gen->emit_add_reg_imm(4, stack_cleanup); // add rsp, stack_cleanup
    }
    */ // End of commented section
    
    std::cout << "[NEW_FUNCTION_SYSTEM] Function call completed using new hidden parameter system" << std::endl;
}

//=============================================================================
// MAIN FUNCTION CALL GENERATION
//=============================================================================

void generate_function_call(CodeGenerator& gen, const std::string& function_var_name, 
                           const SimpleLexicalScopeAnalyzer* analyzer) {
    
    std::cout << "[FUNCTION.md] FunctionCall::generate_code - function: " << function_var_name << std::endl;
    
    X86CodeGenV2* x86_gen = dynamic_cast<X86CodeGenV2*>(&gen);
    if (!x86_gen) {
        throw std::runtime_error("Function calls require X86CodeGenV2");
    }
    
    // FUNCTION.md approach: Pass parent scope addresses as hidden parameters
    LexicalScopeNode* current_scope = get_current_scope();
    
    if (current_scope && analyzer && analyzer->function_scope_requirements.count(function_var_name)) {
        const auto& required_scopes = analyzer->function_scope_requirements.at(function_var_name);
        
        std::cout << "[FUNCTION.md] Function '" << function_var_name 
                  << "' needs " << required_scopes.size() << " parent scope addresses" << std::endl;
        
        // Pass required scope addresses as hidden parameters after regular arguments
        // Push them in reverse order so the function receives them in correct order on stack
        for (int i = required_scopes.size() - 1; i >= 0; i--) {
            int required_depth = required_scopes[i];
            
            if (required_depth == current_scope->scope_depth) {
                // Current scope - use R15 (current scope register)
                std::cout << "[FUNCTION.md] Passing current scope (depth " << required_depth << ") from R15" << std::endl;
                x86_gen->emit_push_reg(15); // push r15 (current scope address)
            } else {
                // Parent scope - get from our own hidden parameters
                // In FUNCTION.md approach, parent scopes are received as hidden parameters after regular args
                // They're stored at stack offsets: rbp+16, rbp+24, rbp+32, etc.
                // Find which hidden parameter contains this scope depth
                int hidden_param_index = -1;
                if (current_scope->parent_scopes.size() > 0) {
                    for (size_t j = 0; j < current_scope->parent_scopes.size(); j++) {
                        if (current_scope->parent_scopes[j] == required_depth) {
                            hidden_param_index = static_cast<int>(j);
                            break;
                        }
                    }
                }
                
                if (hidden_param_index >= 0) {
                    // Load parent scope address from hidden parameter and push it
                    int stack_offset = 16 + (hidden_param_index * 8); // rbp+16+index*8
                    std::cout << "[FUNCTION.md] Passing parent scope (depth " << required_depth 
                              << ") from hidden parameter at rbp+" << stack_offset << std::endl;
                    x86_gen->emit_mov_reg_reg_offset(0, 5, stack_offset); // mov rax, [rbp+offset]
                    x86_gen->emit_push_reg(0); // push rax (parent scope address)
                } else {
                    std::cerr << "ERROR: Required parent scope depth " << required_depth 
                              << " not available in current function's parameters" << std::endl;
                    throw std::runtime_error("Parent scope not available for function call");
                }
            }
        }
        
        std::cout << "[FUNCTION.md] Pushed " << required_scopes.size() 
                  << " scope addresses as hidden parameters" << std::endl;
    }
    
    // Make the function call - hidden parameters are now on stack after regular arguments
    std::cout << "[FUNCTION.md] Calling function '" << function_var_name 
              << "' with FUNCTION.md calling convention" << std::endl;
    x86_gen->emit_call(function_var_name);
    
    // Clean up hidden parameter stack space
    if (current_scope && analyzer && analyzer->function_scope_requirements.count(function_var_name)) {
        const auto& required_scopes = analyzer->function_scope_requirements.at(function_var_name);
        if (required_scopes.size() > 0) {
            int stack_cleanup = required_scopes.size() * 8; // 8 bytes per scope address
            std::cout << "[FUNCTION.md] Cleaning up " << stack_cleanup << " bytes of hidden parameters" << std::endl;
            x86_gen->emit_add_reg_imm(4, stack_cleanup); // add rsp, stack_cleanup
        }
    }
    
    std::cout << "[FUNCTION.md] Function call completed using hidden parameter approach" << std::endl;
}

//=============================================================================
// STRATEGY IMPLEMENTATIONS (SIMPLIFIED)
//=============================================================================

void generate_direct_function_call(CodeGenerator& gen,
                                  const std::string& function_var_name,
                                  size_t variable_offset,
                                  bool is_local_scope) {
    (void)variable_offset; (void)is_local_scope; // Suppress unused warnings
    
    std::cout << "[FUNCTION_CODEGEN] Strategy 1: SIMPLIFIED Direct function call for " << function_var_name << std::endl;
    
    X86CodeGenV2* x86_gen = dynamic_cast<X86CodeGenV2*>(&gen);
    if (!x86_gen) {
        throw std::runtime_error("Function calls require X86CodeGenV2");
    }
    
    // SIMPLIFIED: Direct call by function name
    x86_gen->emit_call(function_var_name);
    
    std::cout << "[FUNCTION_CODEGEN] Generated simplified Strategy 1 direct call" << std::endl;
}

void generate_function_typed_call(CodeGenerator& gen,
                                 const std::string& function_var_name,
                                 size_t variable_offset,
                                 bool is_local_scope) {
    (void)gen; (void)function_var_name; (void)variable_offset; (void)is_local_scope;
    throw std::runtime_error("Strategy 2 (Function-typed calls) not yet implemented in simplified version");
}

void generate_dynamic_function_call(CodeGenerator& gen,
                                   const std::string& function_var_name,
                                   size_t variable_offset,
                                   bool is_local_scope) {
    (void)gen; (void)function_var_name; (void)variable_offset; (void)is_local_scope;
    throw std::runtime_error("Strategy 3 (Dynamic calls) not yet implemented in simplified version");
}

//=============================================================================
// PLACEHOLDER FUNCTIONS (NOT YET IMPLEMENTED)
//=============================================================================

void generate_function_prologue_with_closure(CodeGenerator& gen,
                                            FunctionInstance* function_instance,
                                            const std::vector<LexicalScopeNode*>& captured_scopes) {
    (void)gen; (void)function_instance; (void)captured_scopes;
    std::cout << "[FUNCTION_CODEGEN] SIMPLIFIED: Skipping function prologue with closure setup" << std::endl;
}

void generate_local_scope_allocation(CodeGenerator& gen, size_t local_scope_size) {
    (void)gen; (void)local_scope_size;
    std::cout << "[FUNCTION_CODEGEN] SIMPLIFIED: Skipping local scope allocation" << std::endl;
}

void generate_function_call_error(CodeGenerator& gen, const std::string& variable_name) {
    (void)gen; (void)variable_name;
    std::cout << "[FUNCTION_CODEGEN] SIMPLIFIED: Skipping function call error handling" << std::endl;
}

void generate_scope_register_save(CodeGenerator& gen) {
    (void)gen;
    std::cout << "[FUNCTION_CODEGEN] SIMPLIFIED: Skipping scope register save" << std::endl;
}

void generate_scope_register_restore(CodeGenerator& gen) {
    (void)gen;
    std::cout << "[FUNCTION_CODEGEN] SIMPLIFIED: Skipping scope register restore" << std::endl;
}
