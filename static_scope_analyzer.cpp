#include "static_scope_analyzer.h"
#include "compiler.h"
#include <iostream>

// ============================================================================
// STATIC SCOPE ANALYZER IMPLEMENTATION
// Pure compile-time analysis to determine lexical scope requirements
// ============================================================================

StaticScopeAnalyzer::StaticScopeAnalyzer() {
    std::cout << "[DEBUG] StaticScopeAnalyzer: Created with pure static analysis" << std::endl;
}

void StaticScopeAnalyzer::analyze_function(const std::string& function_name, ASTNode* function_node) {
    std::cout << "[DEBUG] StaticScopeAnalyzer: Analyzing function '" << function_name << "'" << std::endl;
    
    FunctionScopeAnalysis analysis;
    analysis.function_name = function_name;
    analysis.has_escaping_variables = false;  // Start conservative
    analysis.total_stack_space_needed = 0;
    analysis.total_heap_scope_size = 0;
    
    if (!function_node) {
        std::cout << "[DEBUG] StaticScopeAnalyzer: Function node is null, using defaults" << std::endl;
        function_analyses_[function_name] = analysis;
        return;
    }
    
    // 1. Build scope hierarchy and find all variable declarations
    build_scope_hierarchy(function_node);
    
    // 2. Find all variables referenced by goroutines
    std::unordered_set<std::string> goroutine_captured_vars;
    find_goroutine_captures(function_node, goroutine_captured_vars);
    
    // 3. Find all variables captured by callbacks/closures
    std::unordered_set<std::string> callback_captured_vars;
    find_callback_captures(function_node, callback_captured_vars);
    
    // 4. Determine if any variables escape (are used by goroutines or callbacks)
    for (const auto& var : goroutine_captured_vars) {
        analysis.has_escaping_variables = true;
        analysis.total_heap_scope_size += 8;  // Assume 8 bytes per variable for now
        std::cout << "[DEBUG] StaticScopeAnalyzer: Variable '" << var << "' escapes via goroutine" << std::endl;
    }
    
    for (const auto& var : callback_captured_vars) {
        analysis.has_escaping_variables = true;
        analysis.total_heap_scope_size += 8;  // Assume 8 bytes per variable for now
        std::cout << "[DEBUG] StaticScopeAnalyzer: Variable '" << var << "' escapes via callback" << std::endl;
    }
    
    // 5. Calculate memory layouts and register allocation
    calculate_memory_layouts(function_name);
    
    // 6. Determine optimal register allocation for scope pointers
    determine_register_allocation(function_name);
    
    // Store the analysis
    function_analyses_[function_name] = analysis;
    
    std::cout << "[DEBUG] StaticScopeAnalyzer: Function '" << function_name 
              << "' has_escaping_variables=" << analysis.has_escaping_variables 
              << ", heap_size=" << analysis.total_heap_scope_size << std::endl;
}

void StaticScopeAnalyzer::build_scope_hierarchy(ASTNode* function_node) {
    std::cout << "[DEBUG] StaticScopeAnalyzer: Building scope hierarchy with full AST walking" << std::endl;
    
    if (!function_node) return;
    
    // Walk the AST and identify all scopes and variable declarations
    current_scope_level_ = 0;
    walk_ast_for_scopes(function_node);
}

