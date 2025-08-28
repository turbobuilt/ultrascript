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

// Forward declaration for scope context initialization
void initialize_scope_context(SimpleLexicalScopeAnalyzer* analyzer);

//=============================================================================
// NEW LEXICAL SCOPE-AWARE CODE GENERATION SYSTEM
// This system completely replaces TypeInference with direct scope management
//=============================================================================

// Global scope context that replaces TypeInference
struct GlobalScopeContext {
    // Current scope information from SimpleLexicalScopeAnalyzer
    LexicalScopeNode* current_scope = nullptr;
    SimpleLexicalScopeAnalyzer* scope_analyzer = nullptr;
    
    // Scope register management
    // r15 always points to current scope
    // r12, r13, r14 point to parent scopes in order of frequency
    struct ScopeRegisterState {
        int current_scope_depth = 0;
        std::unordered_map<int, int> scope_depth_to_register;  // scope_depth -> register_id (12,13,14)
        std::vector<int> available_scope_registers = {12, 13, 14};
        std::vector<int> stack_stored_scopes; // scopes that couldn't fit in registers
    } scope_state;
    
    // Type information extracted from parse phase
    std::unordered_map<std::string, DataType> variable_types;
    std::unordered_map<std::string, DataType> variable_array_element_types;
    std::unordered_map<std::string, std::string> variable_class_names;
    
    // Assignment context tracking (similar to old TypeInference)
    DataType current_assignment_target_type = DataType::ANY;
    DataType current_assignment_array_element_type = DataType::ANY;
    DataType current_element_type_context = DataType::ANY;
    DataType current_property_assignment_type = DataType::ANY;
    
    // Current class context for 'this' handling
    std::string current_class_name;
};

// Global scope context instance (thread-local for safety)
thread_local GlobalScopeContext g_scope_context;

// Helper functions to manage scope context
void initialize_scope_context(SimpleLexicalScopeAnalyzer* analyzer) {
    g_scope_context.scope_analyzer = analyzer;
    g_scope_context.current_scope = nullptr;
}

void set_current_scope(LexicalScopeNode* scope) {
    g_scope_context.current_scope = scope;
    if (scope) {
        g_scope_context.scope_state.current_scope_depth = scope->scope_depth;
    }
}

LexicalScopeNode* get_current_scope() {
    return g_scope_context.current_scope;
}

//=============================================================================
// SCOPE-AWARE CODE GENERATION HELPERS
//=============================================================================

// Set up parent scope registers based on priority
void setup_parent_scope_registers(X86CodeGenV2* x86_gen, LexicalScopeNode* scope_node) {
    // Reset scope register state
    g_scope_context.scope_state.available_scope_registers = {12, 13, 14};
    g_scope_context.scope_state.scope_depth_to_register.clear();
    
    // Assign parent scope registers based on priority
    for (size_t i = 0; i < scope_node->priority_sorted_parent_scopes.size() && i < 3; ++i) {
        int parent_depth = scope_node->priority_sorted_parent_scopes[i];
        int scope_reg = g_scope_context.scope_state.available_scope_registers[i];
        
        // Load parent scope pointer into the register
        // For now, use a simple stack-based approach
        int64_t parent_scope_offset = -32 - (parent_depth * 8);  // Placeholder calculation
        x86_gen->emit_mov_reg_mem(scope_reg, parent_scope_offset);  // r12/13/14 = [rbp + parent_offset]
        
        g_scope_context.scope_state.scope_depth_to_register[parent_depth] = scope_reg;
        std::cout << "[SCOPE_CODEGEN] Assigned parent scope depth " << parent_depth 
                  << " to register r" << scope_reg << std::endl;
    }
    
    // Store remaining deep scopes for stack access
    for (size_t i = 3; i < scope_node->priority_sorted_parent_scopes.size(); ++i) {
        int deep_scope_depth = scope_node->priority_sorted_parent_scopes[i];
        g_scope_context.scope_state.stack_stored_scopes.push_back(deep_scope_depth);
        std::cout << "[SCOPE_CODEGEN] Deep parent scope depth " << deep_scope_depth 
                  << " will use stack access" << std::endl;
    }
}

