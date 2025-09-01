#pragma once

#include "x86_codegen_v2.h"
#include "simple_lexical_scope.h"
#include "compiler.h"
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <memory>

// Forward declarations
class SimpleLexicalScopeAnalyzer;
class LexicalScopeNode;

// NEW SCOPE-AWARE CODE GENERATOR
// This extends X86CodeGenV2 with lexical scope management and pure machine code generation
class ScopeAwareCodeGen : public X86CodeGenV2 {
private:
    // Scope register management 
    // r15 = current scope (always)
    // r12, r13, r14 = parent scopes in order of access frequency
    struct ScopeRegisterState {
        int current_scope_depth = 0;
        std::unordered_map<int, int> scope_depth_to_register;  // scope_depth -> register_id (12,13,14)
        std::vector<int> available_scope_registers = {12, 13, 14};
        std::vector<int> stack_stored_scopes; // scopes that couldn't fit in registers
        
        // Register preservation tracking
        std::unordered_set<int> registers_in_use;  // which of r12,r13,r14 are currently used
        std::unordered_set<int> registers_saved_to_stack;  // which registers we've pushed to stack
        std::vector<int> register_save_order;  // order in which registers were saved (for proper restore)
    } scope_state;
    
    // Current context
    LexicalScopeNode* current_scope = nullptr;
    SimpleLexicalScopeAnalyzer* scope_analyzer = nullptr;
    
    // Type information from parse phase
    std::unordered_map<std::string, DataType> variable_types;
    std::unordered_map<std::string, DataType> variable_array_element_types;

public:
    ScopeAwareCodeGen(SimpleLexicalScopeAnalyzer* analyzer);
    
    // --- NEW FUNCTION INSTANCE CREATION ---
    void emit_function_instance_creation(struct FunctionDecl* child_func, size_t func_offset);
    
    // --- NEW FUNCTION CALL EMISSION ---
    void emit_function_instance_call(size_t func_offset, const std::vector<std::unique_ptr<ASTNode>>& arguments);
    
    // --- NEW FUNCTION PROLOGUE/EPILOGUE GENERATION ---
    void emit_function_prologue(struct FunctionDecl* function);
    void emit_function_epilogue(struct FunctionDecl* function);
    
    // Set the current scope context
    void set_current_scope(LexicalScopeNode* scope);
    
    // Scope management methods
    void enter_lexical_scope(LexicalScopeNode* scope_node);
    void exit_lexical_scope(LexicalScopeNode* scope_node);
    
    // Variable access methods
    void emit_variable_load(const std::string& var_name);
    void emit_variable_store(const std::string& var_name);
    
    // Type context methods
    void set_variable_type(const std::string& name, DataType type);
    DataType get_variable_type(const std::string& name);
    
    // Register usage tracking methods
    void mark_register_in_use(int reg_id);
    void mark_register_free(int reg_id);
    bool is_register_in_use(int reg_id);

private:
    void setup_parent_scope_registers(LexicalScopeNode* scope_node);
    void restore_parent_scope_registers();
};

// Factory function to create new scope-aware code generator
std::unique_ptr<CodeGenerator> create_scope_aware_codegen(SimpleLexicalScopeAnalyzer* analyzer);

// Global codegen instance helper functions
ScopeAwareCodeGen* get_current_scope_codegen();
void set_current_scope_codegen(ScopeAwareCodeGen* codegen);
