#include "lexical_scope_address_tracker.h"
#include <iostream>
#include <algorithm>
#include <sstream>
#include <unordered_set>

// *** COMPILE-TIME SCOPE ADDRESS TRACKER ***
// Converts escape analysis results into scope address passing strategies

// EscapeConsumer implementation - called when a variable escapes to a goroutine
void LexicalScopeAddressTracker::on_variable_escaped(const std::string& var_name, 
                                                    FunctionExpression* capturing_func,
                                                    const std::string& var_type) {
    std::cout << "[LexicalScopeAddressTracker] COMPILE-TIME: Variable '" << var_name << "' (type: " << var_type << ") escapes to: " << capturing_func << std::endl;
    
    // For now, we'll create a temporary function for the parent scope
    // TODO: Improve this by properly tracking the parent function context
    FunctionExpression* parent_func = current_parent_function_;
    if (!parent_func) {
        std::cout << "[LexicalScopeAddressTracker] WARNING: No parent function context, using capturing function as placeholder" << std::endl;
        parent_func = capturing_func; // Use capturing function as a placeholder for now
    }
    
    std::cout << "[LexicalScopeAddressTracker] Adding variable '" << var_name << "' to parent function scope" << std::endl;
    
    // Add the variable to the parent function's scope at level 0 (where it's declared)
    scope_manager_->add_variable_to_scope(parent_func, 0, var_name, var_type);
    
    // Mark the variable as escaped in the parent function's scope
    scope_manager_->mark_variable_as_escaped(parent_func, var_name);
    
    // CRITICAL FIX: Also add the variable to the capturing function's scope at level 1 (parent variable)
    if (capturing_func != parent_func) {
        std::cout << "[LexicalScopeAddressTracker] ALSO adding variable '" << var_name << "' to capturing function scope as parent variable" << std::endl;
        scope_manager_->add_variable_to_scope(capturing_func, 1, var_name, var_type);
        scope_manager_->mark_variable_as_escaped(capturing_func, var_name);
    }
    
    // Calculate the scope layouts for both functions
    scope_manager_->calculate_scope_layouts(parent_func);
    scope_manager_->allocate_scope_registers(parent_func);
    
    if (capturing_func != parent_func) {
        scope_manager_->calculate_scope_layouts(capturing_func);
        scope_manager_->allocate_scope_registers(capturing_func);
    }
    
    // Store the escaped variable in our goroutine scope info
    GoroutineScopeInfo info;
    info.goroutine = nullptr;  // TODO: Get the actual goroutine expression
    info.parent_function = parent_func;  // Store the parent function (where variable is declared)
    info.captured_variables.push_back(var_name);
    info.needed_scope_levels.push_back(0); // Variable is in parent's current scope (level 0)
    
    goroutine_scope_info_.push_back(info);
    
    std::cout << "[LexicalScopeAddressTracker] REGISTERED: Added variable '" << var_name << "' to parent scope layout" << std::endl;
    std::cout << "[LexicalScopeAddressTracker] TOTAL_CAPTURED: " << goroutine_scope_info_.size() << " goroutine scope entries" << std::endl;
}

// EscapeConsumer implementation - analysis lifecycle
void LexicalScopeAddressTracker::on_function_analysis_start(FunctionExpression* func) {
    std::cout << "[LexicalScopeAddressTracker] COMPILE-TIME: Starting analysis for function: " << func << std::endl;
    // Initialize function scope info if needed
}

void LexicalScopeAddressTracker::on_function_analysis_complete(FunctionExpression* func) {
    std::cout << "[LexicalScopeAddressTracker] COMPILE-TIME: Completed analysis for function: " << func << std::endl;
    // Finalize scope address calculations
}

// COMPILE-TIME: Set current parent function context for escape analysis
void LexicalScopeAddressTracker::set_current_parent_function(FunctionExpression* parent_func) {
    current_parent_function_ = parent_func;
    std::cout << "[LexicalScopeAddressTracker] COMPILE-TIME: Set current parent function context: " << parent_func << std::endl;
}

