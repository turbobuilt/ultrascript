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

// Generate function call code with proper stack-based scope passing
void generate_function_call_code(CodeGenerator& gen, 
                                const std::string& function_var_name,
                                SimpleLexicalScopeAnalyzer* analyzer,
                                LexicalScopeNode* current_scope) {
    
    std::cout << "[FUNCTION_CODEGEN] STACK-BASED generate_function_call_code - function: " << function_var_name << std::endl;
    
    X86CodeGenV2* x86_gen = dynamic_cast<X86CodeGenV2*>(&gen);
    if (!x86_gen) {
        throw std::runtime_error("Function calls require X86CodeGenV2");
    }
    
    // Find the function declaration to determine what scopes it needs
    auto function_decl_it = g_scope_context.function_lookup.find(function_var_name);
    if (function_decl_it == g_scope_context.function_lookup.end()) {
        std::cout << "[FUNCTION_CODEGEN] WARNING: Function '" << function_var_name << "' not found in lookup, using direct call" << std::endl;
        x86_gen->emit_call(function_var_name);
        return;
    }
    
    FunctionDecl* target_function = function_decl_it->second;
    if (!target_function->lexical_scope) {
        std::cout << "[FUNCTION_CODEGEN] Function '" << function_var_name << "' has no lexical scope, using direct call" << std::endl;
        x86_gen->emit_call(function_var_name);
        return;
    }
    
    // Get the scopes the target function needs access to
    const std::vector<int>& required_scopes = target_function->lexical_scope->priority_sorted_parent_scopes;
    
    std::cout << "[FUNCTION_CODEGEN] Function '" << function_var_name << "' needs " 
              << required_scopes.size() << " parent scopes" << std::endl;
    
    // Push required scope addresses onto stack in reverse order
    for (int i = required_scopes.size() - 1; i >= 0; i--) {
        int required_depth = required_scopes[i];
        
        // Find the scope address - it should be in our current registers or accessible
        if (required_depth == g_scope_context.current_scope_depth) {
            // Current scope - use R15
            std::cout << "[FUNCTION_CODEGEN] Pushing current scope (depth " << required_depth << ") from R15" << std::endl;
            x86_gen->emit_push_reg(15); // push r15
        } else {
            // Parent scope - check if it's in registers
            auto reg_it = g_scope_context.scope_state.scope_depth_to_register.find(required_depth);
            if (reg_it != g_scope_context.scope_state.scope_depth_to_register.end()) {
                int scope_reg = reg_it->second;
                std::cout << "[FUNCTION_CODEGEN] Pushing parent scope (depth " << required_depth 
                          << ") from register R" << scope_reg << std::endl;
                x86_gen->emit_push_reg(scope_reg);
            } else {
                // This shouldn't happen in a properly compiled program
                std::cout << "[FUNCTION_CODEGEN] ERROR: Required scope depth " << required_depth 
                          << " not available in registers" << std::endl;
                throw std::runtime_error("Required scope not available for function call");
            }
        }
    }
    
    // Call the function - it will load scope addresses from stack
    std::cout << "[FUNCTION_CODEGEN] Calling function '" << function_var_name 
              << "' with " << required_scopes.size() << " scope addresses on stack" << std::endl;
    x86_gen->emit_call(function_var_name);
    
    // Clean up the scope addresses from stack (caller cleans up)
    if (required_scopes.size() > 0) {
        int stack_cleanup = required_scopes.size() * 8; // 8 bytes per scope address
        std::cout << "[FUNCTION_CODEGEN] Cleaning up " << stack_cleanup << " bytes from stack" << std::endl;
        x86_gen->emit_add_reg_imm(4, stack_cleanup); // add rsp, stack_cleanup
    }
    
    std::cout << "[FUNCTION_CODEGEN] STACK-BASED: Generated function call successfully" << std::endl;
}

//=============================================================================
// MAIN FUNCTION CALL GENERATION
//=============================================================================

void generate_function_call(CodeGenerator& gen, const std::string& function_var_name, 
                           const SimpleLexicalScopeAnalyzer* analyzer) {
    
    std::cout << "[FUNCTION_CODEGEN] SIMPLIFIED FunctionCall::generate_code - function: " << function_var_name << std::endl;
    
    // SIMPLIFIED APPROACH: Direct call by function name
    // This bypasses the complex function instance system for now
    
    X86CodeGenV2* x86_gen = dynamic_cast<X86CodeGenV2*>(&gen);
    if (!x86_gen) {
        throw std::runtime_error("Function calls require X86CodeGenV2");
    }
    
    std::cout << "[FUNCTION_CODEGEN] SIMPLIFIED: Generating direct call to function '" 
              << function_var_name << "'" << std::endl;
    
    // Direct call to the function label - this will be resolved at link time
    x86_gen->emit_call(function_var_name);
    
    std::cout << "[FUNCTION_CODEGEN] SIMPLIFIED: Generated direct call successfully" << std::endl;
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