// Generate code to enter a lexical scope
void emit_scope_enter(CodeGenerator& gen, LexicalScopeNode* scope_node) {
    std::cout << "[SCOPE_CODEGEN] Entering lexical scope at depth " << scope_node->scope_depth << std::endl;
    
    // Cast to X86CodeGenV2 to access scope management methods
    X86CodeGenV2* x86_gen = dynamic_cast<X86CodeGenV2*>(&gen);
    if (!x86_gen) {
        throw std::runtime_error("Scope management requires X86CodeGenV2");
    }
    
    // Allocate stack space for this scope's variables
    size_t scope_size = scope_node->total_scope_frame_size;
    if (scope_size > 0) {
        gen.emit_sub_reg_imm(4, scope_size); // sub rsp, scope_size
        gen.emit_mov_reg_reg(15, 4); // mov r15, rsp (r15 points to current scope)
        std::cout << "[SCOPE_CODEGEN] Allocated " << scope_size << " bytes for scope, r15 = rsp" << std::endl;
    }
    
    // Set up parent scope registers based on priority
    setup_parent_scope_registers(x86_gen, scope_node);
    
    // Update current context
    set_current_scope(scope_node);
}

// Generate code to exit a lexical scope
void emit_scope_exit(CodeGenerator& gen, LexicalScopeNode* scope_node) {
    std::cout << "[SCOPE_CODEGEN] Exiting lexical scope at depth " << scope_node->scope_depth << std::endl;
    
    // Deallocate stack space
    size_t scope_size = scope_node->total_scope_frame_size;
    if (scope_size > 0) {
        gen.emit_add_reg_imm(4, scope_size); // add rsp, scope_size
        std::cout << "[SCOPE_CODEGEN] Deallocated " << scope_size << " bytes from scope" << std::endl;
    }
    
    // Restore parent context
    // TODO: Implement parent context restoration if needed
}

// Generate variable load code using scope-aware access
void emit_variable_load(CodeGenerator& gen, const std::string& var_name) {
    if (!g_scope_context.current_scope || !g_scope_context.scope_analyzer) {
        throw std::runtime_error("No scope context for variable access: " + var_name);
    }
    
    // Find the scope where this variable is defined
    auto def_scope = g_scope_context.scope_analyzer->get_definition_scope_for_variable(var_name);
    if (!def_scope) {
        throw std::runtime_error("Variable not found in any scope: " + var_name);
    }
    
    // Get variable offset within its scope
    auto offset_it = def_scope->variable_offsets.find(var_name);
    if (offset_it == def_scope->variable_offsets.end()) {
        throw std::runtime_error("Variable offset not found: " + var_name);
    }
    size_t var_offset = offset_it->second;
    
    if (def_scope == g_scope_context.current_scope) {
        // Variable is in current scope - use r15 + offset
        gen.emit_mov_reg_reg_offset(0, 15, var_offset); // rax = [r15 + offset]
        std::cout << "[SCOPE_CODEGEN] Loaded local variable '" << var_name << "' from r15+" << var_offset << std::endl;
    } else {
        // Variable is in parent scope - find which register holds it
        int scope_depth = def_scope->scope_depth;
        auto reg_it = g_scope_context.scope_state.scope_depth_to_register.find(scope_depth);
        
        if (reg_it != g_scope_context.scope_state.scope_depth_to_register.end()) {
            // Parent scope is in a register
            int scope_reg = reg_it->second;
            gen.emit_mov_reg_reg_offset(0, scope_reg, var_offset); // rax = [r12/13/14 + offset]
            std::cout << "[SCOPE_CODEGEN] Loaded parent variable '" << var_name 
                      << "' from r" << scope_reg << "+" << var_offset << std::endl;
        } else {
            // Parent scope is on stack (deep nesting)
            throw std::runtime_error("Deep nested scope access not yet implemented for: " + var_name);
        }
    }
}

