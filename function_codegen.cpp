#include "function_codegen.h"
#include "compiler.h"
#include "simple_lexical_scope.h"
#include "x86_codegen_v2.h"
#include "scope_aware_codegen.h"  // For ScopeAwareCodeGen
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

// Generate function call code based on the appropriate strategy (SIMPLIFIED VERSION)
void generate_function_call_code(CodeGenerator& gen, 
                                const std::string& function_var_name,
                                SimpleLexicalScopeAnalyzer* analyzer,
                                LexicalScopeNode* current_scope) {
    std::cout << "[NEW_SYSTEM] Using new function call system for '" << function_var_name << "'" << std::endl;
    
    // Cast to the new ScopeAwareCodeGen system
    auto scope_gen = dynamic_cast<ScopeAwareCodeGen*>(&gen);
    if (!scope_gen) {
        throw std::runtime_error("New function system requires ScopeAwareCodeGen");
    }
    
    // Find the function variable in the current scope
    if (!current_scope) {
        throw std::runtime_error("No current scope for function variable lookup");
    }
    
    auto offset_it = current_scope->variable_offsets.find(function_var_name);
    if (offset_it == current_scope->variable_offsets.end()) {
        throw std::runtime_error("Function variable not found in scope: " + function_var_name);
    }
    
    size_t func_var_offset = offset_it->second;
    
    // The function instance is stored at func_var_offset + 8 (skip type tag)
    size_t func_instance_offset = func_var_offset + 8;
    
    // Load function instance address
    scope_gen->emit_mov_reg_reg_offset(0, 15, func_instance_offset); // rax = [r15 + instance_offset]
    
    // Get the actual FunctionDecl to access static analysis
    // For now, we'll implement a simpler version that works with the available data
    
    // Load number of captured scopes from the instance
    scope_gen->emit_mov_reg_reg_offset(1, 0, 16); // rcx = [rax + 16] (num_captured_scopes)
    
    // Push captured scopes in reverse order
    // We need a loop since we don't know the count at compile time in this context
    std::cout << "[NEW_SYSTEM] Pushing captured scopes for function call" << std::endl;
    
    // Simple approach: assume we have the scope count in rcx, push in reverse order
    // This is a simplified version - in reality we'd need the static analysis data
    
    // For now, call the function code address directly
    scope_gen->emit_call_reg_offset(0, 8); // call [rax + 8] (function_code_addr)
    
    std::cout << "[NEW_SYSTEM] Function call completed using new system" << std::endl;
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
