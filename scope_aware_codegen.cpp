#include "scope_aware_codegen.h"
#include "simple_lexical_scope.h"
#include "static_analyzer.h"
#include "compiler.h"
#include <cstring>
#include <sys/mman.h>

// Global instance tracking for bridge functions
static ScopeAwareCodeGen* current_scope_codegen = nullptr;

ScopeAwareCodeGen* get_current_scope_codegen() {
    return current_scope_codegen;
}

void set_current_scope_codegen(ScopeAwareCodeGen* codegen) {
    current_scope_codegen = codegen;
}

ScopeAwareCodeGen::ScopeAwareCodeGen(SimpleLexicalScopeAnalyzer* analyzer) 
    : X86CodeGenV2(), scope_analyzer(analyzer), static_analyzer_(nullptr) {
    set_current_scope_codegen(this);
}

ScopeAwareCodeGen::ScopeAwareCodeGen(StaticAnalyzer* analyzer) 
    : X86CodeGenV2(), scope_analyzer(nullptr), static_analyzer_(analyzer) {
    set_current_scope_codegen(this);
    std::cout << "[NEW_SYSTEM] ScopeAwareCodeGen created with StaticAnalyzer" << std::endl;
}

std::unique_ptr<CodeGenerator> create_scope_aware_codegen(SimpleLexicalScopeAnalyzer* analyzer) {
    return std::make_unique<ScopeAwareCodeGen>(analyzer);
}

// Minimal stub implementations for removed methods
void ScopeAwareCodeGen::emit_function_instance_creation(struct FunctionDecl* child_func, size_t func_offset) {
    // Method removed - this is a stub to prevent compile errors
    std::cout << "[REMOVED] emit_function_instance_creation called - method removed" << std::endl;
}

void ScopeAwareCodeGen::emit_function_instance_call(size_t func_offset, const std::vector<std::unique_ptr<ASTNode>>& arguments) {
    // Method removed - this is a stub to prevent compile errors
    std::cout << "[REMOVED] emit_function_instance_call called - method removed" << std::endl;
}

// Legacy method stub
void ScopeAwareCodeGen::setup_parent_scope_registers(LexicalScopeNode* scope_node) {
    // DEPRECATED STUB - Parent scopes now accessed via runtime lookup
    (void)scope_node; // Suppress unused parameter warnings
}

std::unique_ptr<CodeGenerator> create_scope_aware_codegen_with_static_analyzer(StaticAnalyzer* analyzer) {
    return std::make_unique<ScopeAwareCodeGen>(analyzer);
}

