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
    
    // Assignment context tracking (for complex expressions)
    DataType current_assignment_target_type = DataType::ANY;
    DataType current_assignment_array_element_type = DataType::ANY;
    DataType current_element_type_context = DataType::ANY;
    DataType current_property_assignment_type = DataType::ANY;
    
    // Current class context for 'this' handling
    std::string current_class_name;
    
    // Assignment context helpers (direct property access)
    void set_assignment_context(DataType target_type, DataType element_type = DataType::ANY) {
        current_assignment_target_type = target_type;
        current_assignment_array_element_type = element_type;
    }
    
    void clear_assignment_context() {
        current_assignment_target_type = DataType::ANY;
        current_assignment_array_element_type = DataType::ANY;
        current_element_type_context = DataType::ANY;
        current_property_assignment_type = DataType::ANY;
    }
    
    // Function scope management (for function entry/exit)
    void reset_for_function() {
        // Clear function-local state
        clear_assignment_context();
        current_class_name.clear();
    }
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
    // High-performance string creation using interned strings for literals
    // This provides both memory efficiency and extremely fast string creation
    
    if (value.empty()) {
        // Handle empty string efficiently - call __string_create_empty()
        gen.emit_call("__string_create_empty");
    } else {
        // SAFE APPROACH: Use string interning with proper fixed StringPool
        // The StringPool now uses std::string keys instead of char* keys,
        // which makes it safe to use with temporary string data
        
        // Store the string content safely for the call
        // We need to ensure the string data is available during the __string_intern call
        static std::unordered_map<std::string, const char*> literal_storage;
        
        // Check if we already have this literal stored
        auto it = literal_storage.find(value);
        const char* str_ptr;
        if (it != literal_storage.end()) {
            str_ptr = it->second;
        } else {
            // Allocate permanent storage for this literal
            char* permanent_str = new char[value.length() + 1];
            strcpy(permanent_str, value.c_str());
            literal_storage[value] = permanent_str;
            str_ptr = permanent_str;
        }
        
        uint64_t str_literal_addr = reinterpret_cast<uint64_t>(str_ptr);
        gen.emit_mov_reg_imm(7, static_cast<int64_t>(str_literal_addr)); // RDI = first argument
        
        // Use string interning for memory efficiency
        gen.emit_call("__string_intern");
    }
    
    // Result is now in RAX (pointer to GoTSString)
    result_type = DataType::STRING;
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
    
    // ULTRA-FAST DIRECT POINTER ACCESS - Zero lookups needed!
    if (variable_declaration_info) {
        // Get variable type directly from declaration info
        result_type = variable_declaration_info->data_type;
        
        // Use the direct offset from variable_declaration_info (FASTEST - no map lookup!)
        size_t var_offset = variable_declaration_info->offset;
        
        // Generate optimal load code based on scope relationship
        if (definition_scope == g_scope_context.current_scope) {
            // Loading from current scope (fastest path)
            gen.emit_mov_reg_reg_offset(0, 15, var_offset); // rax = [r15 + offset]
            std::cout << "[NEW_CODEGEN] Loaded from local variable '" << name << "' at r15+" << var_offset 
                      << " (type=" << static_cast<int>(variable_declaration_info->data_type) << ")" << std::endl;
        } else {
            // Variable is in parent scope - find which register holds it
            int scope_depth = variable_declaration_info->depth;
            auto reg_it = g_scope_context.scope_state.scope_depth_to_register.find(scope_depth);
            
            if (reg_it != g_scope_context.scope_state.scope_depth_to_register.end()) {
                // Parent scope is in a register (fast path)
                int scope_reg = reg_it->second;
                gen.emit_mov_reg_reg_offset(0, scope_reg, var_offset); // rax = [r12/13/14 + offset]
                std::cout << "[NEW_CODEGEN] Loaded parent variable '" << name 
                          << "' from r" << scope_reg << "+" << var_offset 
                          << " (type=" << static_cast<int>(result_type) << ")" << std::endl;
            } else {
                // Deep nesting - use stack-based access (slower but still optimized)
                throw std::runtime_error("Deep nested scope access not yet implemented for: " + name);
            }
        }
        
        std::cout << "[NEW_CODEGEN] Variable '" << name << "' loaded successfully via direct pointer (ULTRA-FAST)" << std::endl;
        return;
    }
    
    // This should never happen if scope analysis worked correctly
    throw std::runtime_error("Variable declaration info not found for: " + name + " (scope analysis bug)");
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
        
        // ULTRA-FAST DIRECT POINTER STORE
        if (variable_declaration_info) {
            // Update the variable's type in its declaration info
            variable_declaration_info->data_type = variable_type;
            
            // Use the direct offset from variable_declaration_info (FASTEST - no map lookup!)
            size_t var_offset = variable_declaration_info->offset;
            
            // Generate optimal store code based on scope relationship
            if (assignment_scope == g_scope_context.current_scope) {
                // Storing to current scope (fastest path)
                gen.emit_mov_reg_offset_reg(15, var_offset, 0); // [r15 + offset] = rax
                std::cout << "[NEW_CODEGEN] Stored to local variable '" << variable_name << "' at r15+" << var_offset 
                          << " (type=" << static_cast<int>(variable_type) << ")" << std::endl;
            } else {
                // Storing to parent scope
                int scope_depth = variable_declaration_info->depth;
                auto reg_it = g_scope_context.scope_state.scope_depth_to_register.find(scope_depth);
                
                if (reg_it != g_scope_context.scope_state.scope_depth_to_register.end()) {
                    // Parent scope is in register (fast path)
                    int scope_reg = reg_it->second;
                    gen.emit_mov_reg_offset_reg(scope_reg, var_offset, 0); // [r12/13/14 + offset] = rax
                    std::cout << "[NEW_CODEGEN] Stored to parent variable '" << variable_name 
                              << "' at r" << scope_reg << "+" << var_offset 
                              << " (type=" << static_cast<int>(variable_type) << ")" << std::endl;
                } else {
                    // Deep nesting - use stack-based access
                    throw std::runtime_error("Deep nested scope store not yet implemented for: " + variable_name);
                }
            }
        } else {
            // This should never happen if scope analysis worked correctly
            throw std::runtime_error("Variable declaration info not found for assignment: " + variable_name + " (scope analysis bug)");
        }
        
        result_type = variable_type;
        std::cout << "[NEW_CODEGEN] Assignment to '" << variable_name << "' completed (ULTRA-FAST)" << std::endl;
    }
}

// TODO: Implement more AST nodes using the same pattern
// For now, let's implement minimal versions that don't crash