// COMPILE-TIME: Set the current scope context (called by parser)
void LexicalScopeAddressTracker::set_current_function_scope(FunctionExpression* func, 
                                                           const std::unordered_set<std::string>& current_scope_variables) {
    std::cout << "[LexicalScopeAddressTracker] COMPILE-TIME: Setting scope context for function with " 
              << current_scope_variables.size() << " variables" << std::endl;
    
    // Create scope info for this function
    FunctionScopeInfo* scope_info = scope_manager_->create_function_scope_info(func);
    
    // Add all current scope variables to scope level 0
    for (const std::string& var_name : current_scope_variables) {
        scope_manager_->add_variable_to_scope(func, 0, var_name, "auto");  // Type will be inferred later
    }
    
    // Calculate the memory layout
    scope_manager_->calculate_scope_layouts(func);
    scope_manager_->allocate_scope_registers(func);
}

// COMPILE-TIME: Register a goroutine and its lexical scope needs
void LexicalScopeAddressTracker::register_goroutine_scope_capture(GoroutineExpression* goroutine, 
                                                                 FunctionExpression* parent_function,
                                                                 const std::vector<std::string>& captured_vars) {
    std::cout << "[LexicalScopeAddressTracker] COMPILE-TIME: Registering goroutine with " 
              << captured_vars.size() << " captured variables" << std::endl;
    
    GoroutineScopeInfo info;
    info.goroutine = goroutine;
    info.parent_function = parent_function;
    info.captured_variables = captured_vars;
    info.needed_scope_levels = analyze_goroutine_scope_requirements(captured_vars, parent_function);
    
    goroutine_scope_info_.push_back(info);
    
    // Print analysis results
    for (const std::string& var : captured_vars) {
        std::cout << "[LexicalScopeAddressTracker] COMPILE-TIME: Goroutine captures variable: " << var << std::endl;
    }
    for (int level : info.needed_scope_levels) {
        std::cout << "[LexicalScopeAddressTracker] COMPILE-TIME: Goroutine needs scope level: " << level << std::endl;
    }
}

// COMPILE-TIME: Calculate optimal scope address passing for all functions
void LexicalScopeAddressTracker::calculate_all_scope_address_strategies() {
    std::cout << "[LexicalScopeAddressTracker] COMPILE-TIME: Calculating scope address strategies..." << std::endl;
    
    // Analyze which functions need which parent scope addresses
    calculate_scope_propagation_requirements();
    
    // Generate the final scope address allocation
    // This determines which registers (r12, r13, r14) hold which scope addresses
    std::cout << "[LexicalScopeAddressTracker] COMPILE-TIME: Scope address analysis complete!" << std::endl;
}

// ASSEMBLY GENERATION: Generate assembly for variable access in goroutines
std::string LexicalScopeAddressTracker::generate_goroutine_variable_access_asm(GoroutineExpression* goroutine, 
                                                                              const std::string& var_name) const {
    // Find the goroutine scope info
    for (const auto& info : goroutine_scope_info_) {
        if (info.goroutine == goroutine) {
            // Use the scope manager to generate the assembly
            return scope_manager_->generate_variable_access_asm(info.parent_function, var_name);
        }
    }
    
    return "; ERROR: Goroutine not found for variable access: " + var_name;
}