// Generate variable store code using scope-aware access
void emit_variable_store(CodeGenerator& gen, const std::string& var_name) {
    if (!g_scope_context.current_scope || !g_scope_context.scope_analyzer) {
        throw std::runtime_error("No scope context for variable store: " + var_name);
    }
    
    // For assignments, the variable should typically be in current scope
    std::cout << "[SCOPE_DEBUG] Looking for variable '" << var_name << "' in current scope" << std::endl;
    std::cout << "[SCOPE_DEBUG] Current scope node address: " << (void*)g_scope_context.current_scope << std::endl;
    std::cout << "[SCOPE_DEBUG] Current scope has " << g_scope_context.current_scope->variable_offsets.size() << " variables:" << std::endl;
    for (const auto& pair : g_scope_context.current_scope->variable_offsets) {
        std::cout << "[SCOPE_DEBUG]   " << pair.first << " -> offset " << pair.second << std::endl;
    }
    
    auto offset_it = g_scope_context.current_scope->variable_offsets.find(var_name);
    if (offset_it != g_scope_context.current_scope->variable_offsets.end()) {
        // Variable is in current scope
        size_t var_offset = offset_it->second;
        gen.emit_mov_reg_offset_reg(15, var_offset, 0); // [r15 + offset] = rax
        std::cout << "[SCOPE_CODEGEN] Stored to local variable '" << var_name << "' at r15+" << var_offset << std::endl;
    } else {
        // This might be a reassignment to a parent scope variable
        std::cout << "[SCOPE_DEBUG] Variable '" << var_name << "' not in current scope, checking parent scopes" << std::endl;
        auto def_scope = g_scope_context.scope_analyzer->get_definition_scope_for_variable(var_name);
        std::cout << "[SCOPE_DEBUG] get_definition_scope_for_variable returned: " << def_scope << std::endl;
        if (!def_scope) {
            throw std::runtime_error("Variable assignment to undefined variable: " + var_name);
        } else {
            // Reassignment to parent scope variable
            auto parent_offset_it = def_scope->variable_offsets.find(var_name);
            if (parent_offset_it == def_scope->variable_offsets.end()) {
                throw std::runtime_error("Parent variable offset not found: " + var_name);
            }
            
            size_t var_offset = parent_offset_it->second;
            int scope_depth = def_scope->scope_depth;
            auto reg_it = g_scope_context.scope_state.scope_depth_to_register.find(scope_depth);
            
            if (reg_it != g_scope_context.scope_state.scope_depth_to_register.end()) {
                int scope_reg = reg_it->second;
                gen.emit_mov_reg_offset_reg(scope_reg, var_offset, 0); // [r12/13/14 + offset] = rax
                std::cout << "[SCOPE_CODEGEN] Stored to parent variable '" << var_name 
                          << "' at r" << scope_reg << "+" << var_offset << std::endl;
            } else {
                throw std::runtime_error("Deep nested scope store not yet implemented for: " + var_name);
            }
        }
    }
}

//=============================================================================
// NEW AST NODE IMPLEMENTATIONS WITHOUT TYPEINFERENCE
// These replace all the old generate_code methods that used TypeInference
//=============================================================================

void NumberLiteral::generate_code(CodeGenerator& gen) {
    std::cout << "[NEW_CODEGEN] NumberLiteral::generate_code - value=" << value << std::endl;
    
    // For JavaScript compatibility, always treat numbers as FLOAT64 by default
    union { double d; int64_t i; } converter;
    converter.d = value;
    gen.emit_mov_reg_imm(0, converter.i);
    result_type = DataType::FLOAT64;
    
    std::cout << "[NEW_CODEGEN] NumberLiteral: Generated float64 value " << value << std::endl;
}

void StringLiteral::generate_code(CodeGenerator& gen) {
    std::cout << "[NEW_CODEGEN] StringLiteral::generate_code - value=\"" << value << "\"" << std::endl;
    
    // TODO: Implement string literal code generation
    // For now, just put a null pointer in RAX
    gen.emit_mov_reg_imm(0, 0);
    result_type = DataType::STRING;
    
    std::cout << "[NEW_CODEGEN] StringLiteral: Generated string (placeholder implementation)" << std::endl;
}