void BinaryOp::generate_code(CodeGenerator& gen) {
    if (left) {
        left->generate_code(gen);
        // Push left operand result onto stack to protect it during right operand evaluation
        gen.emit_sub_reg_imm(4, 8);   // sub rsp, 8 (allocate stack space)
        // Store to RSP-relative location to match the RSP-relative load later
        if (auto x86_gen = dynamic_cast<X86CodeGenV2*>(&gen)) {
            x86_gen->emit_mov_mem_reg(0, 0);   // mov [rsp], rax (save left operand on stack)
        } else {
            gen.emit_mov_mem_reg(0, 0);   // fallback for other backends
        }
    }
    
    if (right) {
        right->generate_code(gen);
    }
    
    DataType left_type = left ? left->result_type : DataType::ANY;
    DataType right_type = right ? right->result_type : DataType::ANY;
    
    // Helper to get compatible result type (JavaScript-style type coercion)
    auto get_cast_type = [](DataType a, DataType b) -> DataType {
        if (a == DataType::STRING || b == DataType::STRING) return DataType::STRING;
        if (a == DataType::FLOAT64 || b == DataType::FLOAT64) return DataType::FLOAT64;
        if (a == DataType::INT64 || b == DataType::INT64) return DataType::INT64;
        return DataType::FLOAT64; // Default to float64 for JS compatibility
    };
    
    switch (op) {
        case TokenType::PLUS:
            if (left_type == DataType::STRING || right_type == DataType::STRING) {
                result_type = DataType::STRING;
                if (left) {
                    // String concatenation - extremely optimized
                    // Right operand (string) is in RAX
                    gen.emit_mov_reg_reg(6, 0);   // mov rsi, rax (right operand -> second argument)
                    
                    // Pop left operand from stack
                    auto* x86_gen = static_cast<X86CodeGenV2*>(&gen);
                    x86_gen->emit_mov_reg_mem(7, 0);   // mov rdi, [rsp] (left operand -> first argument)
                    gen.emit_add_reg_imm(4, 8);   // add rsp, 8 (restore stack)
                    
                    // Robust string concatenation with proper type handling
                    if (left_type == DataType::STRING && right_type == DataType::STRING) {
                        // Both are GoTSString* - use optimized __string_concat
                        gen.emit_call("__string_concat");
                    } else if (left_type == DataType::STRING && right_type != DataType::STRING) {
                        // Left is GoTSString*, right needs conversion to string
                        gen.emit_call("__string_concat_cstr");
                    } else if (left_type != DataType::STRING && right_type == DataType::STRING) {
                        // Left needs conversion to string, right is GoTSString*
                        gen.emit_call("__string_concat_cstr_left");
                    } else {
                        // Neither operand is a string - fallback to regular concatenation
                        gen.emit_call("__string_concat");
                    }
                    // Result (new GoTSString*) is now in RAX
                }
            } else {
                result_type = get_cast_type(left_type, right_type);
                if (left) {
                    // Pop left operand from stack and add to right operand (in RAX)
                    if (auto x86_gen = dynamic_cast<X86CodeGenV2*>(&gen)) {
                        x86_gen->emit_mov_reg_mem(3, 0);   // mov rbx, [rsp] (load left operand from stack)
                    } else {
                        gen.emit_mov_reg_mem(3, 0);   // fallback for other backends
                    }
                    gen.emit_add_reg_imm(4, 8);   // add rsp, 8 (restore stack)
                    gen.emit_add_reg_reg(0, 3);   // add rax, rbx (add left to right)
                }
            }
            break;
            
        case TokenType::MINUS:
            result_type = get_cast_type(left_type, right_type);
            if (left) {
                // Binary minus: Pop left operand from stack and subtract right operand from it
                if (auto x86_gen = dynamic_cast<X86CodeGenV2*>(&gen)) {
                    x86_gen->emit_mov_reg_mem(3, 0);   // mov rbx, [rsp] (load left operand from stack)
                } else {
                    gen.emit_mov_reg_mem(3, 0);   // fallback for other backends
                }
                gen.emit_add_reg_imm(4, 8);   // add rsp, 8 (restore stack)
                gen.emit_sub_reg_reg(3, 0);   // sub rbx, rax (subtract right from left)
                gen.emit_mov_reg_reg(0, 3);   // mov rax, rbx (result in rax)
            } else {
                // Unary minus: negate the value in RAX
                gen.emit_mov_reg_imm(1, 0);   // mov rcx, 0
                gen.emit_sub_reg_reg(1, 0);   // sub rcx, rax (0 - rax)
                gen.emit_mov_reg_reg(0, 1);   // mov rax, rcx (result in rax)
                result_type = right_type;     // Result type is same as right operand for unary minus
            }
            break;
            
        case TokenType::MULTIPLY:
            result_type = get_cast_type(left_type, right_type);
            if (left) {
                // Pop left operand from stack and multiply with right operand
                if (auto x86_gen = dynamic_cast<X86CodeGenV2*>(&gen)) {
                    x86_gen->emit_mov_reg_mem(3, 0);   // mov rbx, [rsp] (load left operand from stack)
                } else {
                    gen.emit_mov_reg_mem(3, 0);   // fallback for other backends
                }
                gen.emit_add_reg_imm(4, 8);   // add rsp, 8 (restore stack)
                gen.emit_mul_reg_reg(3, 0);   // imul rbx, rax (multiply left with right)
                gen.emit_mov_reg_reg(0, 3);   // mov rax, rbx (result in rax)
            }
            break;
            
        case TokenType::POWER:
            result_type = DataType::FLOAT64; // Power operation returns float64
            if (left) {
                // For exponentiation: base ** exponent
                // Right operand (exponent) is currently in RAX
                gen.emit_mov_reg_reg(6, 0);   // mov rsi, rax (exponent -> second argument)
                
                // Pop left operand from stack (base)
                if (auto x86_gen = dynamic_cast<X86CodeGenV2*>(&gen)) {
                    x86_gen->emit_mov_reg_mem(7, 0);   // mov rdi, [rsp] (base -> first argument)
                } else {
                    gen.emit_mov_reg_mem(7, 0);   // fallback for other backends
                }
                gen.emit_add_reg_imm(4, 8);   // add rsp, 8 (restore stack)
                
                // Call the power function: __runtime_pow(base, exponent)
                gen.emit_call("__runtime_pow");
                // Result will be in RAX
            }
            break;
            
        case TokenType::DIVIDE:
            result_type = get_cast_type(left_type, right_type);
            if (left) {
                // Pop left operand from stack and divide by right operand
                if (auto x86_gen = dynamic_cast<X86CodeGenV2*>(&gen)) {
                    x86_gen->emit_mov_reg_mem(1, 0);   // mov rcx, [rsp] (load left operand from stack)
                } else {
                    gen.emit_mov_reg_mem(1, 0);   // fallback for other backends
                }
                gen.emit_add_reg_imm(4, 8);   // add rsp, 8 (restore stack)
                gen.emit_div_reg_reg(1, 0);   // div rcx by rax (divide left by right)
                gen.emit_mov_reg_reg(0, 1);   // mov rax, rcx (result in rax)
            }
            break;
            
        case TokenType::MODULO:
            result_type = get_cast_type(left_type, right_type);
            if (left) {
                // Use runtime function for modulo to ensure robustness
                // Right operand is in RAX, move to RSI (second argument)
                gen.emit_mov_reg_reg(6, 0);   // RSI = right operand (from RAX)
                
                // Pop left operand from stack directly to RDI (first argument)
                if (auto x86_gen = dynamic_cast<X86CodeGenV2*>(&gen)) {
                    x86_gen->emit_mov_reg_mem(7, 0);   // RDI = left operand from [rsp]
                } else {
                    gen.emit_mov_reg_mem(7, 0);   // fallback for other backends
                }
                gen.emit_add_reg_imm(4, 8);   // add rsp, 8 (restore stack)
                
                // Call __runtime_modulo(left, right)
                gen.emit_call("__runtime_modulo");
                // Result is now in RAX
            }
            break;
            
        case TokenType::EQUAL:
        case TokenType::NOT_EQUAL:
        case TokenType::STRICT_EQUAL:
        case TokenType::LESS:
        case TokenType::GREATER:
        case TokenType::LESS_EQUAL:
        case TokenType::GREATER_EQUAL:
            result_type = DataType::BOOLEAN;
            if (left) {
                // Pop left operand from stack and compare with right operand (in RAX)
                if (auto x86_gen = dynamic_cast<X86CodeGenV2*>(&gen)) {
                    x86_gen->emit_mov_reg_mem(1, 0);   // mov rcx, [rsp] (load left operand from stack)
                } else {
                    gen.emit_mov_reg_mem(1, 0);   // fallback for other backends
                }
                gen.emit_add_reg_imm(4, 8);   // add rsp, 8 (restore stack)
                
                // Optimized comparison logic with string-specific handling
                if (left_type == DataType::STRING && right_type == DataType::STRING) {
                    // Both operands are strings - use high-performance string comparison
                    // Left value is in RCX, right value is in RAX
                    gen.emit_mov_reg_reg(7, 1);   // mov rdi, rcx (left string -> first argument)
                    gen.emit_mov_reg_reg(6, 0);   // mov rsi, rax (right string -> second argument)
                    
                    switch (op) {
                        case TokenType::EQUAL:
                        case TokenType::STRICT_EQUAL:
                            gen.emit_call("__string_equals");
                            // Result (bool) is already in RAX
                            break;
                        case TokenType::NOT_EQUAL:
                            gen.emit_call("__string_equals");
                            // Invert the result: XOR with 1
                            gen.emit_mov_reg_imm(1, 1);
                            gen.emit_xor_reg_reg(0, 1);
                            break;
                        case TokenType::LESS:
                            gen.emit_call("__string_compare");
                            // Result < 0 means left < right
                            gen.emit_mov_reg_imm(1, 0);
                            gen.emit_compare(0, 1);   // compare result with 0
                            gen.emit_setl(0);         // set AL if less than
                            // Convert AL (8-bit) to RAX (64-bit) manually
                            gen.emit_and_reg_imm(0, 0xFF); // Clear upper bits, keep AL
                            break;
                        case TokenType::GREATER:
                            gen.emit_call("__string_compare");
                            // Result > 0 means left > right
                            gen.emit_mov_reg_imm(1, 0);
                            gen.emit_compare(0, 1);   // compare result with 0
                            gen.emit_setg(0);      // set AL if greater than
                            gen.emit_and_reg_imm(0, 0xFF); // Clear upper bits, keep AL
                            break;
                        case TokenType::LESS_EQUAL:
                            gen.emit_call("__string_compare");
                            // Result <= 0 means left <= right
                            gen.emit_mov_reg_imm(1, 0);
                            gen.emit_compare(0, 1);   // compare result with 0
                            gen.emit_setle(0);   // set AL if less or equal
                            gen.emit_and_reg_imm(0, 0xFF); // Clear upper bits, keep AL
                            break;
                        case TokenType::GREATER_EQUAL:
                            gen.emit_call("__string_compare");
                            // Result >= 0 means left >= right
                            gen.emit_mov_reg_imm(1, 0);
                            gen.emit_compare(0, 1);   // compare result with 0
                            gen.emit_setge(0); // set AL if greater or equal
                            gen.emit_and_reg_imm(0, 0xFF); // Clear upper bits, keep AL
                            break;
                    }
                } else {
                    // Numeric comparison - left in RCX, right in RAX
                    gen.emit_compare(1, 0);   // cmp rcx, rax
                    
                    switch (op) {
                        case TokenType::EQUAL:
                        case TokenType::STRICT_EQUAL:
                            gen.emit_sete(0);        // sete al
                            break;
                        case TokenType::NOT_EQUAL:
                            gen.emit_setne(0);    // setne al
                            break;
                        case TokenType::LESS:
                            gen.emit_setl(0);         // setl al
                            break;
                        case TokenType::GREATER:
                            gen.emit_setg(0);      // setg al
                            break;
                        case TokenType::LESS_EQUAL:
                            gen.emit_setle(0);   // setle al
                            break;
                        case TokenType::GREATER_EQUAL:
                            gen.emit_setge(0); // setge al
                            break;
                    }
                    
                    // Convert AL (8-bit) to RAX (64-bit) manually
                    gen.emit_and_reg_imm(0, 0xFF); // Clear upper bits, keep AL
                }
            }
            break;
            
        case TokenType::AND:
            result_type = DataType::BOOLEAN;
            if (left) {
                // Short-circuit evaluation: if left is falsy, don't evaluate right
                // Left operand result is on stack, right operand result is in RAX
                if (auto x86_gen = dynamic_cast<X86CodeGenV2*>(&gen)) {
                    x86_gen->emit_mov_reg_mem(1, 0);   // mov rcx, [rsp] (load left operand)
                } else {
                    gen.emit_mov_reg_mem(1, 0);   // fallback
                }
                gen.emit_add_reg_imm(4, 8);   // add rsp, 8 (restore stack)
                
                // Test left operand for truthiness (simplified - just check if zero)
                gen.emit_compare(1, 1);          // Compare RCX with itself to set flags
                // For now, use simple logic: if left is zero, result is 0, else result is right operand
                // This is a simplified version - in full JS, we'd need more complex truthiness checks
                gen.emit_mov_reg_imm(2, 0);           // mov rdx, 0 (false result)
                // TODO: Use conditional move when available in CodeGenerator
                // For now, use simpler approach
                std::string false_label = "__and_false_" + std::to_string(rand());
                std::string end_label = "__and_end_" + std::to_string(rand());
                gen.emit_jump_if_zero(false_label);
                // Left was truthy, use right operand (already in RAX)
                gen.emit_jump(end_label);
                gen.emit_label(false_label);
                gen.emit_mov_reg_imm(0, 0); // Result is 0 (false)
                gen.emit_label(end_label);
            }
            break;
            
        case TokenType::OR:
            result_type = DataType::BOOLEAN;
            if (left) {
                // Short-circuit evaluation: if left is truthy, don't use right
                // Left operand result is on stack, right operand result is in RAX
                if (auto x86_gen = dynamic_cast<X86CodeGenV2*>(&gen)) {
                    x86_gen->emit_mov_reg_mem(1, 0);   // mov rcx, [rsp] (load left operand)
                } else {
                    gen.emit_mov_reg_mem(1, 0);   // fallback
                }
                gen.emit_add_reg_imm(4, 8);   // add rsp, 8 (restore stack)
                
                // Test left operand for truthiness
                gen.emit_compare(1, 1);          // Compare RCX with itself to set flags
                // If left is truthy, use left; if falsy, use right operand (in RAX)
                std::string true_label = "__or_true_" + std::to_string(rand());
                std::string end_label = "__or_end_" + std::to_string(rand());
                gen.emit_jump_if_not_zero(true_label);
                // Left was falsy, use right operand (already in RAX)
                gen.emit_jump(end_label);
                gen.emit_label(true_label);
                gen.emit_mov_reg_reg(0, 1); // Use left operand
                gen.emit_label(end_label);
            }
            break;
            
        default:
            result_type = DataType::ANY;
            break;
    }
}

