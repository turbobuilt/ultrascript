#include "lexical_scope_layout.h"
#include <iostream>
#include <sstream>
#include <cassert>
#include <algorithm>

// *** PURE COMPILE-TIME ANALYSIS ENGINE ***
// This file contains ZERO runtime functions - everything generates assembly code.

// COMPILE-TIME: Create scope information for a function
FunctionScopeInfo* LexicalScopeManager::create_function_scope_info(FunctionExpression* func) {
    auto info = std::make_unique<FunctionScopeInfo>(func);
    
    // Create root scope (level 0 - current function scope)
    auto root_scope = std::make_unique<LexicalScopeLayout>(0);
    root_scope->is_stack_allocated = true;
    root_scope->base_register = "rbp";
    info->scopes.push_back(std::move(root_scope));
    
    auto* result = info.get();
    function_scopes_[func] = std::move(info);
    
    std::cout << "[LEXICAL SCOPE] Created function scope info for function, root scope allocated on stack with rbp base register" << std::endl;
    
    return result;
}

// COMPILE-TIME: Add variable to a specific scope level (matching tracker signature)
void LexicalScopeManager::add_variable_to_scope(FunctionExpression* func, int scope_level, const std::string& var_name, const std::string& var_type) {
    auto it = function_scopes_.find(func);
    if (it == function_scopes_.end()) {
        create_function_scope_info(func);
        it = function_scopes_.find(func);
    }
    
    FunctionScopeInfo* info = it->second.get();
    
    // Ensure we have enough scope levels
    while (info->scopes.size() <= static_cast<size_t>(scope_level)) {
        int level = info->scopes.size();
        auto new_scope = std::make_unique<LexicalScopeLayout>(level);
        new_scope->is_stack_allocated = true;
        new_scope->base_register = (level == 1) ? "r12" : (level == 2) ? "r13" : "r14";
        info->scopes.push_back(std::move(new_scope));
        
        std::cout << "[LexicalScopeManager] COMPILE-TIME: Created new scope level " << level << std::endl;
    }
    
    // Get type info (size and alignment)
    auto [size, alignment] = ScopeVariable::get_type_info(var_type);
    
    // Add variable to the specified scope
    ScopeVariable var(var_name, var_type, size, alignment, false);  // default: var not let
    var.scope_level = scope_level;
    
    info->scopes[scope_level]->variables.push_back(var);
    
    std::cout << "[LexicalScopeManager] COMPILE-TIME: Added variable '" << var_name 
              << "' (type: " << var_type << ", size: " << size << ", align: " << alignment 
              << ") to scope level " << scope_level << std::endl;
}

// COMPILE-TIME: Mark a variable as escaped (needs heap allocation or cross-scope access)
void LexicalScopeManager::mark_variable_as_escaped(FunctionExpression* func, const std::string& var_name) {
    auto it = function_scopes_.find(func);
    if (it == function_scopes_.end()) {
        return;
    }
    
    FunctionScopeInfo* info = it->second.get();
    
    // Find the variable in any scope and mark it as escaped
    for (auto& scope : info->scopes) {
        for (auto& var : scope->variables) {
            if (var.name == var_name) {
                var.escapes = true;
                std::cout << "[LexicalScopeManager] COMPILE-TIME: Marked variable '" << var_name 
                          << "' as ESCAPED (scope level " << var.scope_level << ")" << std::endl;
                return;
            }
        }
    }
}

// COMPILE-TIME: Calculate memory layouts with optimal packing (renamed from calculate_memory_layouts)
void LexicalScopeManager::calculate_scope_layouts(FunctionExpression* func) {
    auto it = function_scopes_.find(func);
    if (it == function_scopes_.end()) {
        return;
    }
    
    FunctionScopeInfo* info = it->second.get();
    
    std::cout << "[LexicalScopeManager] COMPILE-TIME: Calculating memory layouts..." << std::endl;
    
    // Calculate variable offsets for each scope
    for (auto& scope : info->scopes) {
        calculate_variable_offsets(scope.get());
        std::cout << "[LexicalScopeManager] COMPILE-TIME: Scope level " << scope->scope_level 
                  << " total size: " << scope->total_size << " bytes" << std::endl;
    }
    
    // Determine allocation strategy (stack vs heap) based on escape analysis
    determine_allocation_strategy(info);
}