void ScopeAwareCodeGen::emit_function_prologue(struct FunctionDecl* function) {
    std::cout << "[NEW_FUNCTION_SYSTEM] Generating prologue for " << function->name 
              << " using hidden parameter approach" << std::endl;
    
    X86CodeGenV2* x86_gen = dynamic_cast<X86CodeGenV2*>(this);
    if (!x86_gen) {
        throw std::runtime_error("New function system requires X86CodeGenV2");
    }
    
    // Standard function prologue
    x86_gen->emit_push_reg(5);       // push rbp
    x86_gen->emit_mov_reg_reg(5, 4); // mov rbp, rsp
    
    // Allocate local scope on heap for the function's variables
    if (function->lexical_scope && function->lexical_scope->total_scope_frame_size > 0) {
        size_t local_scope_size = function->lexical_scope->total_scope_frame_size;
        std::cout << "[NEW_FUNCTION_SYSTEM] Allocating " << local_scope_size 
                  << " bytes for function local scope" << std::endl;
        
        x86_gen->emit_mov_reg_imm(7, local_scope_size); // RDI = size
        x86_gen->emit_call("malloc");                   // RAX = allocated memory
        x86_gen->emit_mov_reg_reg(15, 0);              // R15 = local scope address
        
        // Initialize local scope to zeros
        if (local_scope_size <= 64) {
            // Small scope: direct zero writes
            for (size_t i = 0; i < local_scope_size; i += 8) {
                x86_gen->emit_mov_reg_imm(0, 0);
                x86_gen->emit_mov_reg_offset_reg(15, i, 0);
            }
        } else {
            // Large scope: use memset
            x86_gen->emit_mov_reg_reg(7, 15);           // RDI = scope memory
            x86_gen->emit_mov_reg_imm(6, 0);            // RSI = 0 (fill value)
            x86_gen->emit_mov_reg_imm(2, local_scope_size); // RDX = size
            x86_gen->emit_call("memset");
        }
    } else {
        // Even functions without variables get a minimal scope allocation
        x86_gen->emit_mov_reg_imm(7, 8);             // RDI = 8 bytes minimum
        x86_gen->emit_call("malloc");
        x86_gen->emit_mov_reg_reg(15, 0);            // R15 = minimal scope
        std::cout << "[NEW_FUNCTION_SYSTEM] Allocated minimal scope for " << function->name << std::endl;
    }
    
    // NEW: Receive parent scope addresses as hidden parameters
    // These are passed after the regular function arguments
    if (function->lexical_scope && !function->lexical_scope->priority_sorted_parent_scopes.empty()) {
        const auto& needed_scopes = function->lexical_scope->priority_sorted_parent_scopes;
        std::cout << "[NEW_FUNCTION_SYSTEM] Loading " << needed_scopes.size() 
                  << " parent scope addresses from hidden parameters" << std::endl;
        
        // Parent scopes are passed as hidden parameters after regular arguments
        // They appear on the stack above the return address and saved RBP
        // Stack layout: ... | arg6 | arg7 | ... | scope0 | scope1 | ... | return_addr | saved_rbp <- rbp
        
        // Calculate base offset for hidden parameters
        // Regular function arguments (beyond first 6) start at rbp+16
        // Hidden scope parameters come after all regular arguments
        size_t num_regular_args = function->parameters.size();
        size_t stack_args = (num_regular_args > 6) ? num_regular_args - 6 : 0;
        int64_t hidden_param_base_offset = 16 + (stack_args * 8);
        
        // Store parent scope addresses in a simple array for easy access
        // For now, we'll use stack-based storage for the mapping
        for (size_t i = 0; i < needed_scopes.size(); i++) {
            int scope_depth = needed_scopes[i];
            int64_t param_offset = hidden_param_base_offset + (i * 8);
            
            // Load scope address from stack parameter
            x86_gen->emit_mov_reg_reg_offset(10 + i, 5, param_offset); // R10+i = [rbp + offset]
            
            // DISABLED: Runtime scope registration violates FUNCTION.md
            // Register this scope address with the runtime for quick lookup
            // x86_gen->emit_mov_reg_imm(7, scope_depth);     // RDI = scope depth
            // x86_gen->emit_mov_reg_reg(6, 10 + i);          // RSI = scope address
            // x86_gen->emit_call("__register_scope_address_for_depth");
            
            std::cout << "[NEW_FUNCTION_SYSTEM] Loaded parent scope depth " << scope_depth
                      << " from stack offset " << param_offset << " into R" << (10 + i) << std::endl;
        }
    }
    
    // DISABLED: Runtime scope registration violates FUNCTION.md
    // Register current function scope with the runtime
    // if (function->lexical_scope) {
    //     x86_gen->emit_mov_reg_imm(7, function->lexical_scope->scope_depth); // RDI = scope depth
    //     x86_gen->emit_mov_reg_reg(6, 15);                                   // RSI = local scope address
    //     x86_gen->emit_call("__register_scope_address_for_depth");
    //     std::cout << "[NEW_FUNCTION_SYSTEM] Registered function scope depth " 
    //               << function->lexical_scope->scope_depth << " with runtime" << std::endl;
    // }
    
    std::cout << "[NEW_FUNCTION_SYSTEM] Prologue complete for " << function->name 
              << " using new hidden parameter system" << std::endl;
}

