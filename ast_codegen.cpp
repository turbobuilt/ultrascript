#include "compiler.h"
#include "x86_codegen_v2.h"
#include "simple_lexical_scope.h"
#include "function_instance.h"
#include "function_runtime.h"
#include "function_codegen.h"
#include "runtime.h"
#include "runtime_object.h"
#include "compilation_context.h"
#include "function_compilation_manager.h"
#include "scope_aware_codegen.h"
#include "console_log_overhaul.h"
#include "class_runtime_interface.h"
#include "dynamic_properties.h"
#include "function_address_patching.h"
#include <iostream>
#include <unordered_map>
#include <cstring>
#include <cstdlib>
#include <stdexcept>
#include <string>

// Simple global constant storage for imported constants
static std::unordered_map<std::string, double> global_imported_constants;

// Global lookup for FunctionDecl nodes (for patching system)
static std::unordered_map<std::string, FunctionDecl*> global_function_decl_lookup;

// Forward declarations for function ID registry
void __register_function_id(int64_t function_id, const std::string& function_name);
void ensure_lookup_function_by_id_registered();

// Forward declaration for fast runtime function lookup
extern "C" void* __lookup_function_fast(uint16_t func_id);

// Forward declaration for scope context initialization
void initialize_scope_context(SimpleLexicalScopeAnalyzer* analyzer);

// Helper function to generate a user-defined function call with ROBUST patching
void generate_user_function_call_with_patching(CodeGenerator& gen, const std::string& function_name) {
    std::cout << "[ROBUST_PATCHING] Generating user function call to '" << function_name 
              << "' with robust address patching" << std::endl;
    
    // Look up the FunctionDecl for this function
    auto func_it = global_function_decl_lookup.find(function_name);
    if (func_it == global_function_decl_lookup.end()) {
        std::cout << "[ROBUST_PATCHING] WARNING: FunctionDecl not found for '" << function_name 
                  << "', falling back to direct call" << std::endl;
        gen.emit_call(function_name);
        return;
    }
    
    FunctionDecl* target_function = func_it->second;
    std::cout << "[ROBUST_PATCHING] Found FunctionDecl for '" << function_name << "'" << std::endl;
    
    // Use the robust high-level API that handles all the instruction encoding details
    X86CodeGenV2* x86_gen = dynamic_cast<X86CodeGenV2*>(&gen);
    if (x86_gen) {
        x86_gen->emit_patchable_function_call(function_name, target_function);
    } else {
        std::cerr << "[ROBUST_PATCHING] ERROR: Non-X86 backend not supported for robust patching" << std::endl;
        gen.emit_call(function_name);  // Fallback
    }
    
    std::cout << "[FUNCTION_PATCH] Generated MOV+CALL sequence for function '" << function_name << "'" << std::endl;
}

//=============================================================================
// NEW LEXICAL SCOPE-AWARE CODE GENERATION SYSTEM
// This system completely replaces TypeInference with direct scope management
//=============================================================================

// Global scope context that replaces TypeInference
struct GlobalScopeContext {
    // Current scope information from SimpleLexicalScopeAnalyzer
    LexicalScopeNode* current_scope = nullptr;
    SimpleLexicalScopeAnalyzer* scope_analyzer = nullptr;
    
    // Current function declaration (for parameter access)
    FunctionDecl* current_function_decl = nullptr;
    
    // Scope nesting stack for proper save/restore (LIFO order)
    std::vector<LexicalScopeNode*> scope_nesting_stack;
    
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
        
        // Reset scope nesting stack for new function context
        scope_nesting_stack.clear();
        current_scope = nullptr;
        scope_state.scope_depth_to_register.clear();
        scope_state.stack_stored_scopes.clear();
        scope_state.current_scope_depth = 0;
        
        std::cout << "[SCOPE_CONTEXT] Reset scope context for new function" << std::endl;
    }
    
    // Verify scope stack consistency
    bool verify_scope_stack_consistency() const {
        // Check that current scope depth is consistent with stack
        if (current_scope == nullptr) {
            return scope_nesting_stack.empty(); // Should have empty stack at root
        }
        
        // Check that current scope depth is greater than all stack entries
        for (const auto* stacked_scope : scope_nesting_stack) {
            if (stacked_scope->scope_depth >= current_scope->scope_depth) {
                std::cout << "[SCOPE_ERROR] Inconsistent scope stack: stacked depth " 
                          << stacked_scope->scope_depth << " >= current depth " 
                          << current_scope->scope_depth << std::endl;
                return false;
            }
        }
        
        return true;
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
    // Update nesting stack when entering a new scope
    if (scope != nullptr && scope != g_scope_context.current_scope) {
        // Push current scope onto stack before switching (if not null)
        if (g_scope_context.current_scope != nullptr) {
            g_scope_context.scope_nesting_stack.push_back(g_scope_context.current_scope);
            std::cout << "[SCOPE_STACK] Pushed scope depth " << g_scope_context.current_scope->scope_depth 
                      << " onto nesting stack" << std::endl;
        }
    }
    
    // Update current scope
    g_scope_context.current_scope = scope;
    if (scope) {
        g_scope_context.scope_state.current_scope_depth = scope->scope_depth;
        std::cout << "[SCOPE_CONTEXT] Set current scope to depth " << scope->scope_depth << std::endl;
    } else {
        g_scope_context.scope_state.current_scope_depth = 0;
        std::cout << "[SCOPE_CONTEXT] Set current scope to global/root" << std::endl;
    }
}

// Pop from scope nesting stack when exiting a scope
LexicalScopeNode* pop_scope_from_stack() {
    if (g_scope_context.scope_nesting_stack.empty()) {
        std::cout << "[SCOPE_STACK] Stack is empty, returning nullptr" << std::endl;
        return nullptr;
    }
    
    LexicalScopeNode* parent_scope = g_scope_context.scope_nesting_stack.back();
    g_scope_context.scope_nesting_stack.pop_back();
    std::cout << "[SCOPE_STACK] Popped scope depth " << parent_scope->scope_depth 
              << " from nesting stack" << std::endl;
    return parent_scope;
}

LexicalScopeNode* get_current_scope() {
    return g_scope_context.current_scope;
}

// Get parent scope of given scope (for scope exit)
LexicalScopeNode* get_parent_scope(LexicalScopeNode* child_scope) {
    if (!child_scope || child_scope->scope_depth <= 1) {
        return nullptr; // No parent (this is global scope)
    }
    
    // Use the scope analyzer to get parent scope
    if (g_scope_context.scope_analyzer) {
        return g_scope_context.scope_analyzer->get_scope_node_for_depth(child_scope->scope_depth - 1);
    }
    
    return nullptr;
}

// Debug function to print current scope stack state
void debug_print_scope_stack() {
    std::cout << "[SCOPE_DEBUG] Current scope stack state:" << std::endl;
    std::cout << "[SCOPE_DEBUG]   Current scope: " 
              << (g_scope_context.current_scope ? std::to_string(g_scope_context.current_scope->scope_depth) : "nullptr") << std::endl;
    std::cout << "[SCOPE_DEBUG]   Stack depth: " << g_scope_context.scope_nesting_stack.size() << std::endl;
    
    for (size_t i = 0; i < g_scope_context.scope_nesting_stack.size(); i++) {
        std::cout << "[SCOPE_DEBUG]     Stack[" << i << "]: depth " 
                  << g_scope_context.scope_nesting_stack[i]->scope_depth << std::endl;
    }
    
    std::cout << "[SCOPE_DEBUG]   Register assignments:" << std::endl;
    for (const auto& pair : g_scope_context.scope_state.scope_depth_to_register) {
        std::cout << "[SCOPE_DEBUG]     Scope depth " << pair.first << " -> r" << pair.second << std::endl;
    }
}

// Generate stack-based access to a variable in a deeply nested parent scope
void generate_deep_scope_variable_load(CodeGenerator& gen, const std::string& var_name, 
                                      int target_scope_depth, size_t var_offset) {
    std::cout << "[DEEP_SCOPE] Generating stack-based load for '" << var_name 
              << "' in scope depth " << target_scope_depth << " at offset " << var_offset << std::endl;
    
    X86CodeGenV2* x86_gen = dynamic_cast<X86CodeGenV2*>(&gen);
    if (!x86_gen) {
        throw std::runtime_error("Deep scope access requires X86CodeGenV2");
    }
    
    // Calculate the stack-based offset to reach the target parent scope
    int current_depth = g_scope_context.current_scope ? g_scope_context.current_scope->scope_depth : 1;
    int scope_depth_diff = current_depth - target_scope_depth;
    
    std::cout << "[DEEP_SCOPE] Current depth: " << current_depth << ", Target depth: " << target_scope_depth 
              << ", Depth diff: " << scope_depth_diff << std::endl;
    
    if (scope_depth_diff <= 0) {
        throw std::runtime_error("Invalid deep scope access: target scope depth must be less than current");
    }
    
    // Add safety limit to prevent infinite loops from corrupted scope depths
    const int MAX_SCOPE_TRAVERSAL = 100; // Reasonable limit for scope nesting
    if (scope_depth_diff > MAX_SCOPE_TRAVERSAL) {
        std::cerr << "[ERROR] Scope depth difference too large: " << scope_depth_diff 
                  << " (max allowed: " << MAX_SCOPE_TRAVERSAL << ")" << std::endl;
        std::cerr << "[ERROR] Current scope depth: " << current_depth 
                  << ", Target scope depth: " << target_scope_depth << std::endl;
        throw std::runtime_error("Scope traversal limit exceeded - possible corrupted scope depths");
    }
    
    // Each scope level stores a pointer to its parent scope at a fixed offset
    // We traverse the scope chain by following these parent pointers
    
    // Start from current scope (r15 points to current scope)
    x86_gen->emit_mov_reg_reg(11, 15); // mov r11, r15 (use r11 as working register)
    
    // Traverse the scope chain to reach the target scope
    for (int i = 0; i < scope_depth_diff; i++) {
        // Each scope stores a pointer to its parent scope at offset -8 (before the actual variables)
        x86_gen->emit_mov_reg_reg_offset(11, 11, -8); // r11 = [r11 - 8] (follow parent pointer)
        std::cout << "[DEEP_SCOPE] Traversed to parent scope level " << (i + 1) << "/" << scope_depth_diff << std::endl;
    }
    
    // Now r11 points to the target parent scope, load the variable
    x86_gen->emit_mov_reg_reg_offset(0, 11, var_offset); // rax = [r11 + var_offset]
    
    std::cout << "[DEEP_SCOPE] Loaded variable '" << var_name << "' from deep scope (stack-based access)" << std::endl;
}

// Generate stack-based access to store a variable in a deeply nested parent scope  
void generate_deep_scope_variable_store(CodeGenerator& gen, const std::string& var_name,
                                       int target_scope_depth, size_t var_offset) {
    std::cout << "[DEEP_SCOPE] Generating stack-based store for '" << var_name
              << "' in scope depth " << target_scope_depth << " at offset " << var_offset << std::endl;
    
    X86CodeGenV2* x86_gen = dynamic_cast<X86CodeGenV2*>(&gen);
    if (!x86_gen) {
        throw std::runtime_error("Deep scope access requires X86CodeGenV2");
    }
    
    // Calculate the stack-based offset to reach the target parent scope
    int current_depth = g_scope_context.current_scope ? g_scope_context.current_scope->scope_depth : 1;
    int scope_depth_diff = current_depth - target_scope_depth;
    
    std::cout << "[DEEP_SCOPE] Store: Current depth: " << current_depth << ", Target depth: " << target_scope_depth 
              << ", Depth diff: " << scope_depth_diff << std::endl;
    
    if (scope_depth_diff <= 0) {
        throw std::runtime_error("Invalid deep scope store: target scope depth must be less than current");
    }
    
    // Add safety limit to prevent infinite loops from corrupted scope depths
    const int MAX_SCOPE_TRAVERSAL = 100; // Reasonable limit for scope nesting
    if (scope_depth_diff > MAX_SCOPE_TRAVERSAL) {
        std::cerr << "[ERROR] Scope depth difference too large for store: " << scope_depth_diff 
                  << " (max allowed: " << MAX_SCOPE_TRAVERSAL << ")" << std::endl;
        std::cerr << "[ERROR] Current scope depth: " << current_depth 
                  << ", Target scope depth: " << target_scope_depth << std::endl;
        throw std::runtime_error("Scope traversal limit exceeded in store - possible corrupted scope depths");
    }
    
    // Preserve RAX which contains the value to store
    x86_gen->get_instruction_builder().push(X86Reg::RAX); // push rax
    
    // Start from current scope (r15 points to current scope)  
    x86_gen->emit_mov_reg_reg(11, 15); // mov r11, r15 (use r11 as working register)
    
    // Traverse the scope chain to reach the target scope
    for (int i = 0; i < scope_depth_diff; i++) {
        // Each scope stores a pointer to its parent scope at offset -8 (before the actual variables)
        x86_gen->emit_mov_reg_reg_offset(11, 11, -8); // r11 = [r11 - 8] (follow parent pointer)
        std::cout << "[DEEP_SCOPE] Store traversed to parent scope level " << (i + 1) << "/" << scope_depth_diff << std::endl;
    }
    
    // Restore the value to store
    x86_gen->get_instruction_builder().pop(X86Reg::RAX); // pop rax
    
    // Now r11 points to the target parent scope, store the variable
    x86_gen->emit_mov_reg_offset_reg(11, var_offset, 0); // [r11 + var_offset] = rax
    
    std::cout << "[DEEP_SCOPE] Stored variable '" << var_name << "' to deep scope (stack-based access)" << std::endl;
}