// COMPILE-TIME: Determine which scope addresses need to be passed in registers
void LexicalScopeManager::allocate_scope_registers(FunctionExpression* func) {
    auto it = function_scopes_.find(func);
    if (it == function_scopes_.end()) {
        return;
    }
    
    FunctionScopeInfo* info = it->second.get();
    
    std::cout << "[LexicalScopeManager] COMPILE-TIME: Allocating registers for scope addresses..." << std::endl;
    
    // Assign registers for scope base addresses (starting from R13)
    std::vector<std::string> available_registers = {"r13", "r14", "r15", "rbx", "r12"};
    
    for (size_t i = 0; i < info->scopes.size() && i < available_registers.size(); ++i) {
        info->scopes[i]->base_register = available_registers[i];
        std::cout << "[LexicalScopeManager] COMPILE-TIME: Scope level " << i 
                  << " assigned to register " << available_registers[i] << std::endl;
    }
}

// COMPILE-TIME: Get function scope information
const FunctionScopeInfo* LexicalScopeManager::get_function_scope_info(FunctionExpression* func) const {
    auto it = function_scopes_.find(func);
    return (it != function_scopes_.end()) ? it->second.get() : nullptr;
}

// ASSEMBLY GENERATION: Generate assembly code for accessing a variable
std::string LexicalScopeManager::generate_variable_access_asm(FunctionExpression* func, const std::string& var_name) const {
    const ScopeVariable* var = find_variable(func, var_name);
    if (!var) {
        return "; ERROR: Variable '" + var_name + "' not found in lexical scope";
    }
    
    const LexicalScopeLayout* scope_layout = get_scope_layout(func, var->scope_level);
    if (!scope_layout) {
        return "; ERROR: Scope layout not found for variable '" + var_name + "'";
    }
    
    std::ostringstream asm_code;
    asm_code << "; LEXICAL SCOPE ACCESS: Variable '" << var_name << "' from scope level " << var->scope_level << "\n";
    
    // Determine which register to use based on scope level:
    // - scope_level 0 (current function): use r15 
    // - scope_level > 0 (parent scope): use r12
    std::string base_register;
    if (var->scope_level == 0) {
        base_register = "r15";  // Current scope
        asm_code << "    ; Current scope variable: " << var_name << "\n";
    } else {
        base_register = "r12";  // Parent scope (for goroutines)
        asm_code << "    ; Parent scope variable: " << var_name << " (from scope level " << var->scope_level << ")\n";
    }
    
    asm_code << "    mov rax, [" << base_register << " + " << var->offset << "]  ; " 
             << var_name << " (" << var->type << ", " << var->size << " bytes)";
    
    return asm_code.str();
}

// COMPILE-TIME: Print debug information for all scopes
void LexicalScopeManager::print_all_scope_info() const {
    std::cout << "\n[LEXICAL SCOPE DEBUG] Complete scope information:" << std::endl;
    for (const auto& [func, info] : function_scopes_) {
        std::cout << get_debug_info(func) << std::endl;
    }
}

// Get scope layout by level
const LexicalScopeLayout* LexicalScopeManager::get_scope_layout(FunctionExpression* func, int scope_level) const {
    const FunctionScopeInfo* info = get_function_scope_info(func);
    if (!info || scope_level < 0 || static_cast<size_t>(scope_level) >= info->scopes.size()) {
        return nullptr;
    }
    return info->scopes[scope_level].get();
}

