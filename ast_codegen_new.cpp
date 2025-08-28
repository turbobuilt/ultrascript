#include "compiler.h"
#include "x86_codegen_v2.h"
#include "simple_lexical_scope.h"
#include "runtime.h"
#include "runtime_object.h"
#include "compilation_context.h"
#include "function_compilation_manager.h"
#include "console_log_overhaul.h"
#include "class_runtime_interface.h"
#include "dynamic_properties.h"
#include <iostream>
#include <unordered_map>
#include <cstring>
#include <cstdlib>
#include <stdexcept>
#include <string>

// Simple global constant storage for imported constants
static std::unordered_map<std::string, double> global_imported_constants;

// Forward declarations for function ID registry
void __register_function_id(int64_t function_id, const std::string& function_name);
void ensure_lookup_function_by_id_registered();

// Forward declaration for fast runtime function lookup
extern "C" void* __lookup_function_fast(uint16_t func_id);

// NEW SCOPE-AWARE CODE GENERATOR
// This extends X86CodeGenV2 with lexical scope management
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
    } scope_state;
    
    // Current context
    LexicalScopeNode* current_scope = nullptr;
    SimpleLexicalScopeAnalyzer* scope_analyzer = nullptr;
    
    // Type information from parse phase
    std::unordered_map<std::string, DataType> variable_types;
    std::unordered_map<std::string, DataType> variable_array_element_types;
    
public:
    ScopeAwareCodeGen(SimpleLexicalScopeAnalyzer* analyzer) : scope_analyzer(analyzer) {}
    
    // Set the current scope context
    void set_current_scope(LexicalScopeNode* scope) {
        current_scope = scope;
        if (scope) {
            scope_state.current_scope_depth = scope->scope_depth;
        }
    }
    
    // Scope management methods
    void enter_lexical_scope(LexicalScopeNode* scope_node) {
        std::cout << "[NEW_CODEGEN] Entering lexical scope at depth " << scope_node->scope_depth << std::endl;
        
        // Allocate stack space for this scope's variables
        size_t scope_size = scope_node->total_scope_frame_size;
        if (scope_size > 0) {
            emit_sub_reg_imm(4, scope_size); // sub rsp, scope_size
            emit_mov_reg_reg(15, 4); // mov r15, rsp (r15 points to current scope)
            std::cout << "[NEW_CODEGEN] Allocated " << scope_size << " bytes for scope, r15 = rsp" << std::endl;
        }
        
        // Set up parent scope registers based on priority
        setup_parent_scope_registers(scope_node);
        
        // Update current context
        set_current_scope(scope_node);
    }
    
    void exit_lexical_scope(LexicalScopeNode* scope_node) {
        std::cout << "[NEW_CODEGEN] Exiting lexical scope at depth " << scope_node->scope_depth << std::endl;
        
        // Deallocate stack space
        size_t scope_size = scope_node->total_scope_frame_size;
        if (scope_size > 0) {
            emit_add_reg_imm(4, scope_size); // add rsp, scope_size
            std::cout << "[NEW_CODEGEN] Deallocated " << scope_size << " bytes from scope" << std::endl;
        }
        
        // Restore parent scope registers if needed
        restore_parent_scope_registers();
    }
    
    // Variable access methods
    void emit_variable_load(const std::string& var_name) {
        if (!current_scope || !scope_analyzer) {
            throw std::runtime_error("No scope context for variable access: " + var_name);
        }
        
        // Find the scope where this variable is defined
        LexicalScopeNode* def_scope = scope_analyzer->get_definition_scope_for_variable(var_name);
        if (!def_scope) {
            throw std::runtime_error("Variable not found in any scope: " + var_name);
        }
        
        // Get variable offset within its scope
        auto offset_it = def_scope->variable_offsets.find(var_name);
        if (offset_it == def_scope->variable_offsets.end()) {
            throw std::runtime_error("Variable offset not found: " + var_name);
        }
        size_t var_offset = offset_it->second;
        
        if (def_scope == current_scope) {
            // Variable is in current scope - use r15 + offset
            emit_mov_reg_reg_offset(0, 15, var_offset); // rax = [r15 + offset]
            std::cout << "[NEW_CODEGEN] Loaded local variable '" << var_name << "' from r15+" << var_offset << std::endl;
        } else {
            // Variable is in parent scope - find which register holds it
            int scope_depth = def_scope->scope_depth;
            auto reg_it = scope_state.scope_depth_to_register.find(scope_depth);
            
            if (reg_it != scope_state.scope_depth_to_register.end()) {
                // Parent scope is in a register
                int scope_reg = reg_it->second;
                emit_mov_reg_reg_offset(0, scope_reg, var_offset); // rax = [r12/13/14 + offset]
                std::cout << "[NEW_CODEGEN] Loaded parent variable '" << var_name 
                          << "' from r" << scope_reg << "+" << var_offset << std::endl;
            } else {
                // Parent scope is on stack (deep nesting)
                // TODO: Implement stack-based parent scope access
                throw std::runtime_error("Deep nested scope access not yet implemented for: " + var_name);
            }
        }
    }
    
    void emit_variable_store(const std::string& var_name) {
        if (!current_scope || !scope_analyzer) {
            throw std::runtime_error("No scope context for variable store: " + var_name);
        }
        
        // For assignments, the variable should be in current scope
        auto offset_it = current_scope->variable_offsets.find(var_name);
        if (offset_it != current_scope->variable_offsets.end()) {
            // Variable is in current scope
            size_t var_offset = offset_it->second;
            emit_mov_reg_offset_reg(15, var_offset, 0); // [r15 + offset] = rax
            std::cout << "[NEW_CODEGEN] Stored to local variable '" << var_name << "' at r15+" << var_offset << std::endl;
        } else {
            // This might be a reassignment to a parent scope variable
            LexicalScopeNode* def_scope = scope_analyzer->get_definition_scope_for_variable(var_name);
            if (!def_scope) {
                // New variable - add to current scope
                // This would need integration with the scope analyzer
                throw std::runtime_error("Variable assignment to undefined variable: " + var_name);
            } else {
                // Reassignment to parent scope variable
                auto parent_offset_it = def_scope->variable_offsets.find(var_name);
                if (parent_offset_it == def_scope->variable_offsets.end()) {
                    throw std::runtime_error("Parent variable offset not found: " + var_name);
                }
                
                size_t var_offset = parent_offset_it->second;
                int scope_depth = def_scope->scope_depth;
                auto reg_it = scope_state.scope_depth_to_register.find(scope_depth);
                
                if (reg_it != scope_state.scope_depth_to_register.end()) {
                    int scope_reg = reg_it->second;
                    emit_mov_reg_offset_reg(scope_reg, var_offset, 0); // [r12/13/14 + offset] = rax
                    std::cout << "[NEW_CODEGEN] Stored to parent variable '" << var_name 
                              << "' at r" << scope_reg << "+" << var_offset << std::endl;
                } else {
                    throw std::runtime_error("Deep nested scope store not yet implemented for: " + var_name);
                }
            }
        }
    }
    
    // Type context methods (similar to old TypeInference)
    void set_variable_type(const std::string& name, DataType type) {
        variable_types[name] = type;
    }
    
    DataType get_variable_type(const std::string& name) {
        auto it = variable_types.find(name);
        return (it != variable_types.end()) ? it->second : DataType::ANY;
    }
    
