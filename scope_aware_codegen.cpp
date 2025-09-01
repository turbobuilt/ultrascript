#include "scope_aware_codegen.h"
#include "simple_lexical_scope.h"
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
    : X86CodeGenV2(), scope_analyzer(analyzer) {
    set_current_scope_codegen(this);
}

std::unique_ptr<CodeGenerator> create_scope_aware_codegen(SimpleLexicalScopeAnalyzer* analyzer) {
    return std::make_unique<ScopeAwareCodeGen>(analyzer);
}

// --- STUB IMPLEMENTATIONS FOR COMPILATION ---
// These will be properly implemented once the build system works

void ScopeAwareCodeGen::emit_function_instance_creation(struct FunctionDecl* child_func, size_t func_offset) {
    // STUB: Create function instance using mmap
    std::cout << "[SCOPE_CODEGEN] Creating function instance for " << child_func->name 
              << " at offset " << func_offset << std::endl;
    // TODO: Implement full function instance creation with static analysis data
}

void ScopeAwareCodeGen::emit_function_instance_call(size_t func_offset, const std::vector<std::unique_ptr<ASTNode>>& arguments) {
    // STUB: Call function instance 
    std::cout << "[SCOPE_CODEGEN] Calling function instance at offset " << func_offset 
              << " with " << arguments.size() << " arguments" << std::endl;
    // TODO: Implement full function instance call with argument handling
}

void ScopeAwareCodeGen::emit_function_prologue(struct FunctionDecl* function) {
    std::cout << "[SCOPE_CODEGEN] Generating prologue for " << function->name << std::endl;
    
    // Call the base class method to generate actual machine code
    emit_prologue();
    
    // TODO: Add scope register setup and parameter handling once basic execution works
}

void ScopeAwareCodeGen::emit_function_epilogue(struct FunctionDecl* function) {
    std::cout << "[SCOPE_CODEGEN] Generating epilogue for " << function->name << std::endl;
    
    // Call the base class method to generate actual machine code
    emit_epilogue();
    
    // TODO: Add scope register cleanup once basic execution works
}

void ScopeAwareCodeGen::set_current_scope(LexicalScopeNode* scope) {
    current_scope = scope;
}

void ScopeAwareCodeGen::enter_lexical_scope(LexicalScopeNode* scope_node) {
    current_scope = scope_node;
    setup_parent_scope_registers(scope_node);
}

void ScopeAwareCodeGen::exit_lexical_scope(LexicalScopeNode* scope_node) {
    restore_parent_scope_registers();
    // TODO: Fix when LexicalScopeNode has parent field properly defined
    // current_scope = scope_node->parent;
    current_scope = nullptr; // STUB
}

void ScopeAwareCodeGen::emit_variable_load(const std::string& var_name) {
    // STUB: Variable loading from appropriate scope 
    std::cout << "[SCOPE_CODEGEN] Loading variable " << var_name << std::endl;
    // TODO: Implement scope chain traversal and register/stack access
}

void ScopeAwareCodeGen::emit_variable_store(const std::string& var_name) {
    // STUB: Variable storing to appropriate scope
    std::cout << "[SCOPE_CODEGEN] Storing variable " << var_name << std::endl;
    // TODO: Implement scope chain traversal and register/stack access
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

void ScopeAwareCodeGen::setup_parent_scope_registers(LexicalScopeNode* scope_node) {
    // STUB: Set up r12, r13, r14 based on parent scope frequency
    std::cout << "[SCOPE_CODEGEN] Setting up parent scope registers" << std::endl;
}

void ScopeAwareCodeGen::restore_parent_scope_registers() {
    // STUB: Restore any saved registers from stack
    std::cout << "[SCOPE_CODEGEN] Restoring parent scope registers" << std::endl;
}