void ScopeAwareCodeGen::emit_function_epilogue(struct FunctionDecl* function) {
    std::cout << "[NEW_FUNCTION_SYSTEM] Generating epilogue for " << function->name 
              << " using new approach" << std::endl;
    
    X86CodeGenV2* x86_gen = dynamic_cast<X86CodeGenV2*>(this);
    if (!x86_gen) {
        throw std::runtime_error("New function system requires X86CodeGenV2");
    }
    
    // DISABLED: Runtime scope unregistration violates FUNCTION.md
    // Unregister current function scope from the runtime
    // if (function->lexical_scope) {
    //     x86_gen->emit_mov_reg_imm(7, function->lexical_scope->scope_depth); // RDI = scope depth
    //     x86_gen->emit_call("__unregister_scope_address_for_depth");
    //     std::cout << "[NEW_FUNCTION_SYSTEM] Unregistered function scope depth " 
    //               << function->lexical_scope->scope_depth << " from runtime" << std::endl;
    // }
    
    // Free the local scope memory
    x86_gen->emit_mov_reg_reg(7, 15);  // RDI = local scope address (R15)
    x86_gen->emit_call("free");        // Free heap-allocated scope
    std::cout << "[NEW_FUNCTION_SYSTEM] Freed local scope memory for " << function->name << std::endl;
    
    // Standard function epilogue
    x86_gen->emit_mov_reg_reg(4, 5);   // mov rsp, rbp
    x86_gen->emit_pop_reg(5);          // pop rbp
    x86_gen->emit_ret();               // ret
    
    std::cout << "[NEW_FUNCTION_SYSTEM] Epilogue complete for " << function->name 
              << " using new system" << std::endl;
}

void ScopeAwareCodeGen::set_current_scope(LexicalScopeNode* scope) {
    current_scope = scope;
    
    // Also update the global scope context that variable access code uses
    ::set_current_scope(scope);  // Call the global function
}

LexicalScopeNode* ScopeAwareCodeGen::get_scope_node_for_depth(int depth) {
    if (static_analyzer_) {
        return static_analyzer_->get_scope_node_for_depth(depth);
    } else if (scope_analyzer) {
        return scope_analyzer->get_scope_node_for_depth(depth);
    }
    return nullptr;
}

LexicalScopeNode* ScopeAwareCodeGen::get_definition_scope_for_variable(const std::string& name) {
    if (static_analyzer_) {
        return static_analyzer_->get_definition_scope_for_variable(name);
    } else if (scope_analyzer) {
        return scope_analyzer->get_definition_scope_for_variable(name);
    }
    return nullptr;
}

void ScopeAwareCodeGen::perform_deferred_packing_for_scope(LexicalScopeNode* scope_node) {
    if (static_analyzer_) {
        static_analyzer_->perform_deferred_packing_for_scope(scope_node);
    } else if (scope_analyzer) {
        scope_analyzer->perform_deferred_packing_for_scope(scope_node);
    }
}

VariableDeclarationInfo* ScopeAwareCodeGen::get_variable_declaration_info(const std::string& name) {
    if (static_analyzer_) {
        // For static analyzer, we need to get the current scope depth for lookup
        int current_depth = current_scope ? current_scope->scope_depth : 1;
        return static_analyzer_->get_variable_declaration_info(name, current_depth);
    } else if (scope_analyzer) {
        return scope_analyzer->get_variable_declaration_info(name);
    }
    return nullptr;
}

void ScopeAwareCodeGen::enter_lexical_scope(LexicalScopeNode* scope_node) {
    current_scope = scope_node;
    setup_parent_scope_registers(scope_node);
}



void ScopeAwareCodeGen::set_variable_type(const std::string& name, DataType type) {
    variable_types[name] = type;
}

DataType ScopeAwareCodeGen::get_variable_type(const std::string& name) {
    auto it = variable_types.find(name);
    if (it != variable_types.end()) {
        return it->second;
    }
    return DataType::UNKNOWN; 
}

void ScopeAwareCodeGen::mark_register_in_use(int reg_id) {
    scope_state.registers_in_use.insert(reg_id);
}

void ScopeAwareCodeGen::mark_register_free(int reg_id) {
    scope_state.registers_in_use.erase(reg_id);
}

bool ScopeAwareCodeGen::is_register_in_use(int reg_id) {
    return scope_state.registers_in_use.count(reg_id) > 0;
}