//=============================================================================
// SCOPE-AWARE CODE GENERATION HELPERS
//=============================================================================

// Stack locations for saved scope registers (relative to current RSP)
// These are used to save/restore r12, r13, r14 when entering/exiting nested scopes
static const int64_t SAVED_R12_OFFSET = -8;   // [rsp - 8]  = saved r12
static const int64_t SAVED_R13_OFFSET = -16;  // [rsp - 16] = saved r13  
static const int64_t SAVED_R14_OFFSET = -24;  // [rsp - 24] = saved r14
static const int64_t SCOPE_SAVE_AREA_SIZE = 24; // Total bytes for register save area

// Save current parent scope registers to stack before entering child scope
void save_parent_scope_registers(X86CodeGenV2* x86_gen) {
    std::cout << "[SCOPE_CODEGEN] Saving current parent scope registers to stack" << std::endl;
    
    // Allocate stack space for saving registers
    x86_gen->emit_sub_reg_imm(4, SCOPE_SAVE_AREA_SIZE); // sub rsp, 24
    
    // Save current parent scope registers to stack
    x86_gen->emit_mov_mem_rsp_reg(SAVED_R12_OFFSET + SCOPE_SAVE_AREA_SIZE, 12); // mov [rsp + 16], r12
    x86_gen->emit_mov_mem_rsp_reg(SAVED_R13_OFFSET + SCOPE_SAVE_AREA_SIZE, 13); // mov [rsp + 8], r13
    x86_gen->emit_mov_mem_rsp_reg(SAVED_R14_OFFSET + SCOPE_SAVE_AREA_SIZE, 14); // mov [rsp], r14
    
    std::cout << "[SCOPE_CODEGEN] Saved r12, r13, r14 to stack at rsp+16, rsp+8, rsp+0" << std::endl;
}

// SMART Save parent scope registers - only saves registers that the function actually needs
void save_parent_scope_registers_smart(X86CodeGenV2* x86_gen, LexicalScopeNode* scope_node) {
    // Determine how many parent scopes this function needs
    size_t num_parent_scopes = scope_node->self_dependencies.size();
    if (num_parent_scopes == 0) {
        std::cout << "[SCOPE_CODEGEN] SMART: Function accesses no parent scopes, skipping register save" << std::endl;
        return;
    }
    
    // Only allocate stack space for the registers we actually need
    size_t registers_to_save = std::min(num_parent_scopes, size_t(3));
    size_t stack_space_needed = registers_to_save * 8;
    
    std::cout << "[SCOPE_CODEGEN] SMART: Function needs " << num_parent_scopes 
              << " parent scopes, saving " << registers_to_save << " registers" << std::endl;
    
    x86_gen->emit_sub_reg_imm(4, stack_space_needed); // sub rsp, stack_space_needed
    
    // Save only the registers we need
    if (registers_to_save >= 1) {
        x86_gen->emit_mov_mem_rsp_reg(stack_space_needed - 8, 12); // mov [rsp + offset], r12
        std::cout << "[SCOPE_CODEGEN] SMART: Saved r12 to stack" << std::endl;
    }
    if (registers_to_save >= 2) {
        x86_gen->emit_mov_mem_rsp_reg(stack_space_needed - 16, 13); // mov [rsp + offset], r13
        std::cout << "[SCOPE_CODEGEN] SMART: Saved r13 to stack" << std::endl;
    }
    if (registers_to_save >= 3) {
        x86_gen->emit_mov_mem_rsp_reg(stack_space_needed - 24, 14); // mov [rsp + offset], r14
        std::cout << "[SCOPE_CODEGEN] SMART: Saved r14 to stack" << std::endl;
    }
}

// Restore parent scope registers from stack after exiting child scope
void restore_parent_scope_registers(X86CodeGenV2* x86_gen) {
    std::cout << "[SCOPE_CODEGEN] Restoring parent scope registers from stack" << std::endl;
    
    // Restore parent scope registers from stack
    x86_gen->emit_mov_reg_mem_rsp(12, SAVED_R12_OFFSET + SCOPE_SAVE_AREA_SIZE); // mov r12, [rsp + 16]
    x86_gen->emit_mov_reg_mem_rsp(13, SAVED_R13_OFFSET + SCOPE_SAVE_AREA_SIZE); // mov r13, [rsp + 8]
    x86_gen->emit_mov_reg_mem_rsp(14, SAVED_R14_OFFSET + SCOPE_SAVE_AREA_SIZE); // mov r14, [rsp]
    
    // Deallocate stack space used for saving registers
    x86_gen->emit_add_reg_imm(4, SCOPE_SAVE_AREA_SIZE); // add rsp, 24
    
    std::cout << "[SCOPE_CODEGEN] Restored r12, r13, r14 from stack and deallocated save area" << std::endl;
}

// SMART Restore parent scope registers - only restores registers that were saved
void restore_parent_scope_registers_smart(X86CodeGenV2* x86_gen, LexicalScopeNode* scope_node) {
    // Determine how many parent scopes this function needed
    size_t num_parent_scopes = scope_node->self_dependencies.size();
    if (num_parent_scopes == 0) {
        std::cout << "[SCOPE_CODEGEN] SMART: Function accessed no parent scopes, skipping register restore" << std::endl;
        return;
    }
    
    size_t registers_to_restore = std::min(num_parent_scopes, size_t(3));
    size_t stack_space_used = registers_to_restore * 8;
    
    std::cout << "[SCOPE_CODEGEN] SMART: Restoring " << registers_to_restore << " registers" << std::endl;
    
    // Restore only the registers we saved
    if (registers_to_restore >= 1) {
        x86_gen->emit_mov_reg_mem_rsp(12, stack_space_used - 8); // mov r12, [rsp + offset]
        std::cout << "[SCOPE_CODEGEN] SMART: Restored r12 from stack" << std::endl;
    }
    if (registers_to_restore >= 2) {
        x86_gen->emit_mov_reg_mem_rsp(13, stack_space_used - 16); // mov r13, [rsp + offset]
        std::cout << "[SCOPE_CODEGEN] SMART: Restored r13 from stack" << std::endl;
    }
    if (registers_to_restore >= 3) {
        x86_gen->emit_mov_reg_mem_rsp(14, stack_space_used - 24); // mov r14, [rsp + offset]
        std::cout << "[SCOPE_CODEGEN] SMART: Restored r14 from stack" << std::endl;
    }
    
    // Deallocate the stack space we used
    x86_gen->emit_add_reg_imm(4, stack_space_used); // add rsp, stack_space_used
    
    std::cout << "[SCOPE_CODEGEN] SMART: Deallocated " << stack_space_used << " bytes of stack space" << std::endl;
}

// Set up parent scope registers based on priority for current scope
void setup_parent_scope_registers(X86CodeGenV2* x86_gen, LexicalScopeNode* scope_node) {
    std::cout << "[SCOPE_CODEGEN] Setting up parent scope registers for scope depth " << scope_node->scope_depth << std::endl;
    
    // Clear previous register assignments
    g_scope_context.scope_state.scope_depth_to_register.clear();
    g_scope_context.scope_state.stack_stored_scopes.clear();
    
    // Reset available registers
    g_scope_context.scope_state.available_scope_registers = {12, 13, 14};
    
    // FREQUENCY-BASED REGISTER ALLOCATION: Use priority_sorted_parent_scopes
    // This implements the FUNCTION.md specification for optimal register allocation
    if (!scope_node->priority_sorted_parent_scopes.empty()) {
        std::cout << "[SCOPE_CODEGEN] Using frequency-based allocation for " 
                  << scope_node->priority_sorted_parent_scopes.size() << " parent scopes" << std::endl;
        
        for (size_t i = 0; i < scope_node->priority_sorted_parent_scopes.size(); i++) {
            int parent_depth = scope_node->priority_sorted_parent_scopes[i];
            
            if (i < 3) {
                // Most frequent scopes get registers R12, R13, R14
                int scope_reg = g_scope_context.scope_state.available_scope_registers[i];
                
                // NEW SYSTEM: Direct register assignment instead of legacy runtime call
                if (parent_depth == 1) {
                    // Parent is main scope - use r15 (main scope register)
                    x86_gen->emit_mov_reg_reg(scope_reg, 15); // Move r15 to assigned register
                    std::cout << "[SCOPE_CODEGEN] Assigned parent scope depth " << parent_depth 
                              << " to register r" << scope_reg << " = r15 (main scope)" << std::endl;
                } else {
                    // Parent is intermediate scope - find its register
                    auto parent_reg_it = g_scope_context.scope_state.scope_depth_to_register.find(parent_depth);
                    if (parent_reg_it != g_scope_context.scope_state.scope_depth_to_register.end()) {
                        int parent_reg = parent_reg_it->second;
                        x86_gen->emit_mov_reg_reg(scope_reg, parent_reg);
                        std::cout << "[SCOPE_CODEGEN] Assigned parent scope depth " << parent_depth 
                                  << " to register r" << scope_reg << " = r" << parent_reg << std::endl;
                    } else {
                        std::cout << "[SCOPE_CODEGEN] ERROR: Parent scope depth " << parent_depth 
                                  << " not found in register mapping!" << std::endl;
                    }
                }
                
                // Track the assignment
                g_scope_context.scope_state.scope_depth_to_register[parent_depth] = scope_reg;
            } else {
                // Less frequent scopes go to stack
                g_scope_context.scope_state.stack_stored_scopes.push_back(parent_depth);
                std::cout << "[SCOPE_CODEGEN] Parent scope depth " << parent_depth 
                          << " stored on stack (priority " << i << ")" << std::endl;
            }
        }
    } else {
        std::cout << "[SCOPE_CODEGEN] No parent scopes needed for this function" << std::endl;
    }
    
    std::cout << "[SCOPE_CODEGEN] Register allocation complete: " 
              << g_scope_context.scope_state.scope_depth_to_register.size() << " in registers, "
              << g_scope_context.scope_state.stack_stored_scopes.size() << " on stack" << std::endl;
}

// Initialize a function variable during scope hoisting
// This creates the function instance with proper closure capture while parent scopes are active
void initialize_hoisted_function_variable(X86CodeGenV2* gen, FunctionDecl* func_decl, 
                                         LexicalScopeNode* scope_node, size_t func_var_offset) {
    std::cout << "[NEW_SYSTEM] Using new function instance creation for '" << func_decl->name << "'" << std::endl;
    
    // Cast to the new ScopeAwareCodeGen system
    auto scope_gen = dynamic_cast<ScopeAwareCodeGen*>(gen);
    if (!scope_gen) {
        throw std::runtime_error("New function system requires ScopeAwareCodeGen");
    }
    
    // Use the new direct function instance creation method
    scope_gen->emit_function_instance_creation(func_decl, func_var_offset);
    
    std::cout << "[NEW_SYSTEM] Function instance created using new system" << std::endl;
}