// Placeholder implementations for other nodes to prevent compilation errors
void RegexLiteral::generate_code(CodeGenerator& gen) {
    // Create a runtime regex object from pattern and flags
    
    // Store pattern string
    static std::unordered_map<std::string, const char*> pattern_storage;
    
    // Check if we already have this pattern stored
    auto pattern_it = pattern_storage.find(pattern);
    const char* pattern_ptr;
    if (pattern_it != pattern_storage.end()) {
        pattern_ptr = pattern_it->second;
    } else {
        // Allocate permanent storage for this pattern
        char* permanent_pattern = new char[pattern.length() + 1];
        strcpy(permanent_pattern, pattern.c_str());
        pattern_storage[pattern] = permanent_pattern;
        pattern_ptr = permanent_pattern;
    }
    
    // Use a safer method to pass the pattern
    // Store pattern in a safe global registry with integer IDs
    static std::unordered_map<std::string, int> pattern_registry;
    static int next_pattern_id = 1;
    
    int pattern_id;
    auto registry_it = pattern_registry.find(pattern);
    if (registry_it != pattern_registry.end()) {
        pattern_id = registry_it->second;
    } else {
        pattern_id = next_pattern_id++;
        pattern_registry[pattern] = pattern_id;
    }
    
    // FIXED: First create a GoTSString for the pattern using string interning
    uint64_t pattern_str_addr = reinterpret_cast<uint64_t>(pattern_ptr);
    gen.emit_mov_reg_imm(7, static_cast<int64_t>(pattern_str_addr)); // RDI = first argument
    gen.emit_call("__string_intern"); // Convert raw C string to GoTSString*
    
    // Now register the pattern with the runtime (RAX contains GoTSString*)
    gen.emit_mov_reg_reg(7, 0); // RDI = RAX (GoTSString* from string interning)
    gen.emit_call("__register_regex_pattern");
    
    // The function returns the pattern ID in RAX, use it to create the regex
    gen.emit_mov_reg_reg(7, 0); // RDI = RAX (pattern ID returned)
    gen.emit_call("__regex_create_by_id");
    
    // Result is now in RAX (pointer to GoTSRegExp)
    result_type = DataType::REGEX;
}

void TernaryOperator::generate_code(CodeGenerator& gen) {
    // Generate unique labels for the ternary branches
    static int label_counter = 0;
    std::string false_label = "__ternary_false_" + std::to_string(label_counter);
    std::string end_label = "__ternary_end_" + std::to_string(label_counter++);
    
    // Generate code for condition
    condition->generate_code(gen);
    
    // Test if condition is zero (false) - compare RAX with 0
    gen.emit_mov_reg_imm(1, 0); // mov rcx, 0
    gen.emit_compare(0, 1); // Compare RAX with RCX (0)
    gen.emit_jump_if_zero(false_label);
    
    // Generate code for true expression
    true_expr->generate_code(gen);
    gen.emit_jump(end_label);
    
    // False branch
    gen.emit_label(false_label);
    false_expr->generate_code(gen);
    
    // End label
    gen.emit_label(end_label);
    
    // Result type is the common type of true and false expressions
    // For now, use FLOAT64 as common numeric type
    if (true_expr->result_type == false_expr->result_type) {
        result_type = true_expr->result_type;
    } else {
        result_type = DataType::FLOAT64; // Default to JavaScript number type
    }
}

void FunctionCall::generate_code(CodeGenerator& gen) {
    if (is_goroutine) {
        // For goroutines, we need to build an argument array on the stack
        if (arguments.size() > 0) {
            // Push arguments onto stack in reverse order to create array
            for (int i = arguments.size() - 1; i >= 0; i--) {
                arguments[i]->generate_code(gen);
                gen.emit_sub_reg_imm(4, 8);  // sub rsp, 8
                if (auto x86_gen = dynamic_cast<X86CodeGenV2*>(&gen)) {
                    x86_gen->emit_mov_mem_reg(0, 0);  // mov [rsp], rax
                } else {
                    gen.emit_mov_mem_reg(0, 0);  // fallback for other backends
                }
            }
            
            // Now stack contains arguments in correct order: arg0, arg1, arg2...
            gen.emit_goroutine_spawn_with_args(name, arguments.size());
            
            // Clean up the argument array from stack
            int64_t array_size = arguments.size() * 8;
            gen.emit_add_reg_imm(4, array_size);  // add rsp, array_size
        } else {
            gen.emit_goroutine_spawn(name);
        }
        result_type = DataType::PROMISE;
    } else {
        // Check for global timer functions and map them to runtime equivalents
        if (name == "setTimeout") {
            // Map setTimeout to runtime.timer.setTimeout
            for (size_t i = 0; i < arguments.size() && i < 6; i++) {
                arguments[i]->generate_code(gen);
                switch (i) {
                    case 0: gen.emit_mov_reg_reg(7, 0); break;  // RDI = RAX
                    case 1: gen.emit_mov_reg_reg(6, 0); break;  // RSI = RAX
                    case 2: gen.emit_mov_reg_reg(2, 0); break;  // RDX = RAX
                    case 3: gen.emit_mov_reg_reg(1, 0); break;  // RCX = RAX
                    case 4: gen.emit_mov_reg_reg(8, 0); break;  // R8 = RAX
                    case 5: gen.emit_mov_reg_reg(9, 0); break;  // R9 = RAX
                }
            }
            gen.emit_call("__gots_set_timeout");
            result_type = DataType::INT64; // Timer ID
            return;
        } else if (name == "setInterval") {
            for (size_t i = 0; i < arguments.size() && i < 6; i++) {
                arguments[i]->generate_code(gen);
                switch (i) {
                    case 0: gen.emit_mov_reg_reg(7, 0); break;  // RDI = RAX
                    case 1: gen.emit_mov_reg_reg(6, 0); break;  // RSI = RAX
                    case 2: gen.emit_mov_reg_reg(2, 0); break;  // RDX = RAX
                    case 3: gen.emit_mov_reg_reg(1, 0); break;  // RCX = RAX
                    case 4: gen.emit_mov_reg_reg(8, 0); break;  // R8 = RAX
                    case 5: gen.emit_mov_reg_reg(9, 0); break;  // R9 = RAX
                }
            }
            gen.emit_call("__gots_set_interval");
            result_type = DataType::INT64; // Timer ID
            return;
        } else if (name == "clearTimeout") {
            for (size_t i = 0; i < arguments.size() && i < 6; i++) {
                arguments[i]->generate_code(gen);
                switch (i) {
                    case 0: gen.emit_mov_reg_reg(7, 0); break;  // RDI = RAX
                    case 1: gen.emit_mov_reg_reg(6, 0); break;  // RSI = RAX
                    case 2: gen.emit_mov_reg_reg(2, 0); break;  // RDX = RAX
                    case 3: gen.emit_mov_reg_reg(1, 0); break;  // RCX = RAX
                    case 4: gen.emit_mov_reg_reg(8, 0); break;  // R8 = RAX
                    case 5: gen.emit_mov_reg_reg(9, 0); break;  // R9 = RAX
                }
            }
            gen.emit_call("__gots_clear_timeout");
            result_type = DataType::BOOLEAN; // Success/failure
            return;
        } else if (name == "clearInterval") {
            for (size_t i = 0; i < arguments.size() && i < 6; i++) {
                arguments[i]->generate_code(gen);
                switch (i) {
                    case 0: gen.emit_mov_reg_reg(7, 0); break;  // RDI = RAX
                    case 1: gen.emit_mov_reg_reg(6, 0); break;  // RSI = RAX
                    case 2: gen.emit_mov_reg_reg(2, 0); break;  // RDX = RAX
                    case 3: gen.emit_mov_reg_reg(1, 0); break;  // RCX = RAX
                    case 4: gen.emit_mov_reg_reg(8, 0); break;  // R8 = RAX
                    case 5: gen.emit_mov_reg_reg(9, 0); break;  // R9 = RAX
                }
            }
            gen.emit_call("__gots_clear_interval");
            result_type = DataType::BOOLEAN; // Success/failure
            return;
        } else if (name == "console.log") {
            // Simple console.log implementation - for now just call runtime function
            // TODO: Implement full type-aware console.log with new scope system
            std::vector<ExpressionNode*> arg_ptrs;
            for (const auto& arg : arguments) {
                arg_ptrs.push_back(arg.get());
            }
            
            // For now, just emit a simple console log call
            if (arguments.size() > 0) {
                arguments[0]->generate_code(gen);
                gen.emit_mov_reg_reg(7, 0); // RDI = first argument
                gen.emit_call("__console_log_simple");
            }
            result_type = DataType::VOID;
            return;
        }
        
        // Regular function call - use x86-64 calling convention
        // TODO: Implement function variable resolution with new scope system
        
        // Generate code for arguments and place them in appropriate registers
        for (size_t i = 0; i < arguments.size() && i < 6; i++) {
            arguments[i]->generate_code(gen);
            
            // Move result to appropriate argument register
            switch (i) {
                case 0: gen.emit_mov_reg_reg(7, 0); break;  // RDI = RAX
                case 1: gen.emit_mov_reg_reg(6, 0); break;  // RSI = RAX
                case 2: gen.emit_mov_reg_reg(2, 0); break;  // RDX = RAX
                case 3: gen.emit_mov_reg_reg(1, 0); break;  // RCX = RAX
                case 4: gen.emit_mov_reg_reg(8, 0); break;  // R8 = RAX
                case 5: gen.emit_mov_reg_reg(9, 0); break;  // R9 = RAX
            }
        }
        
        // For more than 6 arguments, push them onto stack (in reverse order)
        for (int i = arguments.size() - 1; i >= 6; i--) {
            arguments[i]->generate_code(gen);
            // Push RAX onto stack
            gen.emit_sub_reg_imm(4, 8);  // sub rsp, 8
            gen.emit_mov_mem_reg(0, 0);  // mov [rsp], rax
        }
        
        // Direct function call by name
        gen.emit_call(name);
        
        // TODO: Look up function return type from compiler registry with new system
        result_type = DataType::FLOAT64; // Default for built-in functions
        
        // Clean up stack if we pushed arguments
        if (arguments.size() > 6) {
            int stack_cleanup = (arguments.size() - 6) * 8;
            gen.emit_add_reg_imm(4, stack_cleanup);  // add rsp, cleanup_amount
        }
    }
    
    if (is_awaited) {
        gen.emit_promise_await(0);
    }
}