private:
    void setup_parent_scope_registers(LexicalScopeNode* scope_node) {
        // Set up parent scope registers based on priority_sorted_parent_scopes
        scope_state.available_scope_registers = {12, 13, 14}; // Reset available registers
        scope_state.scope_depth_to_register.clear();
        
        for (size_t i = 0; i < scope_node->priority_sorted_parent_scopes.size() && i < 3; ++i) {
            int parent_depth = scope_node->priority_sorted_parent_scopes[i];
            int scope_reg = scope_state.available_scope_registers[i];
            
            // Load parent scope pointer into the register
            // TODO: Implement actual parent scope pointer loading
            // For now, assume parent scopes are stored on stack
            int64_t parent_scope_offset = -32 - (parent_depth * 8);  // Placeholder calculation
            emit_mov_reg_mem(scope_reg, parent_scope_offset);  // r12/13/14 = [rbp + parent_offset]
            
            scope_state.scope_depth_to_register[parent_depth] = scope_reg;
            std::cout << "[NEW_CODEGEN] Assigned parent scope depth " << parent_depth 
                      << " to register r" << scope_reg << std::endl;
        }
        
        // Store remaining deep scopes on stack
        for (size_t i = 3; i < scope_node->priority_sorted_parent_scopes.size(); ++i) {
            int deep_scope_depth = scope_node->priority_sorted_parent_scopes[i];
            scope_state.stack_stored_scopes.push_back(deep_scope_depth);
            std::cout << "[NEW_CODEGEN] Deep parent scope depth " << deep_scope_depth 
                      << " will use stack access" << std::endl;
        }
    }
    
    void restore_parent_scope_registers() {
        // TODO: Implement register restoration if needed for nested function calls
        // For basic block scopes, registers don't need restoration
    }
};

// Global codegen instance - this will replace the TypeInference usage
thread_local ScopeAwareCodeGen* g_scope_codegen = nullptr;