// Find variable in any scope
const ScopeVariable* LexicalScopeManager::find_variable(FunctionExpression* func, const std::string& var_name) const {
    const FunctionScopeInfo* info = get_function_scope_info(func);
    if (!info) {
        return nullptr;
    }
    
    // Search through all scopes starting from current scope
    for (const auto& scope : info->scopes) {
        for (const auto& var : scope->variables) {
            if (var.name == var_name) {
                return &var;
            }
        }
    }
    
    return nullptr;
}

// Check if function requires lexical scope
bool LexicalScopeManager::requires_lexical_scope(FunctionExpression* func) const {
    const FunctionScopeInfo* info = get_function_scope_info(func);
    if (!info) {
        return false;
    }
    
    // Function needs lexical scope if:
    // 1. It has more than just the root scope (level 0)
    // 2. Any variables are marked as escaped
    
    if (info->scopes.size() > 1) {
        return true;  // Has parent scopes
    }
    
    // Check if any variables in current scope are escaped
    if (!info->scopes.empty()) {
        for (const auto& var : info->scopes[0]->variables) {
            if (var.escapes) {
                return true;
            }
        }
    }
    
    return false;
}

// Get debug information string
std::string LexicalScopeManager::get_debug_info(FunctionExpression* func) const {
    const FunctionScopeInfo* info = get_function_scope_info(func);
    if (!info) {
        return "No scope information available";
    }
    
    std::ostringstream debug;
    debug << "Function scope information (" << info->scopes.size() << " scopes):\n";
    
    for (size_t i = 0; i < info->scopes.size(); ++i) {
        const auto& scope = info->scopes[i];
        debug << "  Scope level " << i << " (register: " << scope->base_register 
              << ", size: " << scope->total_size << " bytes, " 
              << (scope->is_stack_allocated ? "stack" : "heap") << "):\n";
        
        for (const auto& var : scope->variables) {
            debug << "    - " << var.name << " (" << var.type << ") at offset +" 
                  << var.offset << " (size: " << var.size << ", align: " 
                  << var.alignment << ", escaped: " << (var.escapes ? "yes" : "no") << ")\n";
        }
    }
    
    return debug.str();
}

// Clear scope info for a function
void LexicalScopeManager::clear_scope_info(FunctionExpression* func) {
    function_scopes_.erase(func);
}

// Clear all scope information
void LexicalScopeManager::clear_all() {
    function_scopes_.clear();
}
// COMPILE-TIME PRIVATE: Calculate variable offsets within a scope using optimal packing
void LexicalScopeManager::calculate_variable_offsets(LexicalScopeLayout* scope_layout) {
    if (scope_layout->variables.empty()) {
        scope_layout->total_size = 0;
        return;
    }
    
    std::cout << "[LexicalScopeManager] COMPILE-TIME: Calculating offsets for " 
              << scope_layout->variables.size() << " variables" << std::endl;
    
    // Sort variables for optimal packing (largest alignment first, then by size)
    std::sort(scope_layout->variables.begin(), scope_layout->variables.end(),
              [](const ScopeVariable& a, const ScopeVariable& b) {
                  if (a.alignment != b.alignment) {
                      return a.alignment > b.alignment;  // Higher alignment first
                  }
                  return a.size > b.size;                // Larger size first
              });
    
    size_t current_offset = 0;
    
    for (auto& var : scope_layout->variables) {
        // Align the current offset to the variable's alignment requirement
        size_t aligned_offset = (current_offset + var.alignment - 1) & ~(var.alignment - 1);
        
        var.offset = aligned_offset;
        current_offset = aligned_offset + var.size;
        
        std::cout << "[LexicalScopeManager] COMPILE-TIME: Variable '" << var.name 
                  << "' placed at offset " << var.offset << " (size: " << var.size 
                  << ", align: " << var.alignment << ")" << std::endl;
    }
    
    // Align the total size to the largest alignment requirement
    size_t max_alignment = 8; // Default to 8-byte alignment
    for (const auto& var : scope_layout->variables) {
        max_alignment = std::max(max_alignment, var.alignment);
    }
    
    scope_layout->total_size = (current_offset + max_alignment - 1) & ~(max_alignment - 1);
    
    std::cout << "[LexicalScopeManager] COMPILE-TIME: Scope total size: " << scope_layout->total_size 
              << " bytes (max alignment: " << max_alignment << ")" << std::endl;
}