void FunctionExpression::generate_code(CodeGenerator& gen) {
    std::cout << "[NEW_CODEGEN] FunctionExpression::generate_code - function: " << compilation_assigned_name_ << std::endl;
    
    // Check if the function should already be compiled in the function compilation manager
    std::string func_name = compilation_assigned_name_;
    if (func_name.empty()) {
        // Generate a unique name for anonymous function expressions
        static int func_expr_counter = 0;
        func_name = "__function_expr_" + std::to_string(func_expr_counter++);
        compilation_assigned_name_ = func_name;
    }
    
    // For now, use a simple approach - store function address or ID
    // TODO: Integrate with FunctionCompilationManager when available
    
    if (is_goroutine) {
        // Goroutine spawn with function name
        gen.emit_mov_reg_imm(7, reinterpret_cast<int64_t>(func_name.c_str())); // RDI = function name
        gen.emit_call("__goroutine_spawn_by_name");
        result_type = DataType::PROMISE;
    } else {
        // Return function reference (for now, use function name hash as ID)
        std::hash<std::string> hasher;
        size_t func_hash = hasher(func_name);
        gen.emit_mov_reg_imm(0, static_cast<int64_t>(func_hash)); // RAX = function ID/hash
        result_type = DataType::FUNCTION;
    }
    
    std::cout << "[NEW_CODEGEN] FunctionExpression: generated reference for " << func_name << std::endl;
}

void ArrowFunction::generate_code(CodeGenerator& gen) {
    std::cout << "[NEW_CODEGEN] ArrowFunction::generate_code - arrow function" << std::endl;
    
    // Generate a unique name for arrow functions
    static int arrow_func_counter = 0;
    std::string func_name = "__arrow_func_" + std::to_string(arrow_func_counter++);
    
    // Arrow functions are similar to function expressions but with lexical 'this' binding
    // For now, treat them similarly to function expressions
    
    if (is_goroutine) {
        // Goroutine spawn with arrow function name
        gen.emit_mov_reg_imm(7, reinterpret_cast<int64_t>(func_name.c_str())); // RDI = function name
        gen.emit_call("__goroutine_spawn_by_name");
        result_type = DataType::PROMISE;
    } else {
        // Return arrow function reference
        std::hash<std::string> hasher;
        size_t func_hash = hasher(func_name);
        gen.emit_mov_reg_imm(0, static_cast<int64_t>(func_hash)); // RAX = function ID/hash
        result_type = DataType::FUNCTION;
    }
    
    std::cout << "[NEW_CODEGEN] ArrowFunction: generated reference for " << func_name << std::endl;
}

void MethodCall::generate_code(CodeGenerator& gen) {
    
    // Handle built-in methods
    if (object_name == "console") {
        if (method_name == "log") {
            // Simple console.log implementation for now
            // TODO: Implement full type-aware console.log with new scope system
            
            for (size_t i = 0; i < arguments.size() && i < 6; i++) {
                arguments[i]->generate_code(gen);
                
                // Move result to appropriate argument register
                switch (i) {
                    case 0: gen.emit_mov_reg_reg(7, 0); break;  // RDI = RAX
                    case 1: gen.emit_mov_reg_reg(6, 0); break;  // RSI = RAX
                    case 2: gen.emit_mov_reg_reg(2, 0); break;  // RDX = RAX
                    case 3: gen.emit_mov_reg_reg(1, 0); break;  // RCX = RAX
                    case 4: gen.emit_mov_reg_reg(8, 0); break;  // R8 = RAX
                    case 5: gen.emit_mov_reg_reg(9, 0); break;  // R9 = RAX
                }
            }
            
            // Call appropriate runtime function based on argument count
            if (arguments.size() == 0) {
                gen.emit_call("__console_log_empty");
            } else if (arguments.size() == 1) {
                gen.emit_call("__console_log_single");
            } else {
                gen.emit_call("__console_log_multiple");
            }
            
            result_type = DataType::VOID;
        } else if (method_name == "time") {
            // Call console.time built-in function
            if (arguments.size() > 0) {
                arguments[0]->generate_code(gen);
                gen.emit_mov_reg_reg(7, 0); // RDI = RAX
            }
            gen.emit_sub_reg_imm(4, 8);  // Align stack to 16-byte boundary
            gen.emit_call("__console_time");
            gen.emit_add_reg_imm(4, 8);  // Restore stack
            result_type = DataType::VOID;
        } else if (method_name == "timeEnd") {
            // Call console.timeEnd built-in function
            if (arguments.size() > 0) {
                arguments[0]->generate_code(gen);
                gen.emit_mov_reg_reg(7, 0); // RDI = RAX
            }
            gen.emit_sub_reg_imm(4, 8);  // Align stack to 16-byte boundary
            gen.emit_call("__console_timeEnd");
            gen.emit_add_reg_imm(4, 8);  // Restore stack
            result_type = DataType::VOID;
        } else {
            throw std::runtime_error("Unknown console method: " + method_name);
        }
    } else if (object_name == "Promise") {
        if (method_name == "all") {
            // Promise.all expects an array as its first argument
            if (arguments.size() > 0) {
                arguments[0]->generate_code(gen);
                gen.emit_mov_reg_reg(7, 0); // RDI = RAX
            } else {
                gen.emit_mov_reg_imm(7, 0); // RDI = 0 (nullptr)
            }
            gen.emit_call("__promise_all");
            result_type = DataType::PROMISE;
        } else {
            throw std::runtime_error("Unknown Promise method: " + method_name);
        }
    } else {
        // Handle variable method calls (like array.push()) using scope system
        // TODO: Replace TypeInference with scope-based variable type lookup
        
        // For now, try to load the variable and determine its type at runtime
        // This is a simplified implementation until we have full scope analysis
        
        // Load the variable using scope-aware access
        emit_variable_load(gen, object_name);
        
        // For now, assume it's an array and handle common array methods
        if (method_name == "push") {
            // RDX will contain the array pointer after emit_variable_load
            gen.emit_mov_reg_reg(2, 0); // RDX = array pointer (from RAX)
            
            // Call array push for each argument
            for (size_t i = 0; i < arguments.size(); i++) {
                // Save array pointer to a safe stack location
                gen.emit_mov_mem_reg(-32, 2); // Save array pointer to stack
                
                // Generate code for the argument
                arguments[i]->generate_code(gen);
                
                // Restore array pointer and set up call parameters
                gen.emit_mov_reg_mem(7, -32); // RDI = array pointer from stack
                gen.emit_mov_reg_reg(6, 0); // RSI = value to push
                gen.emit_call("__array_push");
            }
            result_type = DataType::VOID;
        } else if (method_name == "pop") {
            // Array is already loaded in RAX
            gen.emit_mov_reg_reg(7, 0); // RDI = array pointer
            gen.emit_call("__simple_array_pop");
            result_type = DataType::FLOAT64;
        } else if (method_name == "slice") {
            // Array is already loaded in RAX
            gen.emit_mov_reg_reg(7, 0); // RDI = array pointer
            
            // Handle arguments: start, end, step (with defaults)
            if (arguments.size() >= 1) {
                arguments[0]->generate_code(gen);
                gen.emit_mov_reg_reg(6, 0); // RSI = start
            } else {
                gen.emit_mov_reg_imm(6, 0); // RSI = 0 (default start)
            }
            
            if (arguments.size() >= 2) {
                arguments[1]->generate_code(gen);
                gen.emit_mov_reg_reg(2, 0); // RDX = end
            } else {
                gen.emit_mov_reg_imm(2, -1); // RDX = -1 (default end)
            }
            
            if (arguments.size() >= 3) {
                arguments[2]->generate_code(gen);
                gen.emit_mov_reg_reg(1, 0); // RCX = step
            } else {
                gen.emit_mov_reg_imm(1, 1); // RCX = 1 (default step)
            }
            
            gen.emit_call("__simple_array_slice");
            result_type = DataType::ARRAY;
        } else if (method_name == "toString") {
            // Array is already loaded in RAX
            gen.emit_mov_reg_reg(7, 0); // RDI = array pointer
            gen.emit_call("__simple_array_tostring");
            result_type = DataType::STRING;
        } else if (method_name == "sum") {
            gen.emit_mov_reg_reg(7, 0); // RDI = array pointer
            gen.emit_call("__simple_array_sum");
            result_type = DataType::FLOAT64;
        } else if (method_name == "mean") {
            gen.emit_mov_reg_reg(7, 0); // RDI = array pointer
            gen.emit_call("__simple_array_mean");
            result_type = DataType::FLOAT64;
        } else if (method_name == "max") {
            gen.emit_mov_reg_reg(7, 0); // RDI = array pointer
            gen.emit_call("__simple_array_max");
            result_type = DataType::FLOAT64;
        } else if (method_name == "min") {
            gen.emit_mov_reg_reg(7, 0); // RDI = array pointer
            gen.emit_call("__simple_array_min");
            result_type = DataType::FLOAT64;
        } else if (method_name == "test") {
            // Assume this is a regex test method
            gen.emit_mov_reg_reg(12, 0); // R12 = regex pointer (save in callee-saved register)
            
            if (arguments.size() > 0) {
                arguments[0]->generate_code(gen);
                gen.emit_mov_reg_reg(6, 0); // RSI = string pointer
                gen.emit_mov_reg_reg(7, 12); // RDI = regex pointer (restore from R12)
                
                gen.emit_call("__regex_test");
                result_type = DataType::BOOLEAN;
            } else {
                throw std::runtime_error("RegExp.test() requires a string argument");
            }
        } else if (method_name == "exec") {
            // Assume this is a regex exec method
            gen.emit_mov_reg_reg(12, 0); // R12 = regex pointer (save in callee-saved register)
            
            if (arguments.size() > 0) {
                arguments[0]->generate_code(gen);
                gen.emit_mov_reg_reg(6, 0); // RSI = string pointer
                gen.emit_mov_reg_reg(7, 12); // RDI = regex pointer (restore from R12)
                
                gen.emit_call("__regex_exec");
                result_type = DataType::ARRAY; // Returns match array
            } else {
                throw std::runtime_error("RegExp.exec() requires a string argument");
            }
        } else {
            throw std::runtime_error("Unknown method: " + object_name + "." + method_name);
        }
    }
}