void StaticScopeAnalyzer::walk_ast_for_scopes(ASTNode* node) {
    if (!node) return;
    
    // Handle different node types for scope analysis
    if (auto* func_expr = dynamic_cast<FunctionExpression*>(node)) {
        // Nested function creates new scope
        current_scope_level_++;
        
        // Analyze parameters as local variables
        for (const auto& param : func_expr->parameters) {
            add_variable_to_scope(param.name, current_scope_level_, param.type);
        }
        
        // Analyze function body
        for (const auto& stmt : func_expr->body) {
            walk_ast_for_scopes(stmt.get());
        }
        
        current_scope_level_--;
    }
    else if (auto* assignment = dynamic_cast<Assignment*>(node)) {
        // Variable assignment/declaration - add to current scope
        add_variable_to_scope(assignment->variable_name, current_scope_level_, assignment->declared_type);
        
        // Also analyze the assigned expression
        if (assignment->value) {
            walk_ast_for_scopes(assignment->value.get());
        }
    }
    else if (auto* identifier = dynamic_cast<Identifier*>(node)) {
        // Variable usage - record for escape analysis
        record_variable_usage(identifier->name, current_scope_level_);
    }
    else if (auto* func_call = dynamic_cast<FunctionCall*>(node)) {
        // Check if this is a goroutine call
        if (func_call->is_goroutine) {
            current_goroutine_depth_++;
            for (const auto& arg : func_call->arguments) {
                walk_ast_for_scopes(arg.get());
            }
            current_goroutine_depth_--;
        } else {
            for (const auto& arg : func_call->arguments) {
                walk_ast_for_scopes(arg.get());
            }
        }
    }
    else if (auto* method_call = dynamic_cast<MethodCall*>(node)) {
        // Check if this is a goroutine method call
        if (method_call->is_goroutine) {
            current_goroutine_depth_++;
            for (const auto& arg : method_call->arguments) {
                walk_ast_for_scopes(arg.get());
            }
            current_goroutine_depth_--;
        } else {
            for (const auto& arg : method_call->arguments) {
                walk_ast_for_scopes(arg.get());
            }
        }
    }
    else if (auto* binary_op = dynamic_cast<BinaryOp*>(node)) {
        walk_ast_for_scopes(binary_op->left.get());
        walk_ast_for_scopes(binary_op->right.get());
    }
    else if (auto* ternary = dynamic_cast<TernaryOperator*>(node)) {
        walk_ast_for_scopes(ternary->condition.get());
        walk_ast_for_scopes(ternary->true_expr.get());
        walk_ast_for_scopes(ternary->false_expr.get());
    }
    else if (auto* if_stmt = dynamic_cast<IfStatement*>(node)) {
        // If statement - analyze condition and both branches
        walk_ast_for_scopes(if_stmt->condition.get());
        for (const auto& stmt : if_stmt->then_body) {
            walk_ast_for_scopes(stmt.get());
        }
        for (const auto& stmt : if_stmt->else_body) {
            walk_ast_for_scopes(stmt.get());
        }
    }
    else if (auto* for_loop = dynamic_cast<ForLoop*>(node)) {
        // For loop - analyze init, condition, update, and body
        walk_ast_for_scopes(for_loop->init.get());
        walk_ast_for_scopes(for_loop->condition.get());
        walk_ast_for_scopes(for_loop->update.get());
        for (const auto& stmt : for_loop->body) {
            walk_ast_for_scopes(stmt.get());
        }
    }
    else if (auto* foreach_loop = dynamic_cast<ForEachLoop*>(node)) {
        // ForEach loop - analyze iterable and body, add loop variables to scope
        walk_ast_for_scopes(foreach_loop->iterable.get());
        
        // Add loop variables to current scope
        add_variable_to_scope(foreach_loop->index_var_name, current_scope_level_, DataType::INT64);
        add_variable_to_scope(foreach_loop->value_var_name, current_scope_level_, DataType::ANY);
        
        for (const auto& stmt : foreach_loop->body) {
            walk_ast_for_scopes(stmt.get());
        }
    }
    else if (auto* forin_stmt = dynamic_cast<ForInStatement*>(node)) {
        // ForIn statement - analyze object and body, add key variable to scope
        walk_ast_for_scopes(forin_stmt->object.get());
        
        // Add key variable to current scope
        add_variable_to_scope(forin_stmt->key_var_name, current_scope_level_, DataType::STRING);
        
        for (const auto& stmt : forin_stmt->body) {
            walk_ast_for_scopes(stmt.get());
        }
    }
    else if (auto* return_stmt = dynamic_cast<ReturnStatement*>(node)) {
        // Return statement - analyze return value expression
        if (return_stmt->value) {
            walk_ast_for_scopes(return_stmt->value.get());
        }
    }
}