// COMPILE-TIME PRIVATE: Determine stack vs heap allocation strategy
void LexicalScopeManager::determine_allocation_strategy(FunctionScopeInfo* info) {
    std::cout << "[LexicalScopeManager] COMPILE-TIME: Determining allocation strategies (per-scope decision)..." << std::endl;
    
    // Each scope level makes its own stack vs heap decision based on whether ITS variables escape
    for (auto& scope : info->scopes) {
        bool scope_has_escaped_vars = false;
        
        // Check if any variables in THIS specific scope level have escaped
        for (const auto& var : scope->variables) {
            if (var.escapes) {
                scope_has_escaped_vars = true;
                std::cout << "[LexicalScopeManager] COMPILE-TIME: Variable '" << var.name 
                          << "' in scope level " << scope->scope_level << " has ESCAPED" << std::endl;
            }
        }
        
        // Decision: heap allocation if THIS scope's variables escape OR size exceeds threshold
        const size_t HEAP_THRESHOLD = 1024; // 1KB threshold for large scopes
        bool use_heap_for_this_scope = scope_has_escaped_vars || (scope->total_size > HEAP_THRESHOLD);
        
        if (use_heap_for_this_scope) {
            scope->is_stack_allocated = false;
            scope->allocate_on_heap = true;
            std::cout << "[LexicalScopeManager] COMPILE-TIME: Scope level " << scope->scope_level 
                      << " -> HEAP allocation (escaped vars: " << scope_has_escaped_vars 
                      << ", size: " << scope->total_size << " bytes)" << std::endl;
        } else {
            scope->is_stack_allocated = true;
            scope->allocate_on_heap = false;
            std::cout << "[LexicalScopeManager] COMPILE-TIME: Scope level " << scope->scope_level 
                      << " -> STACK allocation (no escapes, size: " << scope->total_size << " bytes)" << std::endl;
        }
    }
    
    // TODO: Analyze which scope levels are needed by child functions for register/stack passing
    std::cout << "[LexicalScopeManager] COMPILE-TIME: Next step - analyze scope level dependencies for register/stack passing" << std::endl;
}

// COMPILE-TIME: Analyze which parent scope levels a child function needs access to
void LexicalScopeManager::analyze_scope_dependencies(FunctionExpression* parent_func, FunctionExpression* child_func) {
    std::cout << "[LexicalScopeManager] COMPILE-TIME: Analyzing scope dependencies from parent to child function" << std::endl;
    
    const FunctionScopeInfo* parent_info = get_function_scope_info(parent_func);
    if (!parent_info) {
        std::cout << "[LexicalScopeManager] COMPILE-TIME: No parent scope info found" << std::endl;
        return;
    }
    
    // TODO: This would be called during escape analysis to determine which scopes child needs
    // For now, assume child needs access to parent's current scope (level 0)
    std::cout << "[LexicalScopeManager] COMPILE-TIME: Child function needs access to parent scope level 0" << std::endl;
}