void ExpressionMethodCall::generate_code(CodeGenerator& gen) {
    std::cout << "[NEW_CODEGEN] ExpressionMethodCall::generate_code - method: " << method_name << std::endl;
    
    // Check for runtime.x.y() patterns (optimized direct calls)
    // TODO: Add proper AST analysis to detect runtime property access patterns
    
    // For now, generate the object expression and call the method on it
    object->generate_code(gen);
    
    // Save object reference to stack for method call
    gen.emit_mov_mem_reg(-80, 0); // Save object pointer
    
    // Generate arguments and set up x86-64 calling convention
    for (size_t i = 0; i < arguments.size() && i < 6; i++) {
        arguments[i]->generate_code(gen);
        
        // Move argument to appropriate register
        switch (i) {
            case 0: gen.emit_mov_reg_reg(7, 0); break;  // RDI = RAX (1st arg)
            case 1: gen.emit_mov_reg_reg(6, 0); break;  // RSI = RAX (2nd arg)
            case 2: gen.emit_mov_reg_reg(2, 0); break;  // RDX = RAX (3rd arg)
            case 3: gen.emit_mov_reg_reg(1, 0); break;  // RCX = RAX (4th arg)
            case 4: gen.emit_mov_reg_reg(8, 0); break;  // R8 = RAX (5th arg)
            case 5: gen.emit_mov_reg_reg(9, 0); break;  // R9 = RAX (6th arg)
        }
    }
    
    // For more than 6 arguments, push onto stack in reverse order
    for (int i = arguments.size() - 1; i >= 6; i--) {
        arguments[i]->generate_code(gen);
        gen.emit_sub_reg_imm(4, 8);  // sub rsp, 8
        gen.emit_mov_mem_reg(0, 0);  // mov [rsp], rax
    }
    
    // Restore object pointer and call the method
    // For now, construct method name using simple pattern
    std::string method_call_name = "__method_call_" + method_name;
    
    // Move object pointer to first argument position (before other arguments)
    gen.emit_mov_reg_mem(7, -80); // RDI = object pointer
    
    // Call the dynamic method dispatcher
    gen.emit_call(method_call_name);
    
    // Clean up stack arguments if any
    if (arguments.size() > 6) {
        int stack_cleanup = (arguments.size() - 6) * 8;
        gen.emit_add_reg_imm(4, stack_cleanup);
    }
    
    result_type = DataType::ANY; // Dynamic method calls can return any type
    
    std::cout << "[NEW_CODEGEN] ExpressionMethodCall: called " << method_call_name << std::endl;
}

void ArrayLiteral::generate_code(CodeGenerator& gen) {
    std::cout << "[NEW_CODEGEN] ArrayLiteral::generate_code - Creating array with " << elements.size() << " elements" << std::endl;
    
    if (elements.size() == 0) {
        // Empty array case
        gen.emit_mov_reg_imm(7, 0);  // RDI = 0 (empty array)
        gen.emit_call("__simple_array_zeros");  // Creates empty array
    } else {
        // Non-empty array - create empty array first
        gen.emit_mov_reg_imm(7, 0);  // RDI = 0 (empty array)
        gen.emit_call("__simple_array_zeros");  // Creates empty array
        
        // Store array pointer in a safe stack location
        gen.emit_mov_mem_reg(-64, 0); // Save array pointer to stack[rbp-64]
        
        // Push each element into the array
        for (size_t i = 0; i < elements.size(); i++) {
            std::cout << "[NEW_CODEGEN] ArrayLiteral: Processing element " << i << std::endl;
            
            // Restore array pointer to a register
            gen.emit_mov_reg_mem(3, -64); // RBX = array pointer from stack[rbp-64]
            
            // Generate the element value
            elements[i]->generate_code(gen);
            // RAX now contains the element value
            
            // Set up parameters for __simple_array_push_int64(array_ptr, value)
            gen.emit_mov_reg_reg(7, 3); // RDI = array pointer (from RBX)
            gen.emit_mov_reg_reg(6, 0); // RSI = value (from RAX)
            
            std::cout << "[NEW_CODEGEN] ArrayLiteral: Calling __simple_array_push_int64 for element " << i << std::endl;
            gen.emit_call("__simple_array_push_int64");
        }
        
        // Return the array pointer in RAX
        gen.emit_mov_reg_mem(0, -64); // RAX = array pointer from stack[rbp-64]
    }
    
    std::cout << "[NEW_CODEGEN] ArrayLiteral::generate_code complete" << std::endl;
    result_type = DataType::ARRAY;
}

void ObjectLiteral::generate_code(CodeGenerator& gen) {
    std::cout << "[NEW_CODEGEN] ObjectLiteral::generate_code - Creating object with " << properties.size() << " properties" << std::endl;
    
    // Create an object using the runtime object system
    static const char* object_literal_class = "ObjectLiteral";
    
    // Call __object_create with class name and property count
    gen.emit_mov_reg_imm(7, reinterpret_cast<int64_t>(object_literal_class)); // RDI = class_name
    gen.emit_mov_reg_imm(6, properties.size()); // RSI = property count
    gen.emit_call("__object_create");
    
    // RAX now contains the object_id
    // Store it temporarily while we add properties
    gen.emit_mov_mem_reg(-72, 0); // Save object_id at stack location
    
    // Add each property to the object using property indices
    for (size_t i = 0; i < properties.size(); i++) {
        const auto& prop = properties[i];
        
        // Store property name in static storage for the runtime call
        static std::unordered_map<std::string, const char*> property_name_storage;
        auto it = property_name_storage.find(prop.first);
        const char* name_ptr;
        if (it != property_name_storage.end()) {
            name_ptr = it->second;
        } else {
            // Allocate permanent storage for this property name
            char* permanent_name = new char[prop.first.length() + 1];
            strcpy(permanent_name, prop.first.c_str());
            property_name_storage[prop.first] = permanent_name;
            name_ptr = permanent_name;
        }
        
        // Call __object_set_property_name(object_id, property_index, property_name)
        gen.emit_mov_reg_mem(7, -72); // RDI = object_id
        gen.emit_mov_reg_imm(6, i); // RSI = property_index
        gen.emit_mov_reg_imm(2, reinterpret_cast<int64_t>(name_ptr)); // RDX = property_name
        gen.emit_call("__object_set_property_name");
        
        // Generate code for the property value
        prop.second->generate_code(gen);
        
        // Set up call to __object_set_property(object_id, property_index, value)
        gen.emit_mov_reg_reg(2, 0); // RDX = value (save from RAX)
        gen.emit_mov_reg_mem(7, -72); // RDI = object_id
        gen.emit_mov_reg_imm(6, i); // RSI = property_index
        gen.emit_call("__object_set_property");
        
        std::cout << "[NEW_CODEGEN] ObjectLiteral: Added property '" << prop.first << "' at index " << i << std::endl;
    }
    
    // Return the object_id in RAX
    gen.emit_mov_reg_mem(0, -72);
    result_type = DataType::CLASS_INSTANCE; // Objects are class instances
    
    std::cout << "[NEW_CODEGEN] ObjectLiteral::generate_code complete" << std::endl;
}