// ASSEMBLY GENERATION: Generate assembly for setting up scope addresses when calling goroutines
std::string LexicalScopeAddressTracker::generate_goroutine_scope_setup_asm(GoroutineExpression* goroutine) const {
    // Find the goroutine scope info
    for (const auto& info : goroutine_scope_info_) {
        if (info.goroutine == goroutine) {
            std::ostringstream asm_code;
            asm_code << "; === GOROUTINE SCOPE ADDRESS SETUP (COMPILE-TIME GENERATED) ===" << std::endl;
            
            // Generate assembly to pass needed scope addresses to the goroutine
            // The goroutine function will receive these addresses in its parameters
            for (int scope_level : info.needed_scope_levels) {
                if (scope_level == 0) {
                    // Current scope address (from r15)
                    asm_code << "mov rdi, r15  ; pass current scope address as parameter" << std::endl;
                } else {
                    // Parent scope addresses (from r12, r13, r14 or stack)
                    const FunctionScopeInfo* parent_info = scope_manager_->get_function_scope_info(info.parent_function);
                    if (parent_info) {
                        std::string scope_register;
                        for (const auto& reg_info : parent_info->scope_registers) {
                            if (reg_info.scope_level == scope_level) {
                                scope_register = reg_info.register_name;
                                break;
                            }
                        }
                        
                        if (!scope_register.empty()) {
                            asm_code << "mov rsi, " << scope_register << "  ; pass scope level " << scope_level 
                                     << " address as parameter" << std::endl;
                        } else {
                            // Fallback to stack access
                            int stack_offset = -8 * scope_level;
                            asm_code << "mov rsi, [rbp " << stack_offset << "]  ; pass scope level " << scope_level 
                                     << " address from stack" << std::endl;
                        }
                    }
                }
            }
            
            return asm_code.str();
        }
    }
    
    return "; ERROR: Goroutine not found for scope setup";
}

// DEBUG: Print all scope address tracking information
void LexicalScopeAddressTracker::print_scope_address_analysis() const {
    std::cout << "\n[LexicalScopeAddressTracker] COMPILE-TIME ANALYSIS RESULTS:" << std::endl;
    std::cout << "============================================================" << std::endl;
    
    for (const auto& info : goroutine_scope_info_) {
        std::cout << "\nGoroutine: " << info.goroutine << std::endl;
        std::cout << "Parent Function: " << info.parent_function << std::endl;
        std::cout << "Captured Variables: ";
        for (const std::string& var : info.captured_variables) {
            std::cout << var << " ";
        }
        std::cout << std::endl;
        std::cout << "Needed Scope Levels: ";
        for (int level : info.needed_scope_levels) {
            std::cout << level << " ";
        }
        std::cout << std::endl;
    }
    
    std::cout << "\nScope Manager Analysis:" << std::endl;
    scope_manager_->print_all_scope_info();
    
    std::cout << "============================================================" << std::endl;
}

// COMPILE-TIME: Analyze which scope levels a goroutine needs
std::vector<int> LexicalScopeAddressTracker::analyze_goroutine_scope_requirements(const std::vector<std::string>& captured_vars,
                                                                                 FunctionExpression* parent_function) const {
    std::vector<int> needed_levels;
    
    // For now, assume all captured variables are from the current scope (level 0)
    // This will be enhanced when we have proper scope level analysis
    if (!captured_vars.empty()) {
        needed_levels.push_back(0);  // Current scope
    }
    
    std::cout << "[LexicalScopeAddressTracker] COMPILE-TIME: Goroutine needs " << needed_levels.size() 
              << " scope levels" << std::endl;
    
    return needed_levels;
}

// COMPILE-TIME: Calculate which parent scope levels need to be passed down through call chain
void LexicalScopeAddressTracker::calculate_scope_propagation_requirements() {
    std::cout << "[LexicalScopeAddressTracker] COMPILE-TIME: Calculating scope propagation requirements..." << std::endl;
    
    // This is where we determine which intermediate functions need to pass scope addresses
    // even if they don't use them directly (for their children that do need them)
    
    // For now, mark all parent functions as needing their current scope
    for (const auto& info : goroutine_scope_info_) {
        function_needed_scopes_[info.parent_function] = info.needed_scope_levels;
    }
    
    std::cout << "[LexicalScopeAddressTracker] COMPILE-TIME: Scope propagation analysis complete" << std::endl;
}