void Identifier::generate_code(CodeGenerator& gen) {
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
    
    // Use scope-aware variable access
    try {
        emit_variable_load(gen, name);
        // Get variable type from scope context
        auto type_it = g_scope_context.variable_types.find(name);
        result_type = (type_it != g_scope_context.variable_types.end()) ? type_it->second : DataType::ANY;
        std::cout << "[NEW_CODEGEN] Variable '" << name << "' loaded successfully" << std::endl;
        return;
    } catch (const std::exception& e) {
        std::cout << "[NEW_CODEGEN] Variable load failed: " << e.what() << std::endl;
    }
    
    // TODO: Try implicit 'this.property' access for class methods
    
    throw std::runtime_error("Undefined variable: " + name);
}

void Assignment::generate_code(CodeGenerator& gen) {
    std::cout << "[NEW_CODEGEN] Assignment::generate_code - variable: " << variable_name 
              << ", declared_type=" << static_cast<int>(declared_type) << std::endl;
    
    // Generate value first
    if (value) {
        value->generate_code(gen);
        
        // Determine variable type
        DataType variable_type;
        if (declared_type != DataType::ANY) {
            variable_type = declared_type;
        } else {
            variable_type = value->result_type;
        }
        
        // Store type information in scope context
        g_scope_context.variable_types[variable_name] = variable_type;
        
        // Store the value using scope-aware codegen
        emit_variable_store(gen, variable_name);
        
        result_type = variable_type;
        std::cout << "[NEW_CODEGEN] Assignment to '" << variable_name << "' completed" << std::endl;
    }
}

// TODO: Implement more AST nodes using the same pattern
// For now, let's implement minimal versions that don't crash

void BinaryOp::generate_code(CodeGenerator& gen) {
    std::cout << "[NEW_CODEGEN] BinaryOp::generate_code - operator: " << static_cast<int>(op) << std::endl;
    
    if (left) {
        left->generate_code(gen);
        // Save left operand on stack
        gen.emit_sub_reg_imm(4, 8);   // sub rsp, 8
        gen.emit_mov_mem_rsp_reg(0, 0);   // mov [rsp], rax
    }
    
    if (right) {
        right->generate_code(gen);
        // Right operand is now in RAX
    }
    
    // TODO: Implement actual binary operations based on op type
    // For now, just return the right operand
    result_type = DataType::ANY;
    std::cout << "[NEW_CODEGEN] BinaryOp: Placeholder implementation" << std::endl;
}

// Placeholder implementations for other nodes to prevent compilation errors
void RegexLiteral::generate_code(CodeGenerator& gen) {
    gen.emit_mov_reg_imm(0, 0);
    result_type = DataType::REGEX;
}

void TernaryOperator::generate_code(CodeGenerator& gen) {
    // TODO: Implement ternary operator
    gen.emit_mov_reg_imm(0, 0);
    result_type = DataType::ANY;
}

void FunctionCall::generate_code(CodeGenerator& gen) {
    // TODO: Implement function calls
    gen.emit_mov_reg_imm(0, 0);
    result_type = DataType::ANY;
}

void FunctionExpression::generate_code(CodeGenerator& gen) {
    // TODO: Implement function expressions
    gen.emit_mov_reg_imm(0, 0);
    result_type = DataType::FUNCTION;
}

void ArrowFunction::generate_code(CodeGenerator& gen) {
    // TODO: Implement arrow functions
    gen.emit_mov_reg_imm(0, 0);
    result_type = DataType::FUNCTION;
}

void MethodCall::generate_code(CodeGenerator& gen) {
    // TODO: Implement method calls
    gen.emit_mov_reg_imm(0, 0);
    result_type = DataType::ANY;
}

void ExpressionMethodCall::generate_code(CodeGenerator& gen) {
    // TODO: Implement expression method calls
    gen.emit_mov_reg_imm(0, 0);
    result_type = DataType::ANY;
}