void TypedArrayLiteral::generate_code(CodeGenerator& gen) {
    std::cout << "[NEW_CODEGEN] TypedArrayLiteral::generate_code - Creating typed array with " << elements.size() << " elements" << std::endl;
    
    // Create typed array with initial capacity - maximum performance
    gen.emit_mov_reg_imm(7, elements.size() > 0 ? elements.size() : 8); // RDI = initial capacity
    
    // Call appropriate typed array creation function based on type
    switch (array_type) {
        case DataType::INT32:
            gen.emit_call("__typed_array_create_int32");
            break;
        case DataType::INT64:
            gen.emit_call("__typed_array_create_int64");
            break;
        case DataType::FLOAT32:
            gen.emit_call("__typed_array_create_float32");
            break;
        case DataType::FLOAT64:
            // FLOAT64 is the standard numeric type (equivalent to JavaScript's number)
            gen.emit_call("__typed_array_create_float64");
            break;
        case DataType::UINT8:
            gen.emit_call("__typed_array_create_uint8");
            break;
        case DataType::UINT16:
            gen.emit_call("__typed_array_create_uint16");
            break;
        case DataType::UINT32:
            gen.emit_call("__typed_array_create_uint32");
            break;
        case DataType::UINT64:
            gen.emit_call("__typed_array_create_uint64");
            break;
        default:
            // For unsupported types, create a generic array
            gen.emit_call("__typed_array_create_int64");
            break;
    }
    
    gen.emit_mov_mem_reg(-80, 0); // Save array pointer on stack
    
    // Push each element into the typed array using appropriate typed push function
    for (const auto& element : elements) {
        element->generate_code(gen);
        gen.emit_mov_reg_mem(7, -80); // RDI = array pointer from stack
        gen.emit_mov_reg_reg(6, 0); // RSI = value to push
        
        // Call appropriate push function based on type for maximum performance
        switch (array_type) {
            case DataType::INT32:
                gen.emit_call("__typed_array_push_int32");
                break;
            case DataType::INT64:
                gen.emit_call("__typed_array_push_int64");
                break;
            case DataType::FLOAT32:
                gen.emit_call("__typed_array_push_float32");
                break;
            case DataType::FLOAT64:
                gen.emit_call("__typed_array_push_float64");
                break;
            case DataType::UINT8:
                gen.emit_call("__typed_array_push_uint8");
                break;
            case DataType::UINT16:
                gen.emit_call("__typed_array_push_uint16");
                break;
            case DataType::UINT32:
                gen.emit_call("__typed_array_push_uint32");
                break;
            case DataType::UINT64:
                gen.emit_call("__typed_array_push_uint64");
                break;
            default:
                // For unsupported types, use generic int64 push
                gen.emit_call("__typed_array_push_int64");
                break;
        }
    }
    
    // Return the array pointer in RAX
    gen.emit_mov_reg_mem(0, -80); // RAX = array pointer from stack
    result_type = DataType::ARRAY;
    
    std::cout << "[NEW_CODEGEN] TypedArrayLiteral::generate_code complete" << std::endl;
}

void ArrayAccess::generate_code(CodeGenerator& gen) {
    std::cout << "[NEW_CODEGEN] ArrayAccess::generate_code" << std::endl;
    
    // For now, implement simplified array access without class operator overloads
    // TODO: Add full class operator overload support with new scope system
    
    // Check if object is an identifier for simplified handling
    if (auto* var_expr = dynamic_cast<Identifier*>(object.get())) {
        std::cout << "[NEW_CODEGEN] ArrayAccess: Array variable '" << var_expr->name << "'" << std::endl;
        
        // Load the array variable using scope-aware access
        emit_variable_load(gen, var_expr->name);
        gen.emit_mov_reg_reg(7, 0); // RDI = array pointer
        
        // Generate code for the index
        if (index) {
            index->generate_code(gen);
            gen.emit_mov_reg_reg(6, 0); // RSI = index
        } else if (!slices.empty()) {
            // For slice syntax, use slice function
            slices[0]->generate_code(gen);
            gen.emit_mov_reg_reg(6, 0); // RSI = slice object
        } else {
            gen.emit_mov_reg_imm(6, 0); // RSI = 0 (default index)
        }
        
        gen.emit_call("__simple_array_get");
        result_type = DataType::FLOAT64;
        
    } else {
        // Standard array access for complex expressions
        // Generate code for the object expression
        object->generate_code(gen);
        
        // Save object on stack
        gen.emit_sub_reg_imm(4, 8);   // sub rsp, 8 (allocate stack space)
        if (auto x86_gen = dynamic_cast<X86CodeGenV2*>(&gen)) {
            x86_gen->emit_mov_mem_reg(0, 0);   // mov [rsp], rax (save object on stack)
        } else {
            gen.emit_mov_mem_reg(0, 0);   // fallback
        }
        
        // Generate code for the index expression
        if (index) {
            index->generate_code(gen);
        } else if (!slices.empty()) {
            // For new slice syntax, generate slice object code
            slices[0]->generate_code(gen);
        } else {
            // Fallback - generate a zero index
            gen.emit_mov_reg_imm(0, 0);
        }
        gen.emit_mov_reg_reg(6, 0); // Move index to RSI
        
        // Pop object into RDI
        if (auto x86_gen = dynamic_cast<X86CodeGenV2*>(&gen)) {
            x86_gen->emit_mov_reg_mem(7, 0);   // mov rdi, [rsp] (load object from stack)
        } else {
            gen.emit_mov_reg_mem(7, 0);   // fallback
        }
        gen.emit_add_reg_imm(4, 8);   // add rsp, 8 (restore stack)
        
        // Call array access function
        gen.emit_call("__array_access");
        
        // Result is in RAX
        result_type = DataType::ANY; // Array access returns unknown type for JavaScript compatibility
    }
}

void PostfixIncrement::generate_code(CodeGenerator& gen) {
    std::cout << "[NEW_CODEGEN] PostfixIncrement for variable: " << variable_name << std::endl;
    
    // Find the variable in the current scope context using the scope analyzer
    if (!g_scope_context.scope_analyzer) {
        throw std::runtime_error("No scope analyzer available for PostfixIncrement: " + variable_name);
    }
    
    auto var_info = g_scope_context.scope_analyzer->get_variable_declaration_info(variable_name);
    if (!var_info) {
        throw std::runtime_error("Variable declaration info not found for PostfixIncrement: " + variable_name);
    }
    
    auto def_scope = g_scope_context.scope_analyzer->get_definition_scope_for_variable(variable_name);
    if (!def_scope) {
        throw std::runtime_error("Definition scope not found for PostfixIncrement: " + variable_name);
    }
    
    // Get variable offset within its scope
    size_t var_offset = var_info->offset;
    
    // Load current value based on scope relationship
    if (def_scope == g_scope_context.current_scope) {
        // Variable is in current scope
        gen.emit_mov_reg_reg_offset(0, 15, var_offset); // rax = [r15 + offset]
        std::cout << "[NEW_CODEGEN] PostfixIncrement: loaded local variable '" << variable_name 
                  << "' from r15+" << var_offset << std::endl;
    } else {
        // Variable is in parent scope - find which register holds it
        int scope_depth = def_scope->scope_depth;
        auto reg_it = g_scope_context.scope_state.scope_depth_to_register.find(scope_depth);
        
        if (reg_it != g_scope_context.scope_state.scope_depth_to_register.end()) {
            // Parent scope is in a register
            int scope_reg = reg_it->second;
            gen.emit_mov_reg_reg_offset(0, scope_reg, var_offset); // rax = [r12/13/14 + offset]
            std::cout << "[NEW_CODEGEN] PostfixIncrement: loaded parent variable '" << variable_name 
                      << "' from r" << scope_reg << "+" << var_offset << std::endl;
        } else {
            throw std::runtime_error("Deep nested scope access not yet implemented for PostfixIncrement: " + variable_name);
        }
    }
    
    // Store the current value (this is the return value for postfix)
    gen.emit_mov_reg_reg(1, 0); // RCX = current value
    
    // Increment the value
    gen.emit_add_reg_imm(0, 1); // RAX = current + 1
    
    // Store the incremented value back based on scope relationship
    if (def_scope == g_scope_context.current_scope) {
        // Variable is in current scope
        gen.emit_mov_reg_offset_reg(15, var_offset, 0); // [r15 + offset] = rax
        std::cout << "[NEW_CODEGEN] PostfixIncrement: stored to local variable '" << variable_name 
                  << "' at r15+" << var_offset << std::endl;
    } else {
        // Variable is in parent scope
        int scope_depth = def_scope->scope_depth;
        auto reg_it = g_scope_context.scope_state.scope_depth_to_register.find(scope_depth);
        
        if (reg_it != g_scope_context.scope_state.scope_depth_to_register.end()) {
            // Parent scope is in a register
            int scope_reg = reg_it->second;
            gen.emit_mov_reg_offset_reg(scope_reg, var_offset, 0); // [r12/13/14 + offset] = rax
            std::cout << "[NEW_CODEGEN] PostfixIncrement: stored to parent variable '" << variable_name 
                      << "' at r" << scope_reg << "+" << var_offset << std::endl;
        } else {
            throw std::runtime_error("Deep nested scope store not yet implemented for PostfixIncrement: " + variable_name);
        }
    }
    
    // Return the original value (postfix semantics)
    gen.emit_mov_reg_reg(0, 1); // RAX = original value
    
    result_type = DataType::FLOAT64; // JavaScript compatibility
    std::cout << "[NEW_CODEGEN] PostfixIncrement completed for: " << variable_name << std::endl;
}

void PostfixDecrement::generate_code(CodeGenerator& gen) {
    std::cout << "[NEW_CODEGEN] PostfixDecrement for variable: " << variable_name << std::endl;
    
    // Load the current value using scope-aware access
    emit_variable_load(gen, variable_name);
    
    // Store the current value (this is the return value for postfix)
    gen.emit_mov_reg_reg(1, 0); // RCX = current value
    
    // Decrement the value
    gen.emit_sub_reg_imm(0, 1); // RAX = current - 1
    
    // Store the decremented value back using scope-aware access
    emit_variable_store(gen, variable_name);
    
    // Return the original value (postfix semantics)
    gen.emit_mov_reg_reg(0, 1); // RAX = original value
    
    result_type = DataType::FLOAT64; // JavaScript compatibility
}