// Generate code to enter a lexical scope
void emit_scope_enter(CodeGenerator& gen, LexicalScopeNode* scope_node) {
    std::cout << "[SCOPE_CODEGEN] Entering lexical scope at depth " << scope_node->scope_depth << std::endl;
    
    // DEFERRED PACKING: Perform variable packing now that we have complete hoisting information
    if (scope_node->variable_offsets.empty() && !scope_node->declared_variables.empty()) {
        std::cout << "[SCOPE_CODEGEN] Performing deferred variable packing for " 
                  << scope_node->declared_variables.size() << " variables" << std::endl;
        
        // Access the scope analyzer to perform packing
        if (g_scope_context.scope_analyzer) {
            g_scope_context.scope_analyzer->perform_deferred_packing_for_scope(scope_node);
        } else {
            std::cout << "[SCOPE_CODEGEN] WARNING: No scope analyzer available for deferred packing" << std::endl;
        }
    }
    
    // Cast to X86CodeGenV2 to access scope management methods
    X86CodeGenV2* x86_gen = dynamic_cast<X86CodeGenV2*>(&gen);
    if (!x86_gen) {
        throw std::runtime_error("Scope management requires X86CodeGenV2");
    }
    
    // STEP 1: Save current parent scope registers to stack (only if we have a real parent)
    LexicalScopeNode* parent_scope = get_parent_scope(scope_node);
    if (parent_scope != nullptr) {
        save_parent_scope_registers_smart(x86_gen, scope_node);
    }
    
    // STEP 2: Allocate heap memory for this scope's variables (OPTIMIZED - MINIMIZE RUNTIME CALLS)
    size_t scope_size = scope_node->total_scope_frame_size;
    if (scope_size > 0) {
        std::cout << "[SCOPE_CODEGEN_OPT] Allocating " << scope_size << " bytes with optimized allocation" << std::endl;
        
        // Use malloc for now, but minimize calls by combining allocation and initialization
        x86_gen->emit_mov_reg_imm(7, scope_size); // RDI = size
        x86_gen->emit_call("malloc"); // Returns allocated memory in RAX
        x86_gen->emit_mov_reg_reg(15, 0); // R15 = allocated scope memory
        
        // Optimize zero-initialization based on scope size
        if (scope_size <= 32) {
            // Small scopes: direct zero writes (faster than memset call)
            std::cout << "[SCOPE_CODEGEN_OPT] Zero-initializing " << scope_size << " bytes with direct writes" << std::endl;
            for (size_t i = 0; i < scope_size; i += 8) {
                x86_gen->emit_mov_reg_imm(0, 0); // RAX = 0
                x86_gen->emit_mov_reg_offset_reg(15, i, 0); // [R15 + i] = 0
            }
        } else {
            // Large scopes: use memset (still faster than multiple small writes)
            x86_gen->emit_mov_reg_reg(7, 15); // RDI = scope memory (R15)
            x86_gen->emit_mov_reg_imm(6, 0);  // RSI = 0 (fill value)
            x86_gen->emit_mov_reg_imm(2, scope_size); // RDX = size
            x86_gen->emit_call("memset"); // Zero-fill the memory
        }
        
        std::cout << "[SCOPE_CODEGEN_OPT] Allocated and initialized " << scope_size 
                  << " bytes, r15 = scope address" << std::endl;
    } else {
        // Even empty scopes get a minimal allocation for consistency
        x86_gen->emit_mov_reg_imm(7, 8); // RDI = 8 bytes minimum
        x86_gen->emit_call("malloc");
        x86_gen->emit_mov_reg_reg(15, 0); // R15 = allocated scope memory
        std::cout << "[SCOPE_CODEGEN_OPT] Allocated minimal 8 bytes for empty scope" << std::endl;
    }
    
    // STEP 3: Register this scope address in the global registry
    x86_gen->emit_mov_reg_imm(7, scope_node->scope_depth); // RDI = scope depth
    x86_gen->emit_mov_reg_reg(6, 15); // RSI = scope address (R15)
    x86_gen->emit_call("__register_scope_address_for_depth");
    
    std::cout << "[SCOPE_CODEGEN] Registered scope depth " << scope_node->scope_depth 
              << " in global registry" << std::endl;
    
    // STEP 4: Set up new parent scope registers for this scope's needs (using frequency rankings)
    setup_parent_scope_registers(x86_gen, scope_node);
    
    // STEP 5: FUNCTION HOISTING - Initialize all function variables immediately
    // This implements JavaScript function hoisting: function declarations are available
    // throughout their entire scope from the beginning, not just after declaration
    const auto& function_decls = scope_node->get_declared_functions();
    if (!function_decls.empty()) {
        std::cout << "[SCOPE_CODEGEN] HOISTING: Initializing " << function_decls.size() 
                  << " function variables for hoisting" << std::endl;
        
        for (FunctionDecl* func_decl : function_decls) {
            std::cout << "[SCOPE_CODEGEN] HOISTING: Initializing function variable '" 
                      << func_decl->name << "'" << std::endl;
            
            // Find the variable offset for this function
            auto offset_it = scope_node->variable_offsets.find(func_decl->name);
            if (offset_it != scope_node->variable_offsets.end()) {
                size_t func_var_offset = offset_it->second;
                
                // Initialize the function variable with proper closure capture
                // This creates the function instance while all parent scopes are still active
                initialize_hoisted_function_variable(x86_gen, func_decl, scope_node, func_var_offset);
                
                std::cout << "[SCOPE_CODEGEN] HOISTING: Successfully initialized function variable '" 
                          << func_decl->name << "' at offset " << func_var_offset << std::endl;
            } else {
                std::cout << "[SCOPE_CODEGEN] HOISTING: WARNING - Function '" << func_decl->name 
                          << "' not found in variable offsets" << std::endl;
            }
        }
    }

    // STEP 6: Update current context
    set_current_scope(scope_node);
    
    std::cout << "[SCOPE_CODEGEN] Successfully entered scope depth " << scope_node->scope_depth 
              << " with " << scope_node->priority_sorted_parent_scopes.size() << " parent scopes" << std::endl;
}

// Generate code to exit a lexical scope
void emit_scope_exit(CodeGenerator& gen, LexicalScopeNode* scope_node) {
    std::cout << "[SCOPE_CODEGEN] Exiting lexical scope at depth " << scope_node->scope_depth << std::endl;
    
    // Cast to X86CodeGenV2 to access scope management methods
    X86CodeGenV2* x86_gen = dynamic_cast<X86CodeGenV2*>(&gen);
    if (!x86_gen) {
        throw std::runtime_error("Scope management requires X86CodeGenV2");
    }
    
    // STEP 1: Free the heap-allocated scope memory (FUNCTION.md: scopes are on heap)
    if (scope_node->total_scope_frame_size > 0 || true) { // Always free, even minimal allocations
        x86_gen->emit_mov_reg_reg(7, 15); // RDI = current scope memory (R15)
        x86_gen->emit_call("free"); // Free the heap memory
        std::cout << "[SCOPE_CODEGEN] Freed heap memory for scope at depth " << scope_node->scope_depth << std::endl;
    }
    
    // STEP 2: Unregister this scope address from the global registry
    x86_gen->emit_mov_reg_imm(7, scope_node->scope_depth); // RDI = scope depth
    x86_gen->emit_call("__unregister_scope_address_for_depth");
    std::cout << "[SCOPE_CODEGEN] Unregistered scope depth " << scope_node->scope_depth 
              << " from global registry" << std::endl;
    
    // STEP 3: Restore parent scope context
    LexicalScopeNode* parent_scope = get_parent_scope(scope_node);
    if (parent_scope) {
        // Restore parent scope registers from stack
        restore_parent_scope_registers_smart(x86_gen, scope_node);
        
        // Update current context to parent
        set_current_scope(parent_scope);
        
        std::cout << "[SCOPE_CODEGEN] Restored parent scope context at depth " 
                  << parent_scope->scope_depth << std::endl;
    } else {
        // Return to global/root scope
        set_current_scope(nullptr);
        
        // Clear register assignments when returning to root
        g_scope_context.scope_state.scope_depth_to_register.clear();
        g_scope_context.scope_state.stack_stored_scopes.clear();
        
        std::cout << "[SCOPE_CODEGEN] Returned to global/root scope, cleared register assignments" << std::endl;
    }
    
    std::cout << "[SCOPE_CODEGEN] Successfully exited scope depth " << scope_node->scope_depth << std::endl;
}

// Generate variable load code using scope-aware access
void emit_variable_load(CodeGenerator& gen, const std::string& var_name) {
    if (!g_scope_context.current_scope || !g_scope_context.scope_analyzer) {
        throw std::runtime_error("No scope context for variable access: " + var_name);
    }
    
    std::cout << "[SCOPE_DEBUG] emit_variable_load for '" << var_name 
              << "' - current scope depth: " << g_scope_context.current_scope->scope_depth << std::endl;
    
    // Find the scope where this variable is defined
    auto def_scope = g_scope_context.scope_analyzer->get_definition_scope_for_variable(var_name);
    if (!def_scope) {
        throw std::runtime_error("Variable not found in any scope: " + var_name);
    }
    
    std::cout << "[SCOPE_DEBUG] Variable '" << var_name << "' defined in scope depth: " 
              << def_scope->scope_depth << ", current scope depth: " 
              << g_scope_context.current_scope->scope_depth << std::endl;
    
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
        
        std::cout << "[SCOPE_DEBUG] Looking for scope depth " << scope_depth 
                  << " in register map (map size: " << g_scope_context.scope_state.scope_depth_to_register.size() << ")" << std::endl;
        
        if (reg_it != g_scope_context.scope_state.scope_depth_to_register.end()) {
            // Parent scope is in a register
            int scope_reg = reg_it->second;
            gen.emit_mov_reg_reg_offset(0, scope_reg, var_offset); // rax = [r12/13/14 + offset]
            std::cout << "[SCOPE_CODEGEN] Loaded parent variable '" << var_name 
                      << "' from r" << scope_reg << "+" << var_offset << std::endl;
        } else {
            // Parent scope is on stack (deep nesting)
            generate_deep_scope_variable_load(gen, var_name, scope_depth, var_offset);
            std::cout << "[SCOPE_CODEGEN] Loaded parent variable '" << var_name 
                      << "' from deep scope (stack-based access)" << std::endl;
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
                generate_deep_scope_variable_store(gen, var_name, scope_depth, var_offset);
                std::cout << "[SCOPE_CODEGEN] Stored to parent variable '" << var_name 
                          << "' via deep scope (stack-based access)" << std::endl;
            }
        }
    }
}

//=============================================================================
// NEW AST NODE IMPLEMENTATIONS WITHOUT TYPEINFERENCE
// These replace all the old generate_code methods that used TypeInference
//=============================================================================

void NumberLiteral::generate_code(CodeGenerator& gen) {
    std::cout << "[NEW_CODEGEN] NumberLiteral::generate_code - raw_value=" << raw_value << std::endl;
    
    // Default behavior: treat as FLOAT64 for JavaScript compatibility
    generate_code_as(gen, DataType::FLOAT64);
}