void ArrayLiteral::generate_code(CodeGenerator& gen) {
    // TODO: Implement array literals
    gen.emit_mov_reg_imm(0, 0);
    result_type = DataType::ARRAY;
}

void ObjectLiteral::generate_code(CodeGenerator& gen) {
    // TODO: Implement object literals
    gen.emit_mov_reg_imm(0, 0);
    result_type = DataType::ANY;
}

void TypedArrayLiteral::generate_code(CodeGenerator& gen) {
    // TODO: Implement typed array literals
    gen.emit_mov_reg_imm(0, 0);
    result_type = DataType::ARRAY;
}

void ArrayAccess::generate_code(CodeGenerator& gen) {
    // TODO: Implement array access
    gen.emit_mov_reg_imm(0, 0);
    result_type = DataType::ANY;
}

void PostfixIncrement::generate_code(CodeGenerator& gen) {
    // TODO: Implement postfix increment
    gen.emit_mov_reg_imm(0, 0);
    result_type = DataType::ANY;
}

void PostfixDecrement::generate_code(CodeGenerator& gen) {
    // TODO: Implement postfix decrement
    gen.emit_mov_reg_imm(0, 0);
    result_type = DataType::ANY;
}

void FunctionDecl::generate_code(CodeGenerator& gen) {
    // TODO: Implement function declarations
    std::cout << "[NEW_CODEGEN] FunctionDecl for " << name << std::endl;
}

void IfStatement::generate_code(CodeGenerator& gen) {
    // TODO: Implement if statements
    std::cout << "[NEW_CODEGEN] IfStatement placeholder" << std::endl;
}

void ForLoop::generate_code(CodeGenerator& gen) {
    // TODO: Implement for loops
    std::cout << "[NEW_CODEGEN] ForLoop placeholder" << std::endl;
}

// Add more placeholder implementations as needed for other AST nodes...

void WhileLoop::generate_code(CodeGenerator& gen) {
    // TODO: Implement while loops
    std::cout << "[NEW_CODEGEN] WhileLoop placeholder" << std::endl;
}

void ReturnStatement::generate_code(CodeGenerator& gen) {
    // TODO: Implement return statements
    std::cout << "[NEW_CODEGEN] ReturnStatement placeholder" << std::endl;
}

void BreakStatement::generate_code(CodeGenerator& gen) {
    // TODO: Implement break statements
    std::cout << "[NEW_CODEGEN] BreakStatement placeholder" << std::endl;
}

void FreeStatement::generate_code(CodeGenerator& gen) {
    // TODO: Implement free statements
    std::cout << "[NEW_CODEGEN] FreeStatement placeholder" << std::endl;
}

void ThrowStatement::generate_code(CodeGenerator& gen) {
    // TODO: Implement throw statements
    std::cout << "[NEW_CODEGEN] ThrowStatement placeholder" << std::endl;
}

void CatchClause::generate_code(CodeGenerator& gen) {
    // TODO: Implement catch clauses
    std::cout << "[NEW_CODEGEN] CatchClause placeholder" << std::endl;
}

void TryStatement::generate_code(CodeGenerator& gen) {
    // TODO: Implement try statements
    std::cout << "[NEW_CODEGEN] TryStatement placeholder" << std::endl;
}

void BlockStatement::generate_code(CodeGenerator& gen) {
    // TODO: Implement block statements
    std::cout << "[NEW_CODEGEN] BlockStatement placeholder" << std::endl;
}

void CaseClause::generate_code(CodeGenerator& gen) {
    // TODO: Implement case clauses
    std::cout << "[NEW_CODEGEN] CaseClause placeholder" << std::endl;
}

void SwitchStatement::generate_code(CodeGenerator& gen) {
    // TODO: Implement switch statements
    std::cout << "[NEW_CODEGEN] SwitchStatement placeholder" << std::endl;
}

