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
        
        // Get variable offset directly from declaration info
        size_t var_offset = variable_declaration_info->offset;
        
        // Generate optimal code based on scope relationship
        if (definition_scope == g_scope_context.current_scope) {
            // Variable is in current scope - use r15 + offset (fastest path)
            gen.emit_mov_reg_reg_offset(0, 15, var_offset); // rax = [r15 + offset]
            std::cout << "[NEW_CODEGEN] Loaded local variable '" << name << "' from r15+" << var_offset 
                      << " (type=" << static_cast<int>(result_type) << ")" << std::endl;
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
            
            // Get variable offset directly from declaration info
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
                // Numeric addition
                result_type = DataType::FLOAT64; // JavaScript compatibility
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
            result_type = DataType::FLOAT64;
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
            result_type = DataType::FLOAT64;
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
            
        case TokenType::DIVIDE:
            result_type = DataType::FLOAT64;
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
    
    // Store flags string
    static std::unordered_map<std::string, const char*> flags_storage;
    
    auto flags_it = flags_storage.find(flags);
    const char* flags_ptr;
    if (flags_it != flags_storage.end()) {
        flags_ptr = flags_it->second;
    } else {
        // Allocate permanent storage for this flags string
        char* permanent_flags = new char[flags.length() + 1];
        strcpy(permanent_flags, flags.c_str());
        flags_storage[flags] = permanent_flags;
        flags_ptr = permanent_flags;
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
    
    // Register the pattern with the runtime first
    gen.emit_mov_reg_imm(7, reinterpret_cast<int64_t>(pattern_ptr)); // RDI = pattern string (permanent storage)
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