// Helper function to get current scope-aware codegen
ScopeAwareCodeGen* get_current_scope_codegen() {
    return g_scope_codegen;
}

// Helper function to set current scope-aware codegen
void set_current_scope_codegen(ScopeAwareCodeGen* codegen) {
    g_scope_codegen = codegen;
}

//=============================================================================
// NEW AST NODE IMPLEMENTATIONS WITHOUT TYPEINFERENCE
//=============================================================================

// NumberLiteral - simplified without TypeInference complexity
void NumberLiteral::generate_code_new(CodeGenerator& gen) {
    std::cout << "[NEW_CODEGEN] NumberLiteral::generate_code - value=" << value << std::endl;
    
    // Get type context from scope-aware codegen if available
    ScopeAwareCodeGen* scope_gen = get_current_scope_codegen();
    if (scope_gen) {
        // TODO: Get assignment context from scope generator
        // For now, just use FLOAT64 for JavaScript compatibility
        gen.emit_mov_reg_imm(0, *reinterpret_cast<const int64_t*>(&value));
        result_type = DataType::FLOAT64;
    } else {
        // Fallback: direct code generation
        union { double d; int64_t i; } converter;
        converter.d = value;
        gen.emit_mov_reg_imm(0, converter.i);
        result_type = DataType::FLOAT64;
    }
}

// Identifier - the most important change
void Identifier::generate_code_new(CodeGenerator& gen) {
    std::cout << "[NEW_CODEGEN] Identifier::generate_code - variable: " << name << std::endl;
    
    // Handle special cases first
    if (name == "true") {
        gen.emit_mov_reg_imm(0, 1);
        result_type = DataType::BOOLEAN;
        return;
    }
    if (name == "false") {
        gen.emit_mov_reg_imm(0, 0);
        result_type = DataType::BOOLEAN;
        return;
    }
    if (name == "runtime") {
        result_type = DataType::RUNTIME_OBJECT;
        return;
    }
    
    // Check global constants
    auto it = global_imported_constants.find(name);
    if (it != global_imported_constants.end()) {
        union { double f; int64_t i; } converter;
        converter.f = it->second;
        gen.emit_mov_reg_imm(0, converter.i);
        result_type = DataType::FLOAT64;
        return;
    }
    
    // Use scope-aware codegen for variable access
    ScopeAwareCodeGen* scope_gen = get_current_scope_codegen();
    if (scope_gen) {
        try {
            scope_gen->emit_variable_load(name);
            result_type = scope_gen->get_variable_type(name);
            std::cout << "[NEW_CODEGEN] Variable '" << name << "' loaded successfully" << std::endl;
            return;
        } catch (const std::exception& e) {
            std::cout << "[NEW_CODEGEN] Variable load failed: " << e.what() << std::endl;
        }
    }
    
    // If we reach here, try implicit 'this.property' access
    // TODO: Implement class property access
    
    throw std::runtime_error("Undefined variable: " + name);
}

// Assignment - completely rewritten
void Assignment::generate_code_new(CodeGenerator& gen) {
    std::cout << "[NEW_CODEGEN] Assignment::generate_code - variable: " << variable_name 
              << ", declared_type=" << static_cast<int>(declared_type) << std::endl;
    
    ScopeAwareCodeGen* scope_gen = get_current_scope_codegen();
    if (!scope_gen) {
        throw std::runtime_error("No scope context for assignment: " + variable_name);
    }
    
    // Generate value first
    if (value) {
        // TODO: Once all nodes have generate_code_new, use that instead
        // For now, create a dummy TypeInference to satisfy the interface
        // This is a temporary hack until we convert all nodes
        struct DummyTypeInference {
            // Minimal implementation to satisfy assignment needs
            void set_variable_type(const std::string& name, DataType type) {}
            DataType get_variable_type(const std::string& name) { return DataType::ANY; }
            void mark_variable_used(const std::string& name) {}
            bool variable_exists(const std::string& name) { return false; }
        } dummy_types;
        
        // This is a temporary solution - we'll need to convert all AST nodes
        throw std::runtime_error("Mixed old/new interface not yet supported");
        
        // Determine variable type
        DataType variable_type;
        if (declared_type != DataType::ANY) {
            variable_type = declared_type;
        } else {
            variable_type = value->result_type;
        }
        
        // Store type information
        scope_gen->set_variable_type(variable_name, variable_type);
        
        // Store the value using scope-aware codegen
        scope_gen->emit_variable_store(variable_name);
        
        std::cout << "[NEW_CODEGEN] Assignment to '" << variable_name << "' completed" << std::endl;
    }
}

// TODO: Add more AST node implementations...