// SIMPLIFIED API: Generate assembly for variable access (for integration with ast_codegen)
std::vector<std::string> LexicalScopeAddressTracker::generate_goroutine_variable_access_asm(const std::string& var_name) const {
    std::vector<std::string> asm_code;
    
    std::cout << "[SCOPE_ASM_DEBUG] Generating assembly for variable: " << var_name << std::endl;
    
    // Find the variable in our captured variables list
    const ScopeVariable* var_info = nullptr;
    std::string register_name = "r12";  // Default to r12 for parent scope
    
    for (const auto& info : goroutine_scope_info_) {
        for (const std::string& captured_var : info.captured_variables) {
            if (captured_var == var_name) {
                // Get the actual variable information from the scope manager
                if (info.parent_function) {
                    const FunctionScopeInfo* scope_info = scope_manager_->get_function_scope_info(info.parent_function);
                    if (scope_info) {
                        var_info = scope_info->find_variable(var_name);
                    }
                }
                break;
            }
        }
        if (var_info) break;
    }
    
    if (!var_info) {
        std::cout << "[SCOPE_ASM_DEBUG] ERROR: Variable '" << var_name << "' not found in scope info" << std::endl;
        asm_code.push_back("; ERROR: Variable " + var_name + " not found");
        return asm_code;
    }
    
    // Determine which register to use based on scope level
    if (var_info->scope_level == 0) {
        register_name = "r15";  // Current scope (though this shouldn't happen for escaped vars)
    } else if (var_info->scope_level == 1) {
        register_name = "r12";  // Parent scope
    } else if (var_info->scope_level == 2) {
        register_name = "r13";  // Grandparent scope
    } else if (var_info->scope_level == 3) {
        register_name = "r14";  // Great-grandparent scope
    } else {
        // Fallback to stack access for deeper levels
        register_name = "rbp";  // Will need special handling
    }
    
    std::cout << "[SCOPE_ASM_DEBUG] Strategy: Load from " << register_name << " with offset " 
              << var_info->offset << " (scope level " << var_info->scope_level << ")" << std::endl;
    
    if (register_name == "rbp") {
        // Stack-based access for deep scopes
        int stack_offset = -8 * var_info->scope_level;
        std::string load_instruction = "mov rdx, [rbp " + std::to_string(stack_offset) + "]  ; Load scope " 
                                     + std::to_string(var_info->scope_level) + " address";
        std::string access_instruction = "mov rax, [rdx + " + std::to_string(var_info->offset) + "]  ; Load " 
                                       + var_name + " from scope";
        asm_code.push_back(load_instruction);
        asm_code.push_back(access_instruction);
    } else {
        // Direct register access - OPTIMAL PATH
        std::string load_instruction = "mov rax, [" + register_name + " + " + std::to_string(var_info->offset) 
                                     + "]  ; Load " + var_name + " from scope level " 
                                     + std::to_string(var_info->scope_level);
        asm_code.push_back(load_instruction);
    }
    
    std::string comment_instruction = "; Variable " + var_name + " accessed from lexical scope (COMPILE-TIME OPTIMIZED)";
    asm_code.push_back(comment_instruction);
    
    std::cout << "[SCOPE_ASM_DEBUG] Generated " << asm_code.size() << " assembly instructions for " << var_name << std::endl;
    
    return asm_code;
}