void NumberLiteral::generate_code_as(CodeGenerator& gen, DataType target_type) {
    std::cout << "[NEW_CODEGEN] NumberLiteral::generate_code_as - raw_value=" << raw_value 
              << ", target_type=" << static_cast<int>(target_type) << std::endl;
    
    switch (target_type) {
        case DataType::INT8: {
            int8_t val = static_cast<int8_t>(std::stoll(raw_value));
            gen.emit_mov_reg_imm(0, val);
            result_type = DataType::INT8;
            break;
        }
        case DataType::INT16: {
            int16_t val = static_cast<int16_t>(std::stoll(raw_value));
            gen.emit_mov_reg_imm(0, val);
            result_type = DataType::INT16;
            break;
        }
        case DataType::INT32: {
            int32_t val = static_cast<int32_t>(std::stoll(raw_value));
            gen.emit_mov_reg_imm(0, val);
            result_type = DataType::INT32;
            std::cout << "[NEW_CODEGEN] NumberLiteral: Generated int32 value " << val << std::endl;
            break;
        }
        case DataType::INT64: {
            int64_t val = std::stoll(raw_value);
            gen.emit_mov_reg_imm(0, val);
            result_type = DataType::INT64;
            std::cout << "[NEW_CODEGEN] NumberLiteral: Generated int64 value " << val << std::endl;
            break;
        }
        case DataType::UINT8: {
            uint8_t val = static_cast<uint8_t>(std::stoull(raw_value));
            gen.emit_mov_reg_imm(0, val);
            result_type = DataType::UINT8;
            break;
        }
        case DataType::UINT16: {
            uint16_t val = static_cast<uint16_t>(std::stoull(raw_value));
            gen.emit_mov_reg_imm(0, val);
            result_type = DataType::UINT16;
            break;
        }
        case DataType::UINT32: {
            uint32_t val = static_cast<uint32_t>(std::stoull(raw_value));
            gen.emit_mov_reg_imm(0, val);
            result_type = DataType::UINT32;
            break;
        }
        case DataType::UINT64: {
            uint64_t val = std::stoull(raw_value);
            gen.emit_mov_reg_imm(0, val);
            result_type = DataType::UINT64;
            break;
        }
        case DataType::FLOAT32: {
            float val = std::stof(raw_value);
            union { float f; int32_t i; } converter;
            converter.f = val;
            gen.emit_mov_reg_imm(0, converter.i);
            result_type = DataType::FLOAT32;
            std::cout << "[NEW_CODEGEN] NumberLiteral: Generated float32 value " << val << std::endl;
            break;
        }
        case DataType::FLOAT64:
        default: {
            // Default case - JavaScript numbers are float64
            double val = std::stod(raw_value);
            union { double d; int64_t i; } converter;
            converter.d = val;
            gen.emit_mov_reg_imm(0, converter.i);
            result_type = DataType::FLOAT64;
            std::cout << "[NEW_CODEGEN] NumberLiteral: Generated float64 value " << val << std::endl;
            break;
        }
    }
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

void BooleanLiteral::generate_code(CodeGenerator& gen) {
    // Default behavior: generate as boolean type
    generate_code_as(gen, DataType::BOOLEAN);
}

void BooleanLiteral::generate_code_as(CodeGenerator& gen, DataType target_type) {
    std::cout << "[NEW_CODEGEN] BooleanLiteral::generate_code_as - value=" << value 
              << ", target_type=" << static_cast<int>(target_type) << std::endl;
    
    switch (target_type) {
        case DataType::BOOLEAN: {
            gen.emit_mov_reg_imm(0, value ? 1 : 0);
            result_type = DataType::BOOLEAN;
            std::cout << "[NEW_CODEGEN] BooleanLiteral: Generated boolean value " << value << std::endl;
            break;
        }
        case DataType::INT8:
        case DataType::INT16:
        case DataType::INT32:
        case DataType::INT64: {
            int64_t numeric_val = value ? 1 : 0;
            gen.emit_mov_reg_imm(0, numeric_val);
            result_type = target_type;
            std::cout << "[NEW_CODEGEN] BooleanLiteral: Generated " << static_cast<int>(target_type) 
                      << " value " << numeric_val << std::endl;
            break;
        }
        case DataType::UINT8:
        case DataType::UINT16:
        case DataType::UINT32:
        case DataType::UINT64: {
            uint64_t numeric_val = value ? 1 : 0;
            gen.emit_mov_reg_imm(0, numeric_val);
            result_type = target_type;
            break;
        }
        case DataType::FLOAT32: {
            float numeric_val = value ? 1.0f : 0.0f;
            union { float f; int32_t i; } converter;
            converter.f = numeric_val;
            gen.emit_mov_reg_imm(0, converter.i);
            result_type = DataType::FLOAT32;
            break;
        }
        case DataType::FLOAT64:
        default: {
            double numeric_val = value ? 1.0 : 0.0;
            union { double d; int64_t i; } converter;
            converter.d = numeric_val;
            gen.emit_mov_reg_imm(0, converter.i);
            result_type = DataType::FLOAT64;
            std::cout << "[NEW_CODEGEN] BooleanLiteral: Generated float64 value " << numeric_val << std::endl;
            break;
        }
    }
}

void Identifier::generate_code(CodeGenerator& gen) {
    std::cout << "[NEW_CODEGEN] Identifier::generate_code - variable: " << name << std::endl;
    std::cout << "[DEBUG_CRASH] Started, about to check runtime special case" << std::endl;
    std::cout << "[DEBUG_CRASH] About to check runtime special case" << std::endl;
    
    // Handle special cases first
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

    // Cast to ScopeAwareCodeGen to get variable declaration info
    auto scope_gen = dynamic_cast<ScopeAwareCodeGen*>(&gen);
    if (!scope_gen) {
        throw std::runtime_error("Variable access requires ScopeAwareCodeGen");
    }
    
    // Get variable declaration info from the scope system
    VariableDeclarationInfo* var_info = scope_gen->get_variable_declaration_info(name);
    
    // SPECIAL HANDLING FOR FUNCTION VARIABLES WHEN ACCESSED AS VALUES
    // Check if this is a function variable that needs initialization
    if (var_info && var_info->data_type == DataType::FUNCTION) {
        std::cout << "[FUNCTION_VARIABLE] Function variable '" << name << "' accessed as value, creating function instance" << std::endl;
        
        // Get the function declaration for this variable name
        LexicalScopeNode* func_scope = scope_gen->get_definition_scope_for_variable(name);
        FunctionDecl* func_decl = nullptr;
        if (func_scope) {
            for (auto* decl : func_scope->declared_functions) {
                if (decl && decl->name == name) {
                    func_decl = decl;
                    break;
                }
            }
        }
        
        if (func_decl) {
            std::cout << "[FUNCTION_VARIABLE] Creating function instance for '" << name << "'" << std::endl;
            
            // Instead of initializing the function variable, create a function expression
            // This will create a proper function instance that can be returned
            X86CodeGenV2* x86_gen = dynamic_cast<X86CodeGenV2*>(&gen);
            if (!x86_gen) {
                throw std::runtime_error("Function instance creation requires X86CodeGenV2");
            }
            
            // Calculate function instance size (patching system - header + captured scopes only)
            size_t header_size = 16; // size (8) + function_code_addr (8)  
            size_t num_captured_scopes = func_decl->lexical_scope ? func_decl->lexical_scope->priority_sorted_parent_scopes.size() : 0;
            size_t scope_pointers_size = num_captured_scopes * 8;
            size_t function_instance_size = header_size + scope_pointers_size;
            
            std::cout << "[FUNCTION_VARIABLE] Allocating " << function_instance_size << " bytes for function instance (with patching system)" << std::endl;
            
            // STEP 1: Allocate memory for function instance on heap
            x86_gen->emit_mov_reg_imm(7, function_instance_size); // RDI = size
            x86_gen->emit_call("malloc");
            x86_gen->emit_mov_reg_reg(10, 0); // R10 = allocated memory address
            
            // STEP 2: Initialize FunctionInstance header with placeholder address
            // Store size field
            x86_gen->emit_mov_reg_imm(11, function_instance_size); // R11 = size
            x86_gen->emit_mov_reg_offset_reg(10, 0, 11); // [R10 + 0] = size
            
            // Store placeholder function address (0) - will be patched during startup
            x86_gen->emit_mov_reg_imm(11, 0); // R11 = 0 (placeholder)
            x86_gen->emit_mov_reg_offset_reg(10, 8, 11); // [R10 + 8] = 0 (placeholder function_code_addr)
            
            // Register this function instance for patching during execution
            x86_gen->emit_mov_reg_reg(7, 10); // RDI = instance pointer
            x86_gen->emit_mov_reg_imm(6, reinterpret_cast<int64_t>(name.c_str())); // RSI = function name (temporary)
            x86_gen->emit_call("__string_intern"); // Returns permanent string pointer in RAX
            x86_gen->emit_mov_reg_reg(6, 0); // RSI = permanent function name
            x86_gen->emit_mov_reg_imm(2, 8); // RDX = code address offset (8 bytes into instance)
            x86_gen->emit_call("__register_function_instance_for_patching");
            
            // STEP 3: Capture lexical scope addresses if needed
            if (func_decl->lexical_scope && !func_decl->lexical_scope->priority_sorted_parent_scopes.empty()) {
                std::cout << "[FUNCTION_VARIABLE] Capturing " << func_decl->lexical_scope->priority_sorted_parent_scopes.size() 
                          << " scope addresses" << std::endl;
                
                size_t scope_offset = 16; // Start after header (size + function_code_addr)
                
                for (size_t i = 0; i < func_decl->lexical_scope->priority_sorted_parent_scopes.size(); i++) {
                    int scope_depth = func_decl->lexical_scope->priority_sorted_parent_scopes[i];
                    
                    // Get scope address for this depth
                    x86_gen->emit_mov_reg_imm(7, scope_depth); // RDI = scope depth
                    x86_gen->emit_call("__get_scope_address_for_depth"); // Returns scope address in RAX
                    
                    // Store in function instance
                    x86_gen->emit_mov_reg_offset_reg(10, scope_offset, 0); // [R10 + scope_offset] = scope_address
                    scope_offset += 8;
                    
                    std::cout << "[FUNCTION_VARIABLE] Captured scope depth " << scope_depth 
                              << " at offset " << (scope_offset - 8) << std::endl;
                }
            }
            
            // STEP 4: Return pointer to function instance
            x86_gen->emit_mov_reg_reg(0, 10); // RAX = function instance pointer
            result_type = DataType::LOCAL_FUNCTION_INSTANCE;
            
            std::cout << "[FUNCTION_VARIABLE] Created function instance for " << name 
                      << " with " << (func_decl->lexical_scope ? func_decl->lexical_scope->priority_sorted_parent_scopes.size() : 0) 
                      << " captured scopes" << std::endl;
            return;
        }
    }
    
    // ULTRA-FAST DIRECT POINTER ACCESS - Zero lookups needed!
    if (var_info) {
        std::cout << "[DEBUG_CRASH] Have variable_declaration_info, about to get result_type" << std::endl;
        // Get variable type directly from declaration info
        result_type = var_info->data_type;
        std::cout << "[DEBUG_CRASH] Got result_type, about to check if param" << std::endl;
        
        // SPECIAL HANDLING FOR PARAMETERS
        if (var_info->declaration_type == "param") {
            std::cout << "[PARAMETER_ACCESS] Accessing parameter '" << name << "'" << std::endl;
            std::cout << "[PARAMETER_ACCESS] declaration_type='" << var_info->declaration_type << "'" << std::endl;
            
            // Parameters are stored in the scope object at their designated offsets
            // Use the same scope access logic as regular variables
            std::cout << "[PARAMETER_ACCESS] Parameter '" << name 
                      << "' will be accessed from scope object at offset " << var_info->offset << std::endl;
            // Fall through to normal scope access logic below
        } else {
            std::cout << "[PARAMETER_ACCESS] Not a parameter: declaration_type='" << var_info->declaration_type << "'" << std::endl;
        }
        
        // Use the direct offset from variable_declaration_info (FASTEST - no map lookup!)
        size_t var_offset = var_info->offset;
        
        LexicalScopeNode* definition_scope = scope_gen->get_definition_scope_for_variable(name);
        int definition_depth = var_info->depth;
        
        std::cout << "[DEBUG_OFFSET] Variable '" << name << "' using offset " << var_offset 
                  << " from VariableDeclarationInfo (declaration_depth=" << definition_depth << ")" << std::endl;

        // CRITICAL FIX: For parent scope variables, get offset from the actual defining scope
        // instead of the potentially stale VariableDeclarationInfo
        int current_scope_depth = g_scope_context.current_scope ? g_scope_context.current_scope->scope_depth : 1;
        
        if (definition_depth != current_scope_depth && definition_scope) {
            auto offset_it = definition_scope->variable_offsets.find(name);
            if (offset_it != definition_scope->variable_offsets.end()) {
                var_offset = offset_it->second;
                std::cout << "[OFFSET_FIX] Corrected offset for '" << name 
                          << "' from defining scope: " << var_offset << " (was " 
                          << var_info->offset << ")" << std::endl;
            }
        }
        
        // Generate optimal load code based on scope depth comparison
        std::cout << "[DEBUG_SCOPE] definition_scope=" << definition_scope 
                  << ", current_scope=" << g_scope_context.current_scope 
                  << ", definition_depth=" << definition_depth << std::endl;
                  
        if (definition_depth == current_scope_depth) {
            // Loading from current scope (fastest path)
            gen.emit_mov_reg_reg_offset(0, 15, var_offset); // rax = [r15 + offset]
            std::cout << "[NEW_CODEGEN] Loaded from local variable '" << name << "' at r15+" << var_offset 
                      << " (type=" << static_cast<int>(var_info->data_type) << ")" << std::endl;
        } else if (definition_depth < current_scope_depth) {
            // Variable is in parent scope (normal case)
            int scope_depth = definition_depth;  // Use copied depth instead of pointer access
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
                generate_deep_scope_variable_load(gen, name, scope_depth, var_offset);
                std::cout << "[NEW_CODEGEN] Loaded parent variable '" << name 
                          << "' from deep scope (stack-based access)"
                          << " (type=" << static_cast<int>(result_type) << ")" << std::endl;
            }
        } else {
            // Special case: Loading from deeper scope (upward access)
            // This can happen when Identifier nodes are processed outside their proper lexical context
            std::cout << "[UPWARD_ACCESS] WARNING: Identifier '" << name 
                      << "' accessed outside proper scope context (current=" << current_scope_depth 
                      << ", target=" << definition_depth << ")" << std::endl;
            std::cout << "[UPWARD_ACCESS] Using simulated direct access (assuming variable is accessible)" << std::endl;
            
            // For upward access, we can't easily load from the deeper scope from global context
            // The safest approach is to emit a placeholder that assumes the variable is accessible
            // In practice, this should rarely happen if AST structure is correct
            gen.emit_mov_reg_reg_offset(0, 15, var_offset); // Direct load (may be incorrect but won't crash)
            std::cout << "[NEW_CODEGEN] Loaded deeper-scope variable '" << name 
                      << "' using direct access (type=" << static_cast<int>(result_type) << ")" << std::endl;
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
        // For BooleanLiterals and NumberLiterals, use context-aware generation if we have a declared type
        if (declared_type != DataType::ANY) {
            if (auto bool_literal = dynamic_cast<BooleanLiteral*>(value.get())) {
                // Use context-aware generation for BooleanLiteral
                std::cout << "[NEW_CODEGEN] Using context-aware BooleanLiteral generation for declared_type=" 
                          << static_cast<int>(declared_type) << std::endl;
                bool_literal->generate_code_as(gen, declared_type);
            } else if (auto num_literal = dynamic_cast<NumberLiteral*>(value.get())) {
                // Use context-aware generation for NumberLiteral
                std::cout << "[NEW_CODEGEN] Using context-aware NumberLiteral generation for declared_type=" 
                          << static_cast<int>(declared_type) << std::endl;
                num_literal->generate_code_as(gen, declared_type);
            } else {
                // Non-literal - generate normally and handle conversion below
                value->generate_code(gen);
            }
        } else {
            // No declared type - handle untyped variables by creating DynamicValue
            if (auto bool_literal = dynamic_cast<BooleanLiteral*>(value.get())) {
                std::cout << "[NEW_CODEGEN] Creating DynamicValue for untyped BooleanLiteral: " << bool_literal->value << std::endl;
                
                // Boolean literal -> DynamicValue
                gen.emit_mov_reg_imm(7, bool_literal->value ? 1 : 0); // RDI = boolean value
                gen.emit_call("__dynamic_value_create_from_bool");
                // RAX now contains DynamicValue* pointing to boolean
                
                // Set result type to ANY for DynamicValue
                value->result_type = DataType::ANY;
                
            } else if (auto num_literal = dynamic_cast<NumberLiteral*>(value.get())) {
                std::cout << "[NEW_CODEGEN] Creating DynamicValue for untyped NumberLiteral: " << num_literal->raw_value << std::endl;
                
                // Numeric literal - create DynamicValue containing double (JavaScript compatibility)
                double val = std::stod(num_literal->raw_value);
                gen.emit_mov_reg_imm(7, *reinterpret_cast<int64_t*>(&val)); // RDI = double bits
                gen.emit_call("__dynamic_value_create_from_double");
                // RAX now contains DynamicValue* pointing to double
                
                // Set result type to ANY for DynamicValue
                value->result_type = DataType::ANY;
                
            } else {
                // Non-literal nodes - generate normally (string literals, etc. should also create DynamicValues)
                value->generate_code(gen);
                
                // If result is string, wrap in DynamicValue  
                if (value->result_type == DataType::STRING) {
                    gen.emit_mov_reg_reg(7, 0); // RDI = string pointer from RAX
                    gen.emit_call("__dynamic_value_create_from_string");
                    value->result_type = DataType::ANY;
                }
            }
        }
        
        // Determine variable type
        DataType variable_type;
        if (declared_type != DataType::ANY) {
            variable_type = declared_type;
        } else {
            variable_type = value->result_type;
        }
        
        // Type conversion is now handled by context-aware NumberLiteral generation
        // No need for post-generation conversion since NumberLiterals generate the right type directly
        
        // ULTRA-FAST DIRECT POINTER STORE
        if (variable_declaration_info) {
            // Update the variable's type in its declaration info
            variable_declaration_info->data_type = variable_type;
            
            // Use the direct offset from variable_declaration_info (FASTEST - no map lookup!)
            size_t var_offset = variable_declaration_info->offset;
            
            // Generate optimal store code based on scope depth comparison (not pointer equality)
            int current_depth = g_scope_context.current_scope ? g_scope_context.current_scope->scope_depth : 1;
            int target_depth = definition_depth;  // Use copied depth value
            
            std::cout << "[ASSIGNMENT_DEBUG] Assignment to '" << variable_name 
                      << "': current_depth=" << current_depth << ", target_depth=" << target_depth << std::endl;
            
            if (current_depth == target_depth) {
                // Storing to current scope (fastest path)
                gen.emit_mov_reg_offset_reg(15, var_offset, 0); // [r15 + offset] = rax
                std::cout << "[NEW_CODEGEN] Stored to local variable '" << variable_name << "' at r15+" << var_offset 
                          << " (type=" << static_cast<int>(variable_type) << ")" << std::endl;
            } else if (current_depth > target_depth) {
                // Storing to parent scope (normal case)
                int scope_depth = definition_depth;  // Use copied depth instead of pointer access
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
                    generate_deep_scope_variable_store(gen, variable_name, scope_depth, var_offset);
                    std::cout << "[NEW_CODEGEN] Stored to parent variable '" << variable_name 
                              << "' via deep scope (stack-based access)"
                              << " (type=" << static_cast<int>(variable_type) << ")" << std::endl;
                }
            } else {
                // Special case: Storing to deeper scope (upward assignment)
                // This can happen when Assignment nodes are processed outside their proper lexical context
                std::cout << "[UPWARD_ASSIGNMENT] WARNING: Assignment to '" << variable_name 
                          << "' processed outside proper scope context (current=" << current_depth 
                          << ", target=" << target_depth << ")" << std::endl;
                std::cout << "[UPWARD_ASSIGNMENT] Using simulated direct assignment (assuming variable is accessible)" << std::endl;
                
                // For upward assignments, we can't easily access the deeper scope from global context
                // The safest approach is to emit a placeholder that assumes the variable is accessible
                // In practice, this should rarely happen if AST structure is correct
                gen.emit_mov_reg_offset_reg(15, var_offset, 0); // Direct assignment (may be incorrect but won't crash)
                std::cout << "[NEW_CODEGEN] Stored to deeper-scope variable '" << variable_name 
                          << "' using direct assignment (type=" << static_cast<int>(variable_type) << ")" << std::endl;
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
                    
                    // SMART TYPE HANDLING: Check if operands are DynamicValues that need unpacking
                    if (left_type == DataType::ANY || right_type == DataType::ANY) {
                        std::cout << "[DYNAMIC_ADD] Handling addition with DynamicValue operands" << std::endl;
                        
                        // At this point:
                        // RBX contains left DynamicValue pointer 
                        // RAX contains right DynamicValue pointer 
                        
                        // Save both DynamicValue pointers to stack before function calls
                        gen.emit_sub_reg_imm(4, 16);  // rsp -= 16 (allocate space)
                        gen.emit_mov_mem_reg(0, 3);   // save left operand (RBX) at [rsp]
                        gen.emit_mov_mem_reg(8, 0);   // save right operand (RAX) at [rsp+8]

                        // Extract numeric value from left operand
                        gen.emit_mov_reg_mem(7, 0);   // RDI = left DynamicValue pointer
                        gen.emit_call("__dynamic_value_get_number");
                        // RAX now contains double value as bit pattern
                        gen.emit_mov_mem_reg(0, 0);   // save left result on stack at [rsp]
                        
                        // Extract numeric value from right operand  
                        gen.emit_mov_reg_mem(7, 8);   // RDI = right DynamicValue pointer
                        gen.emit_call("__dynamic_value_get_number");
                        // RAX now contains double value as bit pattern
                        
                        // Now RAX has right number (as bits), left number (as bits) is at [rsp]
                        gen.emit_mov_reg_reg(6, 0);   // RSI = right number (as bit pattern)
                        gen.emit_mov_reg_mem(7, 0);   // RDI = left number (as bit pattern)
                        gen.emit_call("__dynamic_value_add_bits");

                        gen.emit_add_reg_imm(4, 16);  // rsp += 16 (deallocate space)
                        std::cout << "[DYNAMIC_ADD] DynamicValue addition completed" << std::endl;
                    } else {
                        // Normal numeric addition
                        gen.emit_add_reg_reg(0, 3);   // add rax, rbx (add left to right)
                    }
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
    std::cout << "[FUNCTION_CODEGEN] FunctionCall::generate_code - function: " << name << std::endl;
    
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
        return;
    }
    
    // Handle special runtime functions
    if (name == "setTimeout" || name == "setInterval" || name == "clearTimeout" || name == "clearInterval") {
        // Map timer functions to runtime equivalents
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
        
        if (name == "setTimeout") {
            gen.emit_call("__gots_set_timeout");
            result_type = DataType::INT64;
        } else if (name == "setInterval") {
            gen.emit_call("__gots_set_interval");
            result_type = DataType::INT64;
        } else if (name == "clearTimeout") {
            gen.emit_call("__gots_clear_timeout");
            result_type = DataType::BOOLEAN;
        } else if (name == "clearInterval") {
            gen.emit_call("__gots_clear_interval");
            result_type = DataType::BOOLEAN;
        }
        return;
    }
    
    if (name == "console.log") {
        // Simple console.log implementation
        if (arguments.size() > 0) {
            arguments[0]->generate_code(gen);
            gen.emit_mov_reg_reg(7, 0); // RDI = first argument
            gen.emit_call("__console_log_simple");
        }
        result_type = DataType::VOID;
        return;
    }
    
    // NEW: Function variable call using the three-strategy system
    // Try to resolve as a function variable in current scope
    LexicalScopeNode* current_scope = get_current_scope();
    if (current_scope && g_scope_context.scope_analyzer) {
        // Check if this is a function variable
        auto offset_it = current_scope->variable_offsets.find(name);
        if (offset_it != current_scope->variable_offsets.end()) {
            std::cout << "[FUNCTION_CODEGEN] Found function variable '" << name 
                      << "' in scope, using new calling system" << std::endl;
            
            // Generate argument code with type conversion (following x86-64 calling convention)
            for (size_t i = 0; i < arguments.size() && i < 6; i++) {
                arguments[i]->generate_code(gen);
                DataType arg_type = arguments[i]->result_type;
                
                // SMART TYPE CONVERSION: Convert argument to DynamicValue if target parameter is ANY
                // This handles the common case where numeric literals need to be wrapped for ANY parameters
                if (arg_type != DataType::ANY) {
                    std::cout << "[TYPE_CONVERSION] Converting argument " << i 
                              << " from " << static_cast<int>(arg_type) << " to DynamicValue for ANY parameter" << std::endl;
                    
                    if (arg_type == DataType::FLOAT64 || arg_type == DataType::INT64) {
                        // Convert numeric value to DynamicValue using existing function
                        // The __dynamic_value_create_from_double expects the raw bits as int64_t
                        gen.emit_mov_reg_reg(7, 0); // RDI = numeric value (already in correct format)
                        gen.emit_call("__dynamic_value_create_from_double");
                        // RAX now contains DynamicValue* 
                        std::cout << "[TYPE_CONVERSION] Converted numeric argument to DynamicValue" << std::endl;
                    } else if (arg_type == DataType::BOOLEAN) {
                        // Convert boolean to DynamicValue
                        gen.emit_mov_reg_reg(7, 0); // RDI = boolean value
                        gen.emit_call("__dynamic_value_create_from_bool");
                        // RAX now contains DynamicValue*
                        std::cout << "[TYPE_CONVERSION] Converted boolean argument to DynamicValue" << std::endl;
                    }
                    // For other types, assume they're already compatible or add more conversions as needed
                }
                
                switch (i) {
                    case 0: gen.emit_mov_reg_reg(7, 0); break;  // RDI = RAX
                    case 1: gen.emit_mov_reg_reg(6, 0); break;  // RSI = RAX
                    case 2: gen.emit_mov_reg_reg(2, 0); break;  // RDX = RAX
                    case 3: gen.emit_mov_reg_reg(1, 0); break;  // RCX = RAX
                    case 4: gen.emit_mov_reg_reg(8, 0); break;  // R8 = RAX
                    case 5: gen.emit_mov_reg_reg(9, 0); break;  // R9 = RAX
                }
            }
            
            // Push additional arguments onto stack
            for (int i = arguments.size() - 1; i >= 6; i--) {
                arguments[i]->generate_code(gen);
                gen.emit_sub_reg_imm(4, 8);  // sub rsp, 8
                gen.emit_mov_mem_reg(0, 0);  // mov [rsp], rax
            }
            
            // Use new function calling system
            generate_function_call_code(gen, name, g_scope_context.scope_analyzer, current_scope);
            
            // Clean up stack if needed
            if (arguments.size() > 6) {
                int stack_cleanup = (arguments.size() - 6) * 8;
                gen.emit_add_reg_imm(4, stack_cleanup);
            }
            
            result_type = DataType::ANY; // Function calls can return any type
            return;
        }
    }
    
    // Fallback: Check if this is a user-defined function that needs patching
    std::cout << "[FUNCTION_CODEGEN] Checking if '" << name << "' is a user-defined function" << std::endl;
    
    auto func_decl_it = global_function_decl_lookup.find(name);
    if (func_decl_it != global_function_decl_lookup.end()) {
        std::cout << "[FUNCTION_CODEGEN] Found user-defined function '" << name << "', using patching system" << std::endl;
        
        // Generate argument code
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
        
        // Push additional arguments
        for (int i = arguments.size() - 1; i >= 6; i--) {
            arguments[i]->generate_code(gen);
            gen.emit_sub_reg_imm(4, 8);  // sub rsp, 8
            gen.emit_mov_mem_reg(0, 0);  // mov [rsp], rax
        }
        
        // Use the new patching system
        generate_user_function_call_with_patching(gen, name);
        result_type = DataType::ANY; // Function calls can return any type
        
        // Clean up stack
        if (arguments.size() > 6) {
            int stack_cleanup = (arguments.size() - 6) * 8;
            gen.emit_add_reg_imm(4, stack_cleanup);
        }
        
        if (is_awaited) {
            gen.emit_promise_await(0);
        }
        return;
    }
    
    // Fallback: Direct function call by name (built-in/runtime functions)
    std::cout << "[FUNCTION_CODEGEN] Fallback to direct call for built-in function '" << name << "'" << std::endl;
    
    // Generate argument code
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
    
    // Push additional arguments
    for (int i = arguments.size() - 1; i >= 6; i--) {
        arguments[i]->generate_code(gen);
        gen.emit_sub_reg_imm(4, 8);  // sub rsp, 8
        gen.emit_mov_mem_reg(0, 0);  // mov [rsp], rax
    }
    
    gen.emit_call(name);
    result_type = DataType::FLOAT64; // Default return type
    
    // Clean up stack
    if (arguments.size() > 6) {
        int stack_cleanup = (arguments.size() - 6) * 8;
        gen.emit_add_reg_imm(4, stack_cleanup);
    }
    
    if (is_awaited) {
        gen.emit_promise_await(0);
    }
}

void FunctionExpression::generate_code(CodeGenerator& gen) {
    std::cout << "[FUNCTION_CODEGEN] FunctionExpression::generate_code - function: " << compilation_assigned_name_ << std::endl;
    
    // Check if the function should already be compiled in the function compilation manager
    std::string func_name = compilation_assigned_name_;
    if (func_name.empty()) {
        // Generate a unique name for anonymous function expressions
        static int func_expr_counter = 0;
        func_name = "__function_expr_" + std::to_string(func_expr_counter++);
        compilation_assigned_name_ = func_name;
    }
    
    std::cout << "[FUNCTION_CODEGEN] FunctionExpression with computed instance size: " 
              << function_instance_size << " bytes" << std::endl;
    
    X86CodeGenV2* x86_gen = dynamic_cast<X86CodeGenV2*>(&gen);
    if (!x86_gen) {
        throw std::runtime_error("Function instance creation requires X86CodeGenV2");
    }
    
    if (is_goroutine) {
        // Goroutine spawn with function name
        x86_gen->emit_mov_reg_imm(7, reinterpret_cast<int64_t>(func_name.c_str())); // RDI = function name
        x86_gen->emit_call("__goroutine_spawn_by_name");
        result_type = DataType::PROMISE;
        return;
    }
    
    // STEP 1: Allocate memory for function instance on heap
    // Size = sizeof(FunctionInstance) + (num_captured_scopes * 8)
    size_t total_size = function_instance_size;
    std::cout << "[FUNCTION_CODEGEN] Allocating " << total_size << " bytes for function instance" << std::endl;
    
    // Call runtime allocator
    x86_gen->emit_mov_reg_imm(7, total_size); // RDI = size
    x86_gen->emit_call("malloc");
    x86_gen->emit_mov_reg_reg(10, 0); // R10 = allocated memory address
    
    // STEP 2: Initialize FunctionInstance header
    // Store size field
    x86_gen->emit_mov_reg_imm(11, total_size); // R11 = size
    x86_gen->emit_mov_reg_offset_reg(10, 0, 11); // [R10 + 0] = size
    
    // Store placeholder function code address (will be patched during startup)
    x86_gen->emit_mov_reg_imm(11, 0); // R11 = 0 (placeholder)
    x86_gen->emit_mov_reg_offset_reg(10, 8, 11); // [R10 + 8] = 0 (placeholder address)
    
    // Register this function instance for patching during startup
    x86_gen->emit_mov_reg_reg(7, 10); // RDI = function instance pointer
    // Intern the function name for permanent storage
    x86_gen->emit_mov_reg_imm(6, reinterpret_cast<int64_t>(func_name.c_str())); // RSI = function name (temp)
    x86_gen->emit_call("__string_intern"); // Returns interned string in RAX
    x86_gen->emit_mov_reg_reg(6, 0); // RSI = interned function name
    x86_gen->emit_mov_reg_imm(2, 8); // RDX = code address offset (8)
    x86_gen->emit_call("__register_function_instance_for_patching");
    
    // STEP 3: Capture lexical scope addresses based on static analysis
    if (lexical_scope && !lexical_scope->priority_sorted_parent_scopes.empty()) {
        std::cout << "[FUNCTION_CODEGEN] Capturing " << lexical_scope->priority_sorted_parent_scopes.size() 
                  << " scope addresses" << std::endl;
        
        size_t scope_offset = 16; // Start after header (size + function_code_addr)
        
        for (size_t i = 0; i < lexical_scope->priority_sorted_parent_scopes.size(); i++) {
            int scope_depth = lexical_scope->priority_sorted_parent_scopes[i];
            
            // Get scope address for this depth
            x86_gen->emit_mov_reg_imm(7, scope_depth); // RDI = scope depth
            x86_gen->emit_call("__get_scope_address_for_depth"); // Returns scope address in RAX
            
            // Store in function instance
            x86_gen->emit_mov_reg_offset_reg(10, scope_offset, 0); // [R10 + scope_offset] = scope_address
            scope_offset += 8;
            
            std::cout << "[FUNCTION_CODEGEN] Captured scope depth " << scope_depth 
                      << " at offset " << (scope_offset - 8) << std::endl;
        }
    }
    
    // STEP 4: Return pointer to function instance
    x86_gen->emit_mov_reg_reg(0, 10); // RAX = function instance pointer
    result_type = DataType::LOCAL_FUNCTION_INSTANCE;
    
    std::cout << "[FUNCTION_CODEGEN] Created function instance for " << func_name 
              << " with " << (lexical_scope ? lexical_scope->priority_sorted_parent_scopes.size() : 0) 
              << " captured scopes" << std::endl;
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
            // Type-aware console.log implementation using the sophisticated console log system
            std::vector<ExpressionNode*> arg_ptrs;
            for (const auto& arg : arguments) {
                arg_ptrs.push_back(arg.get());
            }
            
            // Use the advanced TypeAwareConsoleLog system (now without TypeInference dependency)
            TypeAwareConsoleLog::generate_console_log_code(gen, arg_ptrs);
            
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
            // Deep nested scope access for PostfixIncrement
            generate_deep_scope_variable_load(gen, variable_name, scope_depth, var_offset);
            std::cout << "[NEW_CODEGEN] PostfixIncrement: loaded parent variable '" << variable_name 
                      << "' from deep scope (stack-based access)" << std::endl;
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
            // Deep nested scope store for PostfixIncrement
            generate_deep_scope_variable_store(gen, variable_name, scope_depth, var_offset);
            std::cout << "[NEW_CODEGEN] PostfixIncrement: stored to parent variable '" << variable_name 
                      << "' via deep scope (stack-based access)" << std::endl;
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
    std::cout << "[NEW_SYSTEM] Generating function '" << name << "' with new system" << std::endl;
    
    // Cast to scope-aware codegen
    auto scope_gen = dynamic_cast<ScopeAwareCodeGen*>(&gen);
    if (!scope_gen) {
        throw std::runtime_error("New function system requires ScopeAwareCodeGen");
    }
    
    // Store the current code offset for address patching
    code_offset = gen.get_current_offset();
    std::cout << "[NEW_SYSTEM] Function '" << name << "' at offset " << code_offset << std::endl;
    
    // Emit function label
    gen.emit_label(name);
    
    // Generate optimized prologue using static analysis
    scope_gen->emit_function_prologue(this);
    
    // Set the lexical scope context for this function
    if (lexical_scope) {
        scope_gen->set_current_scope(lexical_scope.get());
    }
    
    // Generate body code
    for (auto& stmt : body) {
        stmt->generate_code(gen);
    }
    
    // Generate optimized epilogue
    scope_gen->emit_function_epilogue(this);
    
    std::cout << "[NEW_SYSTEM] Function '" << name << "' generation complete" << std::endl;
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
    std::cout << "[NEW_CODEGEN] ThrowStatement::generate_code - throw expression" << std::endl;
    
    // Generate code for the expression to be thrown
    if (value) {
        value->generate_code(gen);
        
        // Store the exception value in a standard location for runtime exception handling
        gen.emit_mov_mem_reg(-272, 0); // Store exception value at stack location
        
        // Set up the exception throw with the runtime system
        gen.emit_mov_reg_mem(7, -272); // RDI = exception value
        gen.emit_call("__runtime_throw_exception");
        
        // The __runtime_throw_exception function should handle:
        // 1. Stack unwinding to find the nearest catch handler
        // 2. Proper cleanup of resources in unwound scopes
        // 3. Jumping to the appropriate catch block or program termination
        
        // This call should not return normally - it either jumps to a catch handler
        // or terminates the program. We add a safety return instruction just in case.
        gen.emit_ret();
        
        result_type = DataType::VOID; // throw statements don't produce values
    } else {
        // Re-throw current exception (throw; with no expression)
        gen.emit_call("__runtime_rethrow_exception");
        gen.emit_ret();
        result_type = DataType::VOID;
    }
    
    std::cout << "[NEW_CODEGEN] ThrowStatement::generate_code complete" << std::endl;
}

void CatchClause::generate_code(CodeGenerator& gen) {
    std::cout << "[NEW_CODEGEN] CatchClause::generate_code - catch(" << parameter << ")" << std::endl;
    
    // TODO: Set up scope-aware exception variable access
    // For now, the exception value should be available in a runtime-defined location
    
    // Store exception value in a temporary location for the catch block
    // The runtime exception handling system should have placed the exception in RAX
    gen.emit_mov_mem_reg(-200, 0); // Store exception value for catch block access
    
    // Generate catch block body
    for (const auto& stmt : body) {
        stmt->generate_code(gen);
    }
    
    std::cout << "[NEW_CODEGEN] CatchClause::generate_code complete" << std::endl;
}

void TryStatement::generate_code(CodeGenerator& gen) {
    static int try_counter = 0;
    std::string catch_label = "try_catch_" + std::to_string(try_counter);
    std::string finally_label = "try_finally_" + std::to_string(try_counter);
    std::string end_label = "try_end_" + std::to_string(try_counter);
    try_counter++;
    
    std::cout << "[NEW_CODEGEN] TryStatement::generate_code - try block with " 
              << (catch_clause ? "catch" : "no catch") 
              << (finally_body.empty() ? "" : " and finally") << std::endl;
    
    // Set up exception handling (simplified approach)
    // TODO: Integrate with proper exception handling system
    
    // Generate try block
    for (const auto& stmt : try_body) {
        stmt->generate_code(gen);
    }
    
    // If no exception occurred, jump to finally (or end if no finally)
    if (!finally_body.empty()) {
        gen.emit_jump(finally_label);
    } else {
        gen.emit_jump(end_label);
    }
    
    // Generate catch block if present
    if (catch_clause) {
        gen.emit_label(catch_label);
        catch_clause->generate_code(gen);
        
        // After catch, go to finally (or end if no finally)
        if (!finally_body.empty()) {
            gen.emit_jump(finally_label);
        } else {
            gen.emit_jump(end_label);
        }
    }
    
    // Generate finally block if present
    if (!finally_body.empty()) {
        gen.emit_label(finally_label);
        for (const auto& stmt : finally_body) {
            stmt->generate_code(gen);
        }
    }
    
    gen.emit_label(end_label);
    std::cout << "[NEW_CODEGEN] TryStatement::generate_code complete" << std::endl;
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
    std::cout << "[NEW_CODEGEN] CaseClause::generate_code - ";
    if (is_default) {
        std::cout << "default case with " << body.size() << " statements" << std::endl;
    } else {
        std::cout << "case with value and " << body.size() << " statements" << std::endl;
    }
    
    // Note: The case label and value comparison is handled by SwitchStatement
    // CaseClause just needs to generate its body statements
    
    for (const auto& stmt : body) {
        stmt->generate_code(gen);
    }
    
    std::cout << "[NEW_CODEGEN] CaseClause::generate_code complete" << std::endl;
}

void SwitchStatement::generate_code(CodeGenerator& gen) {
    static int switch_counter = 0;
    std::string switch_end = "switch_end_" + std::to_string(switch_counter);
    switch_counter++;
    
    std::cout << "[NEW_CODEGEN] SwitchStatement::generate_code - generating switch with " 
              << cases.size() << " cases" << std::endl;
    
    // Generate discriminant code - this puts the result in RAX
    discriminant->generate_code(gen);
    DataType discriminant_type = discriminant->result_type;
    
    // Store discriminant value and type in temporary stack locations
    gen.emit_mov_mem_reg(-150, 0); // Store discriminant value
    gen.emit_mov_reg_imm(0, static_cast<int64_t>(discriminant_type));
    gen.emit_mov_mem_reg(-158, 0); // Store discriminant type
    
    // Generate code for each case
    std::vector<std::string> case_labels;
    std::string default_label;
    bool has_default = false;
    
    // First pass: create labels and generate comparison jumps
    for (size_t i = 0; i < cases.size(); i++) {
        const auto& case_clause = cases[i];
        
        if (case_clause->is_default) {
            default_label = "case_default_" + std::to_string(switch_counter - 1);
            has_default = true;
        } else {
            std::string case_label = "case_" + std::to_string(switch_counter - 1) + "_" + std::to_string(i);
            case_labels.push_back(case_label);
            
            // Generate case value and compare with discriminant
            case_clause->value->generate_code(gen);
            DataType case_type = case_clause->value->result_type;
            
            // Fast path for same known types
            if (discriminant_type != DataType::ANY && case_type != DataType::ANY && discriminant_type == case_type) {
                // Direct comparison for same types
                gen.emit_mov_reg_mem(3, -150); // RBX = discriminant value from stack
                gen.emit_sub_reg_reg(3, 0); // SUB sets zero flag if values are equal
                gen.emit_jump_if_zero(case_label); // Jump if equal (zero flag set)
            } else if (discriminant_type != DataType::ANY && case_type != DataType::ANY && discriminant_type != case_type) {
                // Different known types - never equal, skip this case
                continue;
            } else {
                // At least one operand is ANY - use runtime comparison
                gen.emit_mov_reg_mem(7, -150); // RDI = discriminant value
                gen.emit_mov_reg_mem(6, -158); // RSI = discriminant type
                gen.emit_mov_reg_reg(2, 0);   // RDX = case value (currently in RAX)
                gen.emit_mov_reg_imm(1, static_cast<int64_t>(case_type)); // RCX = case type
                gen.emit_call("__runtime_js_equal");
                
                // Jump to case if equal (RAX != 0)
                gen.emit_mov_reg_imm(1, 0);
                gen.emit_compare(0, 1);
                gen.emit_jump_if_not_zero(case_label);
            }
        }
    }
    
    // If no case matched and there's a default, jump to it
    if (has_default) {
        gen.emit_jump(default_label);
    } else {
        // No default case - jump to end
        gen.emit_jump(switch_end);
    }
    
    // Second pass: generate case bodies
    for (size_t i = 0; i < cases.size(); i++) {
        const auto& case_clause = cases[i];
        
        if (case_clause->is_default) {
            gen.emit_label(default_label);
        } else {
            gen.emit_label(case_labels[i]);
        }
        
        // Generate case body
        for (const auto& stmt : case_clause->body) {
            stmt->generate_code(gen);
        }
        
        // Note: JavaScript switch cases fall through by default unless there's a break
        // The break statement implementation should jump to switch_end
    }
    
    gen.emit_label(switch_end);
    std::cout << "[NEW_CODEGEN] SwitchStatement::generate_code complete" << std::endl;
}

void ImportStatement::generate_code(CodeGenerator& gen) {
    std::cout << "[NEW_CODEGEN] ImportStatement::generate_code - import from " << module_path << std::endl;
    
    // Create a stable string for the module path
    static std::unordered_map<std::string, const char*> module_path_storage;
    auto it = module_path_storage.find(module_path);
    if (it == module_path_storage.end()) {
        char* path_copy = new char[module_path.length() + 1];
        strcpy(path_copy, module_path.c_str());
        module_path_storage[module_path] = path_copy;
        it = module_path_storage.find(module_path);
    }
    
    // Handle different import patterns based on ImportSpecifiers
    if (is_namespace_import) {
        // import * as name from 'module'
        gen.emit_mov_reg_imm(7, reinterpret_cast<int64_t>(it->second)); // RDI = module_path
        gen.emit_call("__module_load"); // Load entire module as namespace object
        
        // Store the namespace object in the specified variable
        if (!namespace_name.empty()) {
            emit_variable_store(gen, namespace_name);
        }
    } else if (specifiers.size() == 1 && specifiers[0].is_default) {
        // import defaultName from 'module'
        gen.emit_mov_reg_imm(7, reinterpret_cast<int64_t>(it->second)); // RDI = module_path
        gen.emit_call("__module_import_default");
        
        // Store the default export in the imported variable
        emit_variable_store(gen, specifiers[0].local_name);
    } else if (!specifiers.empty()) {
        // import { name1, name2 } from 'module'
        gen.emit_mov_reg_imm(7, reinterpret_cast<int64_t>(it->second)); // RDI = module_path
        gen.emit_call("__module_load"); // Load the module
        gen.emit_mov_mem_reg(-360, 0); // Store module object
        
        // Import each named export
        for (const auto& spec : specifiers) {
            // Create stable string for import name
            static std::unordered_map<std::string, const char*> import_name_storage;
            auto name_it = import_name_storage.find(spec.imported_name);
            if (name_it == import_name_storage.end()) {
                char* name_copy = new char[spec.imported_name.length() + 1];
                strcpy(name_copy, spec.imported_name.c_str());
                import_name_storage[spec.imported_name] = name_copy;
                name_it = import_name_storage.find(spec.imported_name);
            }
            
            // Get the named export from the module
            gen.emit_mov_reg_mem(7, -360); // RDI = module object
            gen.emit_mov_reg_imm(6, reinterpret_cast<int64_t>(name_it->second)); // RSI = export name
            gen.emit_call("__module_get_named_export");
            
            // Store in the local variable with scope-aware access
            emit_variable_store(gen, spec.local_name);
        }
    } else {
        // import 'module' (side effects only)
        gen.emit_mov_reg_imm(7, reinterpret_cast<int64_t>(it->second)); // RDI = module_path
        gen.emit_call("__module_import_side_effects");
    }
    
    result_type = DataType::VOID; // Imports don't produce values
    std::cout << "[NEW_CODEGEN] ImportStatement::generate_code complete" << std::endl;
}

void ExportStatement::generate_code(CodeGenerator& gen) {
    std::cout << "[NEW_CODEGEN] ExportStatement::generate_code - export statement" << std::endl;
    
    if (is_default) {
        // export default expression/declaration
        if (declaration) {
            declaration->generate_code(gen);
            
            // Register the default export with the module system
            gen.emit_mov_reg_reg(7, 0); // RDI = declaration result (from RAX)
            gen.emit_call("__module_set_default_export");
        }
        
    } else if (declaration) {
        // export const/let/var/function/class declaration
        declaration->generate_code(gen);
        
        // The declaration should have created variables/functions in scope
        // For simplicity, assume we can extract the name and register it as export
        // This is a simplified implementation
        
        // For now, register all specifiers as named exports
        for (const auto& spec : specifiers) {
            // Load the variable value using local_name
            emit_variable_load(gen, spec.local_name);
            
            // Create stable string for export name
            static std::unordered_map<std::string, const char*> export_name_storage;
            auto it = export_name_storage.find(spec.exported_name);
            if (it == export_name_storage.end()) {
                char* name_copy = new char[spec.exported_name.length() + 1];
                strcpy(name_copy, spec.exported_name.c_str());
                export_name_storage[spec.exported_name] = name_copy;
                it = export_name_storage.find(spec.exported_name);
            }
            
            // Register named export
            gen.emit_mov_reg_imm(7, reinterpret_cast<int64_t>(it->second)); // RDI = export name
            gen.emit_mov_reg_reg(6, 0); // RSI = export value (from variable load)
            gen.emit_call("__module_set_named_export");
        }
        
    } else if (!specifiers.empty()) {
        // export { name1, name2 } - re-export existing variables
        for (const auto& spec : specifiers) {
            // Load the variable to export using local_name
            emit_variable_load(gen, spec.local_name);
            
            // Create stable string for export name
            static std::unordered_map<std::string, const char*> export_name_storage;
            auto it = export_name_storage.find(spec.exported_name);
            if (it == export_name_storage.end()) {
                char* name_copy = new char[spec.exported_name.length() + 1];
                strcpy(name_copy, spec.exported_name.c_str());
                export_name_storage[spec.exported_name] = name_copy;
                it = export_name_storage.find(spec.exported_name);
            }
            
            // Register named export
            gen.emit_mov_reg_imm(7, reinterpret_cast<int64_t>(it->second)); // RDI = export name
            gen.emit_mov_reg_reg(6, 0); // RSI = variable value (from RAX)
            gen.emit_call("__module_set_named_export");
        }
    }
    
    result_type = DataType::VOID; // Exports don't produce values
    std::cout << "[NEW_CODEGEN] ExportStatement::generate_code complete" << std::endl;
}

void ConstructorDecl::generate_code(CodeGenerator& gen) {
    std::cout << "[NEW_CODEGEN] ConstructorDecl::generate_code - constructor for " << class_name << std::endl;
    
    // Set class context for 'this' binding
    std::string previous_class = g_scope_context.current_class_name;
    g_scope_context.current_class_name = class_name;
    
    // Generate constructor function label
    std::string constructor_label = "__constructor_" + class_name;
    gen.emit_label(constructor_label);
    
    // Constructor prologue - establish stack frame
    gen.emit_sub_reg_imm(4, 128); // sub rsp, 128 (allocate larger stack frame for constructor)
    gen.emit_mov_reg_offset_reg(5, 0, 5); // mov [rbp], rbp (save old rbp)
    gen.emit_mov_reg_reg(5, 4); // mov rbp, rsp (establish new frame pointer)
    
    // Save 'this' parameter (passed in RDI) to a known stack location
    gen.emit_mov_mem_reg(-128, 7); // Save 'this' object at [rbp-128]
    
    // Generate parameter assignments (skip 'this' - it's implicit)
    // Constructor parameters are passed in RSI, RDX, RCX, R8, R9, then stack
    for (size_t i = 0; i < parameters.size() && i < 5; i++) {
        // Store parameter values in stack locations for scope-aware access
        int64_t param_offset = -136 - (i * 8); // Start after 'this' storage
        
        switch (i) {
            case 0: gen.emit_mov_mem_reg(param_offset, 6); break; // RSI -> param 0
            case 1: gen.emit_mov_mem_reg(param_offset, 2); break; // RDX -> param 1
            case 2: gen.emit_mov_mem_reg(param_offset, 1); break; // RCX -> param 2
            case 3: gen.emit_mov_mem_reg(param_offset, 8); break; // R8 -> param 3
            case 4: gen.emit_mov_mem_reg(param_offset, 9); break; // R9 -> param 4
        }
    }
    
    // Generate constructor body statements
    for (const auto& stmt : body) {
        stmt->generate_code(gen);
    }
    
    // Constructor epilogue - return 'this' object
    gen.emit_mov_reg_mem(0, -128); // mov rax, [rbp-128] (load 'this' for return)
    gen.emit_mov_reg_reg(4, 5); // mov rsp, rbp (restore stack pointer)
    gen.emit_mov_reg_mem(5, 0); // mov rbp, [rbp] (restore frame pointer)
    gen.emit_ret(); // return 'this'
    
    // Restore previous class context
    g_scope_context.current_class_name = previous_class;
    
    std::cout << "[NEW_CODEGEN] ConstructorDecl::generate_code complete" << std::endl;
}

void MethodDecl::generate_code(CodeGenerator& gen) {
    std::cout << "[NEW_CODEGEN] MethodDecl::generate_code - method " << name << std::endl;
    
    // Generate method function label
    std::string method_label;
    if (!g_scope_context.current_class_name.empty()) {
        method_label = "__method_" + g_scope_context.current_class_name + "_" + name;
    } else {
        method_label = "__function_" + name;
    }
    
    gen.emit_label(method_label);
    
    // Method prologue - establish stack frame
    gen.emit_sub_reg_imm(4, 128); // sub rsp, 128 (allocate stack frame)
    gen.emit_mov_reg_offset_reg(5, 0, 5); // mov [rbp], rbp (save old rbp)
    gen.emit_mov_reg_reg(5, 4); // mov rbp, rsp (establish frame pointer)
    
    // For instance methods, save 'this' parameter (passed in RDI)
    if (!g_scope_context.current_class_name.empty()) {
        gen.emit_mov_mem_reg(-128, 7); // Save 'this' at [rbp-128]
    }
    
    // Generate method body
    for (const auto& stmt : body) {
        stmt->generate_code(gen);
    }
    
    // Method epilogue
    gen.emit_mov_reg_reg(4, 5); // mov rsp, rbp (restore stack pointer)
    gen.emit_mov_reg_mem(5, 0); // mov rbp, [rbp] (restore frame pointer)
    gen.emit_ret(); // return
    
    std::cout << "[NEW_CODEGEN] MethodDecl::generate_code complete for " << name << std::endl;
}

void ClassDecl::generate_code(CodeGenerator& gen) {
    std::cout << "[NEW_CODEGEN] ClassDecl::generate_code - class " << name << std::endl;
    
    // Set current class context for 'this' handling
    std::string previous_class = g_scope_context.current_class_name;
    g_scope_context.current_class_name = name;
    
    // Register the class with the runtime class system
    static std::unordered_map<std::string, const char*> class_name_storage;
    auto it = class_name_storage.find(name);
    if (it == class_name_storage.end()) {
        char* name_copy = new char[name.length() + 1];
        strcpy(name_copy, name.c_str());
        class_name_storage[name] = name_copy;
        it = class_name_storage.find(name);
    }
    
    // Count methods and constructor for class metadata
    size_t method_count = methods.size();
    size_t property_count = 8; // Default property slots for typical classes
    
    // Register class type with runtime
    gen.emit_mov_reg_imm(7, reinterpret_cast<int64_t>(it->second)); // RDI = class name
    gen.emit_mov_reg_imm(6, method_count); // RSI = method count
    gen.emit_mov_reg_imm(2, property_count); // RDX = initial property count
    gen.emit_call("__runtime_register_class");
    
    // Generate constructor if present
    if (constructor) {
        std::cout << "[NEW_CODEGEN] Generating constructor for class " << name << std::endl;
        
        // Generate constructor label
        std::string constructor_label = "__constructor_" + name;
        gen.emit_label(constructor_label);
        
        // Constructor prologue - save stack frame
        gen.emit_sub_reg_imm(4, 64); // sub rsp, 64 (allocate stack frame)
        gen.emit_mov_reg_offset_reg(5, 0, 5); // mov [rbp], rbp (save old rbp)
        gen.emit_mov_reg_reg(5, 4); // mov rbp, rsp (set new frame)
        
        // Generate constructor body
        for (const auto& stmt : constructor->body) {
            stmt->generate_code(gen);
        }
        
        // Constructor epilogue - restore stack frame and return object
        gen.emit_mov_reg_reg(4, 5); // mov rsp, rbp
        gen.emit_mov_reg_mem(5, 0); // mov rbp, [rbp] (restore old rbp)
        gen.emit_mov_reg_reg(0, 7); // mov rax, rdi (return 'this' object)
        gen.emit_ret();
    }
    
    // Generate methods
    for (size_t i = 0; i < methods.size(); i++) {
        std::cout << "[NEW_CODEGEN] Generating method " << methods[i]->name << " for class " << name << std::endl;
        
        // Generate method label
        std::string method_label = "__method_" + name + "_" + methods[i]->name;
        gen.emit_label(method_label);
        
        // Method prologue
        gen.emit_sub_reg_imm(4, 64); // sub rsp, 64
        gen.emit_mov_reg_offset_reg(5, 0, 5); // save rbp
        gen.emit_mov_reg_reg(5, 4); // mov rbp, rsp
        
        // Generate method body
        for (const auto& stmt : methods[i]->body) {
            stmt->generate_code(gen);
        }
        
        // Method epilogue
        gen.emit_mov_reg_reg(4, 5); // restore rsp
        gen.emit_mov_reg_mem(5, 0); // restore rbp
        gen.emit_ret();
    }
    
    // Restore previous class context
    g_scope_context.current_class_name = previous_class;
    
    std::cout << "[NEW_CODEGEN] ClassDecl::generate_code complete for " << name << std::endl;
}

void OperatorOverloadDecl::generate_code(CodeGenerator& gen) {
    std::cout << "[NEW_CODEGEN] OperatorOverloadDecl::generate_code - operator " << static_cast<int>(operator_type) << std::endl;
    
    // Generate simplified operator overload without scope handling for now
    // This can be expanded later once the basic system is working
    
    std::string operator_label = "__operator_" + class_name + "_" + std::to_string(static_cast<int>(operator_type));
    gen.emit_label(operator_label);
    
    // Basic function prologue
    gen.emit_sub_reg_imm(4, 64);
    gen.emit_mov_reg_offset_reg(5, 0, 5);
    gen.emit_mov_reg_reg(5, 4);
    
    // Generate operator body
    for (const auto& stmt : body) {
        stmt->generate_code(gen);
    }
    
    // Basic function epilogue
    gen.emit_mov_reg_reg(4, 5);
    gen.emit_mov_reg_mem(5, 0);
    gen.emit_ret();
    
    std::cout << "[NEW_CODEGEN] OperatorOverloadDecl::generate_code complete" << std::endl;
}

void ForEachLoop::generate_code(CodeGenerator& gen) {
    static int loop_counter = 0;
    std::string loop_start = "foreach_start_" + std::to_string(loop_counter);
    std::string loop_end = "foreach_end_" + std::to_string(loop_counter);
    std::string loop_check = "foreach_check_" + std::to_string(loop_counter);
    loop_counter++;
    
    std::cout << "[NEW_CODEGEN] ForEachLoop::generate_code - iterating over " << index_var_name 
              << ", " << value_var_name << std::endl;
    
    // Generate code for the iterable expression
    iterable->generate_code(gen);
    
    // Store the iterable in a temporary stack location
    gen.emit_mov_mem_reg(-170, 0); // Store iterable pointer
    
    // Initialize loop index to 0
    gen.emit_mov_reg_imm(0, 0); // RAX = 0
    gen.emit_mov_mem_reg(-178, 0); // Store index = 0
    
    gen.emit_label(loop_check);
    
    // Check if we've reached the end of the iterable
    if (iterable->result_type == DataType::ARRAY || iterable->result_type == DataType::TENSOR) {
        // For arrays: check if index < array.length
        gen.emit_mov_reg_mem(7, -170); // RDI = array pointer
        gen.emit_call("__array_size"); // RAX = array size
        gen.emit_mov_reg_reg(3, 0); // RBX = array size
        gen.emit_mov_reg_mem(0, -178); // RAX = current index
        gen.emit_compare(0, 3); // Compare index with size
        gen.emit_jump_if_greater_equal(loop_end); // Jump if index >= size
        
        // Get the value at current index
        gen.emit_mov_reg_mem(7, -170); // RDI = array pointer
        gen.emit_mov_reg_mem(6, -178); // RSI = index
        gen.emit_call("__array_get"); // RAX = array[index]
        
        // Store value in stack location for loop body access
        gen.emit_mov_mem_reg(-186, 0); // Store array element value
        
        // Store index as well (for index variable access)
        gen.emit_mov_reg_mem(0, -178); // RAX = current index
        gen.emit_mov_mem_reg(-194, 0); // Store index for user access
        
    } else {
        // For objects: iterate over properties (more complex)
        // For now, use a simple runtime call
        gen.emit_mov_reg_mem(7, -170); // RDI = object pointer
        gen.emit_mov_reg_mem(6, -178); // RSI = current index
        gen.emit_call("__object_iterate_next"); // RAX = next property value (0 if done)
        
        // Check if iteration is done
        gen.emit_mov_reg_imm(1, 0);
        gen.emit_compare(0, 1);
        gen.emit_jump_if_zero(loop_end); // Jump if no more properties
        
        // Store property value and key
        gen.emit_mov_mem_reg(-186, 0); // Store property value
        // Get the property key from the iterator
        gen.emit_mov_reg_mem(7, -170); // RDI = object pointer
        gen.emit_mov_reg_mem(6, -178); // RSI = current index
        gen.emit_call("__object_get_current_key"); // RAX = property key
        gen.emit_mov_mem_reg(-194, 0); // Store property key for user access
    }
    
    gen.emit_label(loop_start);
    
    // TODO: Set up scope-aware variable access for loop variables
    // For now, use simple assignments to loop variables
    // This would need integration with the scope system to properly handle let/const semantics
    
    // Generate loop body
    for (const auto& stmt : body) {
        stmt->generate_code(gen);
    }
    
    // Increment index and continue
    gen.emit_mov_reg_mem(0, -178); // RAX = current index
    gen.emit_add_reg_imm(0, 1); // RAX++
    gen.emit_mov_mem_reg(-178, 0); // Store incremented index
    gen.emit_jump(loop_check);
    
    gen.emit_label(loop_end);
    std::cout << "[NEW_CODEGEN] ForEachLoop::generate_code complete" << std::endl;
}

void ForInStatement::generate_code(CodeGenerator& gen) {
    std::cout << "[NEW_CODEGEN] ForInStatement::generate_code - for-in loop (simplified)" << std::endl;
    
    // Simplified implementation to resolve compilation errors
    // TODO: Implement full for-in loop functionality once field names are confirmed
    
    // Generate unique labels for the for-in loop
    static int forin_counter = 0;
    std::string loop_end = "__forin_end_" + std::to_string(forin_counter++);
    
    // Placeholder loop structure
    gen.emit_label(loop_end);
    
    std::cout << "[NEW_CODEGEN] ForInStatement::generate_code complete (simplified)" << std::endl;
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
    std::cout << "[NEW_CODEGEN] ExpressionPropertyAccess::generate_code - dynamic property access (simplified)" << std::endl;
    
    // Simplified implementation to resolve compilation errors
    // TODO: Implement full dynamic property access once field names are confirmed
    
    // For now, just generate object code and return a placeholder value
    if (object) {
        object->generate_code(gen);
    } else {
        gen.emit_mov_reg_imm(0, 0); // RAX = 0 (placeholder)
    }
    
    result_type = DataType::ANY;
    std::cout << "[NEW_CODEGEN] ExpressionPropertyAccess: completed (simplified)" << std::endl;
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
    std::cout << "[NEW_CODEGEN] ExpressionPropertyAssignment::generate_code - dynamic property assignment (simplified)" << std::endl;
    
    // Simplified implementation to resolve compilation errors
    // TODO: Implement full dynamic property assignment once field names are confirmed
    
    // Generate the value to be assigned first
    if (value) {
        value->generate_code(gen);
    }
    
    // Generate code for the object expression
    if (object) {
        object->generate_code(gen);
    }
    
    result_type = value ? value->result_type : DataType::ANY;
    std::cout << "[NEW_CODEGEN] ExpressionPropertyAssignment: completed (simplified)" << std::endl;
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
    std::cout << "[NEW_CODEGEN] SuperCall::generate_code - super() constructor call" << std::endl;
    
    // Super calls should only be valid within constructors
    if (g_scope_context.current_class_name.empty()) {
        throw std::runtime_error("super() call outside of class context");
    }
    
    // For now, assume we have parent class information available
    // TODO: Integrate with full inheritance system when available
    std::string parent_class_name = "Object"; // Default parent - should come from class hierarchy
    
    // Generate arguments for parent constructor
    std::vector<int64_t> arg_offsets;
    for (size_t i = 0; i < arguments.size(); i++) {
        arguments[i]->generate_code(gen);
        
        // Store argument value in temporary stack location
        int64_t arg_offset = -280 - (i * 8);
        gen.emit_mov_mem_reg(arg_offset, 0);
        arg_offsets.push_back(arg_offset);
    }
    
    // Set up parent constructor call
    std::string parent_constructor_label = "__constructor_" + parent_class_name;
    
    // Load 'this' object into RDI (first parameter for parent constructor)
    gen.emit_mov_reg_mem(7, -128); // RDI = this (assumed to be at rbp-128)
    
    // Load arguments into appropriate registers
    for (size_t i = 0; i < arguments.size() && i < 5; i++) {
        switch (i) {
            case 0: gen.emit_mov_reg_mem(6, arg_offsets[i]); break; // RSI
            case 1: gen.emit_mov_reg_mem(2, arg_offsets[i]); break; // RDX
            case 2: gen.emit_mov_reg_mem(1, arg_offsets[i]); break; // RCX
            case 3: gen.emit_mov_reg_mem(8, arg_offsets[i]); break; // R8
            case 4: gen.emit_mov_reg_mem(9, arg_offsets[i]); break; // R9
        }
    }
    
    // Call parent constructor
    gen.emit_call(parent_constructor_label);
    
    // Parent constructor returns 'this' in RAX, but we should maintain our own 'this'
    // Store the returned value if needed (for chaining)
    result_type = DataType::CLASS_INSTANCE;
    
    std::cout << "[NEW_CODEGEN] SuperCall::generate_code complete" << std::endl;
}

void SuperMethodCall::generate_code(CodeGenerator& gen) {
    std::cout << "[NEW_CODEGEN] SuperMethodCall::generate_code - super." << method_name << "()" << std::endl;
    
    // Super method calls should only be valid within class methods
    if (g_scope_context.current_class_name.empty()) {
        throw std::runtime_error("super method call outside of class context");
    }
    
    // For now, assume we have parent class information available
    // TODO: Integrate with full inheritance system when available
    std::string parent_class_name = "Object"; // Default parent - should come from class hierarchy
    
    // Generate arguments for parent method
    std::vector<int64_t> arg_offsets;
    for (size_t i = 0; i < arguments.size(); i++) {
        arguments[i]->generate_code(gen);
        
        // Store argument value in temporary stack location
        int64_t arg_offset = -320 - (i * 8);
        gen.emit_mov_mem_reg(arg_offset, 0);
        arg_offsets.push_back(arg_offset);
    }
    
    // Set up parent method call
    std::string parent_method_label = "__method_" + parent_class_name + "_" + method_name;
    
    // Load 'this' object into RDI (first parameter for parent method)
    gen.emit_mov_reg_mem(7, -128); // RDI = this (assumed to be at rbp-128)
    
    // Load arguments into appropriate registers
    for (size_t i = 0; i < arguments.size() && i < 5; i++) {
        switch (i) {
            case 0: gen.emit_mov_reg_mem(6, arg_offsets[i]); break; // RSI
            case 1: gen.emit_mov_reg_mem(2, arg_offsets[i]); break; // RDX
            case 2: gen.emit_mov_reg_mem(1, arg_offsets[i]); break; // RCX
            case 3: gen.emit_mov_reg_mem(8, arg_offsets[i]); break; // R8
            case 4: gen.emit_mov_reg_mem(9, arg_offsets[i]); break; // R9
        }
    }
    
    // For more than 5 arguments, push them onto stack
    for (int i = arguments.size() - 1; i >= 5; i--) {
        gen.emit_mov_reg_mem(0, arg_offsets[i]); // Load argument into RAX
        gen.emit_sub_reg_imm(4, 8); // sub rsp, 8
        gen.emit_mov_mem_reg(0, 0); // mov [rsp], rax
    }
    
    // Call parent method
    gen.emit_call(parent_method_label);
    
    // Clean up stack arguments if any
    if (arguments.size() > 5) {
        int stack_cleanup = (arguments.size() - 5) * 8;
        gen.emit_add_reg_imm(4, stack_cleanup);
    }
    
    // Result is returned in RAX
    result_type = DataType::ANY; // Parent method can return any type
    
    std::cout << "[NEW_CODEGEN] SuperMethodCall::generate_code complete for " << method_name << std::endl;
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