void StaticScopeAnalyzer::add_variable_to_scope(const std::string& name, int scope_level, DataType type) {
    LexicalScopeInfo info;
    info.variable_name = name;
    info.scope_level = scope_level;
    info.offset_in_scope = 0; // Will be calculated later
    info.escapes_current_function = false; // Will be determined by usage analysis
    info.type = type;
    info.size_bytes = 8; // Default size
    
    variable_scope_map_[name] = info;
    std::cout << "[DEBUG] StaticScopeAnalyzer: Variable '" << name << "' declared at scope level " << scope_level << std::endl;
}

void StaticScopeAnalyzer::record_variable_usage(const std::string& name, int usage_scope_level) {
    auto it = variable_scope_map_.find(name);
    if (it != variable_scope_map_.end()) {
        // Check if variable is used in different scope than declaration
        if (usage_scope_level != it->second.scope_level || current_goroutine_depth_ > 0) {
            it->second.escapes_current_function = true;
            std::cout << "[DEBUG] StaticScopeAnalyzer: Variable '" << name << "' escapes (used in scope " 
                      << usage_scope_level << ", declared in scope " << it->second.scope_level 
                      << ", goroutine_depth=" << current_goroutine_depth_ << ")" << std::endl;
        }
    } else {
        // Variable not found in local scope - might be from parent scope
        std::cout << "[DEBUG] StaticScopeAnalyzer: Variable '" << name << "' not found in local scope (parent scope access)" << std::endl;
    }
}

void StaticScopeAnalyzer::analyze_variable_declarations(ASTNode* node) {
    std::cout << "[DEBUG] StaticScopeAnalyzer: Analyzing variable declarations with AST walking" << std::endl;
    walk_ast_for_scopes(node);
}

void StaticScopeAnalyzer::find_goroutine_captures(ASTNode* node, std::unordered_set<std::string>& captured_vars) {
    std::cout << "[DEBUG] StaticScopeAnalyzer: Finding goroutine captures with full AST analysis" << std::endl;
    
    // Use the variable_scope_map_ built during scope hierarchy analysis
    for (const auto& [name, info] : variable_scope_map_) {
        if (info.escapes_current_function) {
            captured_vars.insert(name);
            std::cout << "[DEBUG] StaticScopeAnalyzer: Variable '" << name << "' captured by goroutine/escape" << std::endl;
        }
    }
}

void StaticScopeAnalyzer::find_callback_captures(ASTNode* node, std::unordered_set<std::string>& captured_vars) {
    std::cout << "[DEBUG] StaticScopeAnalyzer: Finding callback captures (using same escape analysis as goroutines)" << std::endl;
    
    // For callbacks/closures, use the same escape analysis as goroutines
    // Variables captured by callbacks also need to escape to heap scope
    for (const auto& [name, info] : variable_scope_map_) {
        if (info.escapes_current_function) {
            captured_vars.insert(name);
            std::cout << "[DEBUG] StaticScopeAnalyzer: Variable '" << name << "' captured by callback/escape" << std::endl;
        }
    }
}