// SIMPLIFIED API: Generate assembly for variable assignment (for integration with ast_codegen)
std::vector<std::string> LexicalScopeAddressTracker::generate_goroutine_variable_assignment_asm(const std::string& var_name) const {
    std::vector<std::string> asm_code;
    
    std::cout << "[SCOPE_ASM_DEBUG] Generating assignment assembly for variable: " << var_name << std::endl;
    
    // Find the variable in our captured variables list
    const ScopeVariable* var_info = nullptr;
    std::string register_name = "r12";  // Default to r12 for parent scope
    
    for (const auto& info : goroutine_scope_info_) {
        for (const std::string& captured_var : info.captured_variables) {
            if (captured_var == var_name) {
                // Get the actual variable information from the scope manager
                if (info.parent_function) {
                    const FunctionScopeInfo* scope_info = scope_manager_->get_function_scope_info(info.parent_function);
                    if (scope_info) {
                        var_info = scope_info->find_variable(var_name);
                    }
                }
                break;
            }
        }
        if (var_info) break;
    }
    
    if (!var_info) {
        std::cout << "[SCOPE_ASM_DEBUG] ERROR: Variable '" << var_name << "' not found in scope info for assignment" << std::endl;
        asm_code.push_back("; ERROR: Variable " + var_name + " not found for assignment");
        return asm_code;
    }
    
    // Determine which register to use based on scope level
    if (var_info->scope_level == 0) {
        register_name = "r15";  // Current scope (though this shouldn't happen for escaped vars)
    } else if (var_info->scope_level == 1) {
        register_name = "r12";  // Parent scope
    } else if (var_info->scope_level == 2) {
        register_name = "r13";  // Grandparent scope
    } else if (var_info->scope_level == 3) {
        register_name = "r14";  // Great-grandparent scope
    } else {
        // Fallback to stack access for deeper levels
        register_name = "rbp";  // Will need special handling
    }
    
    std::cout << "[SCOPE_ASM_DEBUG] Strategy: Store to " << register_name << " with offset " 
              << var_info->offset << " (scope level " << var_info->scope_level << ")" << std::endl;
    
    if (register_name == "rbp") {
        // Stack-based access for deep scopes
        int stack_offset = -8 * var_info->scope_level;
        std::string load_instruction = "mov rdx, [rbp " + std::to_string(stack_offset) + "]  ; Load scope " 
                                     + std::to_string(var_info->scope_level) + " address";
        std::string store_instruction = "mov [rdx + " + std::to_string(var_info->offset) + "], rax  ; Store " 
                                      + var_name + " to scope";
        asm_code.push_back(load_instruction);
        asm_code.push_back(store_instruction);
    } else {
        // Direct register access - OPTIMAL PATH
        std::string store_instruction = "mov [" + register_name + " + " + std::to_string(var_info->offset) 
                                      + "], rax  ; Store " + var_name + " to scope level " 
                                      + std::to_string(var_info->scope_level);
        asm_code.push_back(store_instruction);
    }
    
    std::string comment_instruction = "; Variable " + var_name + " assigned to lexical scope (COMPILE-TIME OPTIMIZED)";
    asm_code.push_back(comment_instruction);
    
    std::cout << "[SCOPE_ASM_DEBUG] Generated " << asm_code.size() << " assignment instructions for " << var_name << std::endl;
    
    return asm_code;
}

// QUERY API: Check if a variable is captured by goroutines
bool LexicalScopeAddressTracker::is_variable_captured(const std::string& var_name) const {
    std::cout << "[SCOPE_TRACKER_DEBUG] Checking if variable '" << var_name << "' is captured..." << std::endl;
    std::cout << "[SCOPE_TRACKER_DEBUG] Total goroutine scope info entries: " << goroutine_scope_info_.size() << std::endl;
    
    // Check if this variable appears in any of our goroutine scope captures
    for (size_t i = 0; i < goroutine_scope_info_.size(); ++i) {
        const auto& info = goroutine_scope_info_[i];
        std::cout << "[SCOPE_TRACKER_DEBUG] Checking goroutine info " << i << " with " << info.captured_variables.size() << " captured variables" << std::endl;
        
        for (size_t j = 0; j < info.captured_variables.size(); ++j) {
            std::cout << "[SCOPE_TRACKER_DEBUG]   - Captured var " << j << ": '" << info.captured_variables[j] << "'" << std::endl;
            if (info.captured_variables[j] == var_name) {
                std::cout << "[SCOPE_TRACKER_DEBUG] MATCH FOUND! Variable '" << var_name << "' is captured!" << std::endl;
                return true;
            }
        }
    }
    
    std::cout << "[SCOPE_TRACKER_DEBUG] Variable '" << var_name << "' is NOT captured" << std::endl;
    return false;
}