void ImportStatement::generate_code(CodeGenerator& gen) {
    // TODO: Implement import statements
    std::cout << "[NEW_CODEGEN] ImportStatement placeholder for " << module_path << std::endl;
}

void ExportStatement::generate_code(CodeGenerator& gen) {
    // TODO: Implement export statements
    std::cout << "[NEW_CODEGEN] ExportStatement placeholder" << std::endl;
}

void ConstructorDecl::generate_code(CodeGenerator& gen) {
    // TODO: Implement constructor declarations
    std::cout << "[NEW_CODEGEN] ConstructorDecl placeholder for " << class_name << std::endl;
}

void MethodDecl::generate_code(CodeGenerator& gen) {
    // TODO: Implement method declarations
    std::cout << "[NEW_CODEGEN] MethodDecl placeholder for " << name << std::endl;
}

void ClassDecl::generate_code(CodeGenerator& gen) {
    // TODO: Implement class declarations
    std::cout << "[NEW_CODEGEN] ClassDecl placeholder for " << name << std::endl;
}

void OperatorOverloadDecl::generate_code(CodeGenerator& gen) {
    // TODO: Implement operator overload declarations
    std::cout << "[NEW_CODEGEN] OperatorOverloadDecl placeholder for TokenType=" << static_cast<int>(operator_type) << std::endl;
}

void ForEachLoop::generate_code(CodeGenerator& gen) {
    // TODO: Implement foreach loops
    std::cout << "[NEW_CODEGEN] ForEachLoop placeholder" << std::endl;
}

void ForInStatement::generate_code(CodeGenerator& gen) {
    // TODO: Implement for-in statements
    std::cout << "[NEW_CODEGEN] ForInStatement placeholder" << std::endl;
}

// Additional missing AST node implementations

void PropertyAccess::generate_code(CodeGenerator& gen) {
    // TODO: Implement property access
    std::cout << "[NEW_CODEGEN] PropertyAccess placeholder for " << object_name << "." << property_name << std::endl;
}

void ExpressionPropertyAccess::generate_code(CodeGenerator& gen) {
    // TODO: Implement expression property access
    std::cout << "[NEW_CODEGEN] ExpressionPropertyAccess placeholder" << std::endl;
}

void PropertyAssignment::generate_code(CodeGenerator& gen) {
    // TODO: Implement property assignment
    std::cout << "[NEW_CODEGEN] PropertyAssignment placeholder" << std::endl;
}

void ExpressionPropertyAssignment::generate_code(CodeGenerator& gen) {
    // TODO: Implement expression property assignment
    std::cout << "[NEW_CODEGEN] ExpressionPropertyAssignment placeholder" << std::endl;
}

void ThisExpression::generate_code(CodeGenerator& gen) {
    // TODO: Implement this expression
    std::cout << "[NEW_CODEGEN] ThisExpression placeholder" << std::endl;
}

void NewExpression::generate_code(CodeGenerator& gen) {
    // TODO: Implement new expression
    std::cout << "[NEW_CODEGEN] NewExpression placeholder for " << class_name << std::endl;
}

void SuperCall::generate_code(CodeGenerator& gen) {
    // TODO: Implement super call
    std::cout << "[NEW_CODEGEN] SuperCall placeholder" << std::endl;
}

void SuperMethodCall::generate_code(CodeGenerator& gen) {
    // TODO: Implement super method call
    std::cout << "[NEW_CODEGEN] SuperMethodCall placeholder" << std::endl;
}

// Add missing FunctionExpression methods
void FunctionExpression::compile_function_body(CodeGenerator& gen, const std::string& func_name) {
    std::cout << "[NEW_CODEGEN] FunctionExpression::compile_function_body placeholder for " << func_name << std::endl;
    // TODO: Implement function body compilation with new scope system
}

// Add missing ConstructorDecl static variable
GoTSCompiler* ConstructorDecl::current_compiler_context = nullptr;

// Initialization message for the new system
static bool system_initialized = []() {
    std::cout << "[NEW_CODEGEN] New scope-aware AST code generation system initialized" << std::endl;
    return true;
}();