void StaticScopeAnalyzer::determine_register_allocation(const std::string& function_name) {
    std::cout << "[DEBUG] StaticScopeAnalyzer: Determining register allocation for " << function_name << std::endl;
    
    auto it = function_analyses_.find(function_name);
    if (it == function_analyses_.end()) {
        std::cout << "[DEBUG] StaticScopeAnalyzer: No analysis found for function " << function_name << std::endl;
        return;
    }
    
    FunctionScopeAnalysis& analysis = it->second;
    
    // Available callee-saved registers for scope storage: R12, R13, R14, R15, RBX
    std::vector<int> available_registers = {12, 13, 14, 15, 3}; // R12, R13, R14, R15, RBX
    
    // Find all scope levels that are actually accessed in this function
    std::unordered_set<int> accessed_scope_levels;
    for (const auto& [var_name, var_info] : analysis.variables) {
        if (var_info.escapes_current_function) {
            accessed_scope_levels.insert(var_info.scope_level);
        }
    }
    
    std::cout << "[DEBUG] StaticScopeAnalyzer: Function '" << function_name 
              << "' accesses " << accessed_scope_levels.size() << " scope levels" << std::endl;
    
    // SMART ALLOCATION: Only allocate registers for scopes that are actually used
    int register_index = 0;
    for (int scope_level : accessed_scope_levels) {
        if (register_index < available_registers.size()) {
            int assigned_register = available_registers[register_index];
            analysis.scope_level_to_register[scope_level] = assigned_register;
            analysis.used_scope_registers.insert(assigned_register);
            
            std::cout << "[DEBUG] StaticScopeAnalyzer: Scope level " << scope_level 
                      << " assigned to register " << assigned_register << std::endl;
            register_index++;
        } else {
            // More than 5 scope levels - use stack fallback for deeper scopes
            analysis.needs_stack_fallback = true;
            std::cout << "[DEBUG] StaticScopeAnalyzer: Scope level " << scope_level 
                      << " uses stack fallback (too many scopes)" << std::endl;
        }
    }
    
    std::cout << "[DEBUG] StaticScopeAnalyzer: Register allocation complete. Used " 
              << analysis.used_scope_registers.size() << " registers, stack_fallback=" 
              << analysis.needs_stack_fallback << std::endl;
}

void StaticScopeAnalyzer::calculate_memory_layouts(const std::string& function_name) {
    std::cout << "[DEBUG] StaticScopeAnalyzer: Calculating memory layouts for " << function_name << std::endl;
    
    // Get the analysis for this function
    auto it = function_analyses_.find(function_name);
    if (it == function_analyses_.end()) {
        std::cout << "[DEBUG] StaticScopeAnalyzer: No analysis found for function " << function_name << std::endl;
        return;
    }
    
    FunctionScopeAnalysis& analysis = it->second;
    
    // For each captured variable, determine memory layout
    int64_t heap_offset = 0;
    int64_t stack_offset = -8;  // Start at -8 from RBP
    
    // For captured variables, create entries in the variables map
    // TODO: This would be populated from actual AST analysis
    for (int i = 0; i < static_cast<int>(analysis.total_heap_scope_size / 8); ++i) {
        std::string var_name = "captured_var_" + std::to_string(i);
        LexicalScopeInfo var_info;
        var_info.variable_name = var_name;
        var_info.scope_level = 0;
        var_info.offset_in_scope = heap_offset;
        var_info.escapes_current_function = true;
        var_info.type = DataType::UNKNOWN;
        var_info.size_bytes = 8;
        
        analysis.variables[var_name] = var_info;
        heap_offset += 8;  // Each variable takes 8 bytes
    }
    
    // Non-captured variables get stack locations
    // TODO: Analyze all local variables and assign stack offsets
}

LexicalScopeInfo StaticScopeAnalyzer::get_variable_info(const std::string& var_name) const {
    auto it = variable_scope_map_.find(var_name);
    if (it != variable_scope_map_.end()) {
        return it->second;
    }
    
    // Return default info for unknown variables
    LexicalScopeInfo default_info;
    default_info.variable_name = var_name;
    default_info.scope_level = 0;
    default_info.offset_in_scope = 0;
    default_info.escapes_current_function = false;
    default_info.type = DataType::UNKNOWN;
    default_info.size_bytes = 8;  // Default size
    
    return default_info;
}