// ASSEMBLY GENERATION: Generate assembly for passing scope addresses to child function  
std::string LexicalScopeManager::generate_scope_address_passing_asm(FunctionExpression* parent_func, 
                                                                   FunctionExpression* child_func) const {
    std::cout << "[LexicalScopeManager] COMPILE-TIME: Generating scope address passing assembly" << std::endl;
    
    const FunctionScopeInfo* parent_info = get_function_scope_info(parent_func);
    if (!parent_info) {
        return "; ERROR: No parent scope info for address passing";
    }
    
    std::ostringstream asm_code;
    asm_code << "; LEXICAL SCOPE ADDRESS PASSING\n";
    
    // Pass up to 3 scope addresses in registers: r12, r13, r14
    std::vector<std::string> scope_registers = {"r12", "r13", "r14"};
    int passed_scopes = 0;
    
    for (size_t level = 0; level < parent_info->scopes.size() && passed_scopes < 3; ++level) {
        const auto& scope = parent_info->scopes[level];
        
        if (scope->is_stack_allocated) {
            // Stack-allocated scope: pass stack address
            asm_code << "    lea " << scope_registers[passed_scopes] << ", [rbp - " 
                     << (level * 8 + 8) << "]  ; pass stack scope level " << level << " address\n";
        } else {
            // Heap-allocated scope: pass heap pointer
            asm_code << "    mov " << scope_registers[passed_scopes] << ", [rbp - " 
                     << (level * 8 + 8) << "]  ; pass heap scope level " << level << " address\n";
        }
        passed_scopes++;
    }
    
    // If more than 3 scopes, pass additional ones on stack
    if (parent_info->scopes.size() > 3) {
        asm_code << "    ; Passing additional scope addresses on stack\n";
        for (size_t level = 3; level < parent_info->scopes.size(); ++level) {
            const auto& scope = parent_info->scopes[level];
            if (scope->is_stack_allocated) {
                asm_code << "    lea rax, [rbp - " << (level * 8 + 8) << "]\n";
                asm_code << "    push rax  ; push stack scope level " << level << " address\n";
            } else {
                asm_code << "    mov rax, [rbp - " << (level * 8 + 8) << "]\n";
                asm_code << "    push rax  ; push heap scope level " << level << " address\n";
            }
        }
    }
    
    return asm_code.str();
}

// ASSEMBLY GENERATION: Generate assembly for callback scope capture
std::string LexicalScopeManager::generate_callback_scope_capture_asm(FunctionExpression* callback_func,
                                                                    const std::vector<int>& needed_scope_levels) const {
    std::cout << "[LexicalScopeManager] COMPILE-TIME: Generating callback scope capture assembly" << std::endl;
    
    std::ostringstream asm_code;
    asm_code << "; CALLBACK LEXICAL SCOPE CAPTURE\n";
    asm_code << "; This callback needs access to " << needed_scope_levels.size() << " parent scope levels\n";
    
    // TODO: Complex callback handling - need to store scope addresses with callback
    // This would create a closure-like structure containing both function pointer and scope addresses
    
    for (int level : needed_scope_levels) {
        asm_code << "; Capture scope level " << level << " address for callback\n";
        // Store scope address in callback closure structure
        asm_code << "    ; TODO: Store scope level " << level << " address in callback closure\n";
    }
    
    return asm_code.str();
}

// COMPILE-TIME: Check if any scope in the function needs heap allocation
bool LexicalScopeManager::function_needs_heap_allocation(FunctionExpression* func) const {
    const FunctionScopeInfo* info = get_function_scope_info(func);
    if (!info) return false;
    
    // Check if any scope level is marked for heap allocation
    for (const auto& scope : info->scopes) {
        if (scope->allocate_on_heap) {
            std::cout << "[LexicalScopeManager] COMPILE-TIME: Function needs heap allocation (scope level " 
                     << scope->scope_level << " escaped)" << std::endl;
            return true;
        }
    }
    
    std::cout << "[LexicalScopeManager] COMPILE-TIME: Function uses stack-only allocation (no escapes)" << std::endl;
    return false;
}

// COMPILE-TIME: Get total heap size needed for all scopes that escape  
size_t LexicalScopeManager::get_total_heap_size(FunctionExpression* func) const {
    const FunctionScopeInfo* info = get_function_scope_info(func);
    if (!info) return 0;
    
    size_t total_size = 0;
    for (const auto& scope : info->scopes) {
        if (scope->allocate_on_heap) {
            total_size += scope->total_size;
            std::cout << "[LexicalScopeManager] COMPILE-TIME: Scope level " << scope->scope_level 
                     << " needs " << scope->total_size << " bytes on heap" << std::endl;
        }
    }
    
    std::cout << "[LexicalScopeManager] COMPILE-TIME: Total heap size needed: " << total_size << " bytes" << std::endl;
    return total_size;
}
