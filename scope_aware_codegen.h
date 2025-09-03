#ifndef SCOPE_AWARE_CODEGEN_H
#define SCOPE_AWARE_CODEGEN_H

#include "x86_codegen_v2.h"
#include "compiler.h"
#include "static_analyzer.h"
#include <unordered_map>
#include <unordered_set>

// State structure for managing scope-aware code generation
struct ScopeState {
    std::unordered_set<int> registers_in_use;
    LexicalScopeNode* current_scope = nullptr;
};

// Complete implementation of scope-aware code generation per FUNCTION.md
class ScopeAwareCodeGen : public X86CodeGenV2 {
private:
    StaticAnalyzer* static_analyzer_ = nullptr;
    SimpleLexicalScopeAnalyzer* scope_analyzer = nullptr;
    std::unordered_map<std::string, DataType> variable_types;
    ScopeState scope_state;
    LexicalScopeNode* current_scope = nullptr;

public:
    // Constructors for both analyzer types
    explicit ScopeAwareCodeGen(SimpleLexicalScopeAnalyzer* analyzer);
    explicit ScopeAwareCodeGen(StaticAnalyzer* analyzer);

    // FUNCTION.md core methods (no override since base methods aren't virtual)
    void emit_function_prologue(struct FunctionDecl* function);
    void emit_function_epilogue(struct FunctionDecl* function);
    
    // Function instance management
    void emit_function_instance_creation(struct FunctionDecl* child_func, size_t func_offset);
    void emit_function_instance_call(size_t func_offset, const std::vector<std::unique_ptr<ASTNode>>& arguments);
    
    // Scope management methods
    void set_current_scope(LexicalScopeNode* scope);
    void enter_lexical_scope(LexicalScopeNode* scope_node);
    void setup_parent_scope_registers(LexicalScopeNode* scope_node);
    
    // Scope analysis integration
    LexicalScopeNode* get_scope_node_for_depth(int depth);
    LexicalScopeNode* get_definition_scope_for_variable(const std::string& name);
    void perform_deferred_packing_for_scope(LexicalScopeNode* scope_node);
    VariableDeclarationInfo* get_variable_declaration_info(const std::string& name);
    
    // Type management
    void set_variable_type(const std::string& name, DataType type);
    DataType get_variable_type(const std::string& name);
    
    // Register state management
    void mark_register_in_use(int reg_id);
    void mark_register_free(int reg_id);
    bool is_register_in_use(int reg_id);
};

// Factory functions
std::unique_ptr<CodeGenerator> create_scope_aware_codegen(SimpleLexicalScopeAnalyzer* analyzer);
std::unique_ptr<CodeGenerator> create_scope_aware_codegen_with_static_analyzer(StaticAnalyzer* analyzer);

// Global state management
ScopeAwareCodeGen* get_current_scope_codegen();
void set_current_scope_codegen(ScopeAwareCodeGen* codegen);

#endif // SCOPE_AWARE_CODEGEN_H