// COMPILE-TIME STATIC ANALYSIS: Register when a variable is declared in a function
void LexicalScopeAddressTracker::register_variable_declaration(const std::string& var_name, FunctionExpression* declaring_func, const std::string& var_type) {
    std::cout << "[STATIC_ANALYSIS] Registering variable declaration: '" << var_name << "' in function " << declaring_func << std::endl;
    
    VariableDeclarationInfo info;
    info.var_name = var_name;
    info.declaring_func = declaring_func;
    info.var_type = var_type;
    info.offset = 0; // Will be calculated by scope manager
    
    variable_declarations_[var_name] = info;
}

// COMPILE-TIME STATIC ANALYSIS: Determine which scope level a variable belongs to when accessed from a function
int LexicalScopeAddressTracker::determine_variable_scope_level(const std::string& var_name, FunctionExpression* accessing_func) const {
    auto it = variable_declarations_.find(var_name);
    if (it == variable_declarations_.end()) {
        std::cout << "[STATIC_ANALYSIS] WARNING: Variable '" << var_name << "' not found in declarations, assuming current scope (level 0)" << std::endl;
        return 0; // Default to current scope
    }
    
    FunctionExpression* declaring_func = it->second.declaring_func;
    
    // If the variable is declared in the same function we're accessing it from, it's scope level 0 (current scope)
    if (declaring_func == accessing_func) {
        std::cout << "[STATIC_ANALYSIS] Variable '" << var_name << "' is declared in current function (scope level 0)" << std::endl;
        return 0;
    }
    
    // If the variable is declared in a different function, it's scope level 1 (parent scope)
    // In more complex scenarios, we'd need to walk the function nesting tree, but for our current case this is sufficient
    std::cout << "[STATIC_ANALYSIS] Variable '" << var_name << "' is declared in parent function (scope level 1)" << std::endl;
    return 1;
}

// COMPILE-TIME STATIC ANALYSIS: Get the register name for a given scope level
std::string LexicalScopeAddressTracker::get_register_for_scope_level(int scope_level) const {
    switch (scope_level) {
        case 0: return "r15";  // Current scope
        case 1: return "r12";  // Parent scope  
        case 2: return "r13";  // Grandparent scope
        case 3: return "r14";  // Great-grandparent scope
        default: return "rbp"; // Stack fallback for deeper nesting
    }
}

// COMPILE-TIME STATIC ANALYSIS: Generate variable access assembly using static analysis
std::vector<std::string> LexicalScopeAddressTracker::generate_variable_access_asm_with_static_analysis(const std::string& var_name, FunctionExpression* accessing_func) const {
    std::vector<std::string> asm_code;
    
    // Determine the scope level using static analysis
    int scope_level = determine_variable_scope_level(var_name, accessing_func);
    std::string base_register = get_register_for_scope_level(scope_level);
    
    // Get variable offset from the lexical scope manager
    size_t offset = 0;
    auto it = variable_declarations_.find(var_name);
    if (it != variable_declarations_.end()) {
        offset = it->second.offset;
    }
    
    std::cout << "[STATIC_ANALYSIS] Generating access for '" << var_name << "': scope_level=" << scope_level 
              << ", register=" << base_register << ", offset=" << offset << std::endl;
    
    // Generate the assembly
    asm_code.push_back("; STATIC ANALYSIS: Variable '" + var_name + "' from scope level " + std::to_string(scope_level));
    asm_code.push_back("mov rax, [" + base_register + " + " + std::to_string(offset) + "]  ; " + var_name + " (scope level " + std::to_string(scope_level) + ")");
    
    return asm_code;
}