void FunctionDecl::generate_code(CodeGenerator& gen) {
    std::cout << "[NEW_CODEGEN] FunctionDecl::generate_code - function: " << name << std::endl;
    
    // Reset scope context for new function
    g_scope_context.reset_for_function();
    
    // Emit function label
    gen.emit_label(name);
    
    // Calculate estimated stack size (parameters + locals + temporaries)
    int64_t estimated_stack_size = (parameters.size() * 8) + (body.size() * 16) + 128;
    // Ensure minimum stack size and 16-byte alignment
    if (estimated_stack_size < 128) estimated_stack_size = 128;
    if (estimated_stack_size % 16 != 0) {
        estimated_stack_size += 16 - (estimated_stack_size % 16);
    }
    
    // Set stack size for this function (if X86CodeGenV2 supports it)
    if (auto x86_gen = dynamic_cast<X86CodeGenV2*>(&gen)) {
        // TODO: Add set_function_stack_size method to X86CodeGenV2 if needed
        std::cout << "[NEW_CODEGEN] FunctionDecl: estimated stack size = " << estimated_stack_size << std::endl;
    }
    
    // Emit function prologue
    gen.emit_prologue();
    
    // If this function has an associated scope from scope analysis, enter it
    if (lexical_scope) {
        std::cout << "[NEW_CODEGEN] FunctionDecl: entering function scope" << std::endl;
        emit_scope_enter(gen, lexical_scope.get());
    }
    
    // Save parameters from registers to their scope locations
    // TODO: Use scope-aware parameter storage once parameter offsets are available
    for (size_t i = 0; i < parameters.size() && i < 6; i++) {
        const auto& param = parameters[i];
        
        // For now, use simple stack offsets for parameters
        int stack_offset = -(int)(i + 1) * 8;  // Start at -8, -16, -24 etc
        
        switch (i) {
            case 0: gen.emit_mov_mem_reg(stack_offset, 7); break;  // save RDI
            case 1: gen.emit_mov_mem_reg(stack_offset, 6); break;  // save RSI
            case 2: gen.emit_mov_mem_reg(stack_offset, 2); break;  // save RDX
            case 3: gen.emit_mov_mem_reg(stack_offset, 1); break;  // save RCX
            case 4: gen.emit_mov_mem_reg(stack_offset, 8); break;  // save R8
            case 5: gen.emit_mov_mem_reg(stack_offset, 9); break;  // save R9
        }
        
        std::cout << "[NEW_CODEGEN] FunctionDecl: saved parameter '" << param.name 
                  << "' at offset " << stack_offset << std::endl;
    }
    
    // Handle stack parameters (beyond first 6)
    for (size_t i = 6; i < parameters.size(); i++) {
        const auto& param = parameters[i];
        // Stack parameters are at positive offsets from RBP
        int stack_offset = (int)(i - 6 + 2) * 8;  // +16 for return addr and old RBP, then +8 for each param
        std::cout << "[NEW_CODEGEN] FunctionDecl: stack parameter '" << param.name 
                  << "' at offset " << stack_offset << std::endl;
    }
    
    // Generate function body
    bool has_explicit_return = false;
    for (const auto& stmt : body) {
        stmt->generate_code(gen);
        
        // Check if this statement is a return statement
        if (dynamic_cast<const ReturnStatement*>(stmt.get())) {
            has_explicit_return = true;
        }
    }
    
    // If no explicit return, add implicit return 0
    if (!has_explicit_return) {
        gen.emit_mov_reg_imm(0, 0);  // mov rax, 0 (default return value)
        gen.emit_function_return();
    }
    
    // Exit function scope if we entered one
    if (lexical_scope) {
        std::cout << "[NEW_CODEGEN] FunctionDecl: exiting function scope" << std::endl;
        emit_scope_exit(gen, lexical_scope.get());
    }
    
    std::cout << "[NEW_CODEGEN] FunctionDecl::generate_code complete for " << name << std::endl;
}

void IfStatement::generate_code(CodeGenerator& gen) {
    static int if_counter = 0;
    std::string else_label = "else_" + std::to_string(if_counter);
    std::string end_label = "end_if_" + std::to_string(if_counter);
    if_counter++;
    
    std::cout << "[NEW_CODEGEN] IfStatement::generate_code - generating if/else with labels " 
              << else_label << " and " << end_label << std::endl;
    
    // Generate condition code - this puts the result in RAX
    condition->generate_code(gen);
    
    // Compare RAX with 0 (false) - JavaScript truthiness
    gen.emit_mov_reg_imm(1, 0);      // RCX = 0
    gen.emit_compare(0, 1);          // Compare RAX with RCX (0)
    gen.emit_jump_if_zero(else_label); // Jump to else if RAX == 0 (false)
    
    // Generate then body
    std::cout << "[NEW_CODEGEN] IfStatement: generating then body (" << then_body.size() << " statements)" << std::endl;
    for (const auto& stmt : then_body) {
        stmt->generate_code(gen);
    }
    
    // Skip else body
    gen.emit_jump(end_label);
    
    // Generate else body
    gen.emit_label(else_label);
    std::cout << "[NEW_CODEGEN] IfStatement: generating else body (" << else_body.size() << " statements)" << std::endl;
    for (const auto& stmt : else_body) {
        stmt->generate_code(gen);
    }
    
    gen.emit_label(end_label);
    std::cout << "[NEW_CODEGEN] IfStatement::generate_code complete" << std::endl;
}

void ForLoop::generate_code(CodeGenerator& gen) {
    static int loop_counter = 0;
    std::string loop_start = "loop_start_" + std::to_string(loop_counter);
    std::string loop_end = "loop_end_" + std::to_string(loop_counter);
    loop_counter++;
    
    std::cout << "[NEW_CODEGEN] ForLoop::generate_code - generating loop with labels " 
              << loop_start << " and " << loop_end << std::endl;
    
    // For for-loops that create their own block scope (let/const), we need to enter that scope
    if (creates_block_scope && init) {
        // The Assignment node in init should have the scope information
        if (auto* assignment = dynamic_cast<Assignment*>(init.get())) {
            if (assignment->assignment_scope && assignment->assignment_scope != g_scope_context.current_scope) {
                std::cout << "[NEW_CODEGEN] ForLoop: entering for-loop scope" << std::endl;
                emit_scope_enter(gen, assignment->assignment_scope);
            }
        }
    }
    
    // Generate initialization code
    if (init) {
        std::cout << "[NEW_CODEGEN] ForLoop: generating init statement" << std::endl;
        init->generate_code(gen);
    }
    
    gen.emit_label(loop_start);
    
    // Generate condition check
    if (condition) {
        std::cout << "[NEW_CODEGEN] ForLoop: generating condition check" << std::endl;
        condition->generate_code(gen);
        // Check if RAX (result of condition) is zero (false)
        gen.emit_mov_reg_imm(1, 0); // RCX = 0
        gen.emit_compare(0, 1); // Compare RAX with 0
        gen.emit_jump_if_zero(loop_end);
    }
    
    // Generate loop body
    std::cout << "[NEW_CODEGEN] ForLoop: generating loop body (" << body.size() << " statements)" << std::endl;
    for (const auto& stmt : body) {
        stmt->generate_code(gen);
    }
    
    // Generate update statement
    if (update) {
        std::cout << "[NEW_CODEGEN] ForLoop: generating update statement" << std::endl;
        update->generate_code(gen);
    }
    
    gen.emit_jump(loop_start);
    gen.emit_label(loop_end);
    
    // Exit the for-loop scope if we entered one
    if (creates_block_scope && init) {
        if (auto* assignment = dynamic_cast<Assignment*>(init.get())) {
            if (assignment->assignment_scope && assignment->assignment_scope != g_scope_context.current_scope) {
                std::cout << "[NEW_CODEGEN] ForLoop: exiting for-loop scope" << std::endl;
                emit_scope_exit(gen, assignment->assignment_scope);
            }
        }
    }
    
    std::cout << "[NEW_CODEGEN] ForLoop::generate_code complete" << std::endl;
}

// Add more placeholder implementations as needed for other AST nodes...

void WhileLoop::generate_code(CodeGenerator& gen) {
    static int while_counter = 0;
    std::string loop_start = "while_start_" + std::to_string(while_counter);
    std::string loop_end = "while_end_" + std::to_string(while_counter);
    while_counter++;
    
    std::cout << "[NEW_CODEGEN] WhileLoop::generate_code - generating while loop with labels " 
              << loop_start << " and " << loop_end << std::endl;
    
    gen.emit_label(loop_start);
    
    // Generate condition check
    std::cout << "[NEW_CODEGEN] WhileLoop: generating condition check" << std::endl;
    condition->generate_code(gen);
    
    // Check if RAX (result of condition) is zero (false)
    gen.emit_mov_reg_imm(1, 0); // RCX = 0
    gen.emit_compare(0, 1); // Compare RAX with 0
    gen.emit_jump_if_zero(loop_end);
    
    // Generate loop body
    std::cout << "[NEW_CODEGEN] WhileLoop: generating loop body (" << body.size() << " statements)" << std::endl;
    for (const auto& stmt : body) {
        stmt->generate_code(gen);
    }
    
    gen.emit_jump(loop_start);
    gen.emit_label(loop_end);
    std::cout << "[NEW_CODEGEN] WhileLoop::generate_code complete" << std::endl;
}

void ReturnStatement::generate_code(CodeGenerator& gen) {
    std::cout << "[NEW_CODEGEN] ReturnStatement::generate_code" << std::endl;
    
    if (value) {
        std::cout << "[NEW_CODEGEN] ReturnStatement: generating return value" << std::endl;
        value->generate_code(gen);
        // Value is now in RAX, which is the standard return register
    } else {
        // No return value - return 0 (undefined/void)
        gen.emit_mov_reg_imm(0, 0);
    }
    
    // Use function return to properly restore stack frame and return
    gen.emit_function_return();
    std::cout << "[NEW_CODEGEN] ReturnStatement::generate_code complete" << std::endl;
}