FunctionScopeAnalysis StaticScopeAnalyzer::get_function_analysis(const std::string& function_name) const {
    auto it = function_analyses_.find(function_name);
    if (it != function_analyses_.end()) {
        return it->second;
    }
    
    // Return default analysis for unknown functions
    FunctionScopeAnalysis default_analysis;
    default_analysis.function_name = function_name;
    default_analysis.has_escaping_variables = false;
    default_analysis.total_stack_space_needed = 0;
    default_analysis.total_heap_scope_size = 0;
    return default_analysis;
}

// ============================================================================
// LEXICAL SCOPE INTEGRATION CLASS
// Bridge between static analyzer and type inference system
// ============================================================================

LexicalScopeIntegration::LexicalScopeIntegration() 
    : analyzer_(std::make_unique<StaticScopeAnalyzer>()) {
    std::cout << "[DEBUG] LexicalScopeIntegration: Created with static analyzer" << std::endl;
}

void LexicalScopeIntegration::analyze_function(const std::string& function_name, ASTNode* function_node) {
    if (analyzer_) {
        analyzer_->analyze_function(function_name, function_node);
    }
}

bool LexicalScopeIntegration::function_needs_r15_register(const std::string& function_name) const {
    if (analyzer_) {
        auto analysis = analyzer_->get_function_analysis(function_name);
        return analysis.has_escaping_variables;  // If has escaping vars, needs R15
    }
    return false;
}

bool LexicalScopeIntegration::should_use_heap_scope(const std::string& function_name) const {
    if (analyzer_) {
        auto analysis = analyzer_->get_function_analysis(function_name);
        return analysis.has_escaping_variables && analysis.total_heap_scope_size > 0;
    }
    return false;
}

std::vector<int> LexicalScopeIntegration::get_required_parent_scope_levels(const std::string& function_name) const {
    // TODO: Calculate based on static analysis
    return {};  // Return empty vector for now
}

size_t LexicalScopeIntegration::get_heap_scope_size(const std::string& function_name) const {
    // TODO: Calculate based on number of captured variables
    return 64;  // Default 64 bytes
}

bool LexicalScopeIntegration::variable_escapes(const std::string& function_name, const std::string& var_name) const {
    if (analyzer_) {
        auto analysis = analyzer_->get_function_analysis(function_name);
        auto var_it = analysis.variables.find(var_name);
        return (var_it != analysis.variables.end()) && var_it->second.escapes_current_function;
    }
    return false;
}

int64_t LexicalScopeIntegration::get_variable_offset(const std::string& function_name, const std::string& var_name) const {
    if (analyzer_) {
        auto analysis = analyzer_->get_function_analysis(function_name);
        auto var_it = analysis.variables.find(var_name);
        if (var_it != analysis.variables.end()) {
            return static_cast<int64_t>(var_it->second.offset_in_scope);
        }
    }
    return -8;  // Default stack offset
}

// HIGH-PERFORMANCE REGISTER-BASED SCOPE ACCESS METHODS
int LexicalScopeIntegration::get_register_for_scope_level(const std::string& function_name, int scope_level) const {
    if (analyzer_) {
        auto analysis = analyzer_->get_function_analysis(function_name);
        auto reg_it = analysis.scope_level_to_register.find(scope_level);
        if (reg_it != analysis.scope_level_to_register.end()) {
            return reg_it->second;
        }
    }
    return -1;  // No register assigned
}

std::unordered_set<int> LexicalScopeIntegration::get_used_scope_registers(const std::string& function_name) const {
    if (analyzer_) {
        auto analysis = analyzer_->get_function_analysis(function_name);
        return analysis.used_scope_registers;
    }
    return {};  // Empty set
}

bool LexicalScopeIntegration::needs_stack_fallback(const std::string& function_name) const {
    if (analyzer_) {
        auto analysis = analyzer_->get_function_analysis(function_name);
        return analysis.needs_stack_fallback;
    }
    return false;
}