// Global variable to track current break target for loops/switches
static std::string current_break_target = "";

void BreakStatement::generate_code(CodeGenerator& gen) {
    std::cout << "[NEW_CODEGEN] BreakStatement::generate_code" << std::endl;
    
    if (!current_break_target.empty()) {
        std::cout << "[NEW_CODEGEN] BreakStatement: jumping to " << current_break_target << std::endl;
        gen.emit_jump(current_break_target);
    } else {
        // No active switch/loop context
        std::cout << "[NEW_CODEGEN] BreakStatement: no active loop/switch context" << std::endl;
        gen.emit_label("__break_without_context");
    }
    
    std::cout << "[NEW_CODEGEN] BreakStatement::generate_code complete" << std::endl;
}

void FreeStatement::generate_code(CodeGenerator& gen) {
    std::cout << "[NEW_CODEGEN] FreeStatement::generate_code - freeing target expression" << std::endl;
    
    // Generate code for the target expression to get its value
    target->generate_code(gen);
    
    // Call the runtime free function
    gen.emit_mov_reg_reg(7, 0); // RDI = pointer to free (from RAX)
    gen.emit_call("__runtime_free");
    
    // Note: For expression targets (not simple variables), we can't set them to null
    // The runtime free function handles the deallocation
    
    std::cout << "[NEW_CODEGEN] FreeStatement::generate_code complete" << std::endl;
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
    std::cout << "[NEW_CODEGEN] BlockStatement::generate_code - " << body.size() << " statements" << std::endl;
    
    // If this block has its own lexical scope (from scope analysis), enter it
    if (lexical_scope && lexical_scope.get() != g_scope_context.current_scope) {
        std::cout << "[NEW_CODEGEN] BlockStatement: entering block scope" << std::endl;
        emit_scope_enter(gen, lexical_scope.get());
        
        // Generate all statements in the block
        for (const auto& stmt : body) {
            stmt->generate_code(gen);
        }
        
        // Exit the block scope
        emit_scope_exit(gen, lexical_scope.get());
        std::cout << "[NEW_CODEGEN] BlockStatement: exited block scope" << std::endl;
    } else {
        // No separate scope needed, just generate statements
        for (const auto& stmt : body) {
            stmt->generate_code(gen);
        }
    }
    
    std::cout << "[NEW_CODEGEN] BlockStatement::generate_code complete" << std::endl;
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
    std::cout << "[NEW_CODEGEN] PropertyAccess::generate_code - " << object_name << "." << property_name << std::endl;
    
    // Load the object using scope-aware access
    emit_variable_load(gen, object_name);
    
    // Save object pointer to stack
    gen.emit_mov_mem_reg(-88, 0); // Save object at stack location
    
    // Create a string for the property name
    static std::unordered_map<std::string, const char*> property_name_storage;
    auto it = property_name_storage.find(property_name);
    const char* name_ptr;
    if (it != property_name_storage.end()) {
        name_ptr = it->second;
    } else {
        // Allocate permanent storage for this property name
        char* permanent_name = new char[property_name.length() + 1];
        strcpy(permanent_name, property_name.c_str());
        property_name_storage[property_name] = permanent_name;
        name_ptr = permanent_name;
    }
    
    // Call __object_get_property(object_ptr, property_name)
    gen.emit_mov_reg_mem(7, -88); // RDI = object pointer
    gen.emit_mov_reg_imm(6, reinterpret_cast<int64_t>(name_ptr)); // RSI = property name
    gen.emit_call("__object_get_property");
    
    // Result is now in RAX
    result_type = DataType::ANY; // Property access can return any type
    
    std::cout << "[NEW_CODEGEN] PropertyAccess: loaded property '" << property_name 
              << "' from object '" << object_name << "'" << std::endl;
}

void ExpressionPropertyAccess::generate_code(CodeGenerator& gen) {
    // TODO: Implement expression property access
    std::cout << "[NEW_CODEGEN] ExpressionPropertyAccess placeholder" << std::endl;
}

void PropertyAssignment::generate_code(CodeGenerator& gen) {
    std::cout << "[NEW_CODEGEN] PropertyAssignment::generate_code - " << object_name << "." << property_name << " = value" << std::endl;
    
    // Generate the value first
    if (value) {
        value->generate_code(gen);
        // Save the value to stack
        gen.emit_mov_mem_reg(-96, 0); // Save value at stack location
    }
    
    // Load the object using scope-aware access
    emit_variable_load(gen, object_name);
    
    // Save object pointer to stack
    gen.emit_mov_mem_reg(-104, 0); // Save object at stack location
    
    // Create a string for the property name
    static std::unordered_map<std::string, const char*> property_name_storage;
    auto it = property_name_storage.find(property_name);
    const char* name_ptr;
    if (it != property_name_storage.end()) {
        name_ptr = it->second;
    } else {
        // Allocate permanent storage for this property name
        char* permanent_name = new char[property_name.length() + 1];
        strcpy(permanent_name, property_name.c_str());
        property_name_storage[property_name] = permanent_name;
        name_ptr = permanent_name;
    }
    
    // Call __object_set_property(object_ptr, property_name, value)
    gen.emit_mov_reg_mem(7, -104); // RDI = object pointer
    gen.emit_mov_reg_imm(6, reinterpret_cast<int64_t>(name_ptr)); // RSI = property name
    gen.emit_mov_reg_mem(2, -96); // RDX = value
    gen.emit_call("__object_set_property");
    
    // Return the assigned value in RAX
    gen.emit_mov_reg_mem(0, -96); // RAX = assigned value
    result_type = value ? value->result_type : DataType::ANY;
    
    std::cout << "[NEW_CODEGEN] PropertyAssignment: set property '" << property_name 
              << "' on object '" << object_name << "'" << std::endl;
}

void ExpressionPropertyAssignment::generate_code(CodeGenerator& gen) {
    // TODO: Implement expression property assignment
    std::cout << "[NEW_CODEGEN] ExpressionPropertyAssignment placeholder" << std::endl;
}

void ThisExpression::generate_code(CodeGenerator& gen) {
    std::cout << "[NEW_CODEGEN] ThisExpression::generate_code" << std::endl;
    
    // In the new scope system, 'this' should be available through scope context
    if (!g_scope_context.current_class_name.empty()) {
        // We are in a class context - load 'this' from the implicit parameter
        // In method calls, 'this' is typically passed as the first hidden parameter
        gen.emit_mov_reg_mem(0, -128); // Load 'this' from standard location (placeholder)
        result_type = DataType::CLASS_INSTANCE;
        
        std::cout << "[NEW_CODEGEN] ThisExpression: loaded 'this' for class " 
                  << g_scope_context.current_class_name << std::endl;
    } else {
        // Outside of class context - 'this' might be global object or undefined
        // For now, return null/undefined
        gen.emit_mov_reg_imm(0, 0); // RAX = 0 (null/undefined)
        result_type = DataType::ANY;
        
        std::cout << "[NEW_CODEGEN] ThisExpression: 'this' outside class context, returning null" << std::endl;
    }
}

void NewExpression::generate_code(CodeGenerator& gen) {
    std::cout << "[NEW_CODEGEN] NewExpression::generate_code - new " << class_name << std::endl;
    
    // Create object instance - for now use default property count
    int64_t property_count = arguments.size() + 4; // Estimate based on constructor arguments
    
    // Setup string pooling for class name
    static std::unordered_map<std::string, const char*> class_name_pool;
    
    auto it = class_name_pool.find(class_name);
    if (it == class_name_pool.end()) {
        char* name_copy = new char[class_name.length() + 1];
        strcpy(name_copy, class_name.c_str());
        class_name_pool[class_name] = name_copy;
        it = class_name_pool.find(class_name);
    }
    
    // Call __object_create(class_name, property_count)
    gen.emit_mov_reg_imm(7, reinterpret_cast<int64_t>(it->second)); // RDI = class_name
    gen.emit_mov_reg_imm(6, property_count); // RSI = property_count
    gen.emit_call("__object_create");
    
    // __object_create returns object_id in RAX
    // Store object_id temporarily for constructor call
    gen.emit_mov_mem_reg(-112, 0); // Save object_id on stack
    
    // Generate constructor arguments
    std::vector<int64_t> arg_offsets;
    for (size_t i = 0; i < arguments.size(); i++) {
        arguments[i]->generate_code(gen);
        
        // Store argument value in temporary stack location
        int64_t arg_offset = -120 - (i * 8);
        gen.emit_mov_mem_reg(arg_offset, 0);
        arg_offsets.push_back(arg_offset);
    }
    
    // Set up constructor call with object_id as 'this' parameter
    std::string constructor_label = "__constructor_" + class_name;
    
    // Load object_id into RDI (this)
    gen.emit_mov_reg_mem(7, -112);
    
    // Load constructor arguments into appropriate registers
    for (size_t i = 0; i < arguments.size() && i < 5; i++) {
        switch (i) {
            case 0: gen.emit_mov_reg_mem(6, arg_offsets[i]); break; // RSI
            case 1: gen.emit_mov_reg_mem(2, arg_offsets[i]); break; // RDX  
            case 2: gen.emit_mov_reg_mem(1, arg_offsets[i]); break; // RCX
            case 3: gen.emit_mov_reg_mem(8, arg_offsets[i]); break; // R8
            case 4: gen.emit_mov_reg_mem(9, arg_offsets[i]); break; // R9
        }
    }
    
    // Call the constructor function
    gen.emit_call(constructor_label);
    
    // Restore object_id to RAX for return value
    gen.emit_mov_reg_mem(0, -112);
    result_type = DataType::CLASS_INSTANCE;
    
    std::cout << "[NEW_CODEGEN] NewExpression: created instance of " << class_name << std::endl;
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
