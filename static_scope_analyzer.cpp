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
    
    // 5. Calculate memory layouts
    calculate_memory_layouts(function_name);
    
    // Store the analysis
    function_analyses_[function_name] = analysis;
    
    std::cout << "[DEBUG] StaticScopeAnalyzer: Function '" << function_name 
              << "' has_escaping_variables=" << analysis.has_escaping_variables 
              << ", heap_size=" << analysis.total_heap_scope_size << std::endl;
}

void StaticScopeAnalyzer::build_scope_hierarchy(ASTNode* function_node) {
    // TODO: Walk the AST and build complete scope hierarchy
    // For now, we'll do basic analysis
    std::cout << "[DEBUG] StaticScopeAnalyzer: Building scope hierarchy (basic implementation)" << std::endl;
    
    // This would walk the AST tree and identify:
    // - All variable declarations
    // - All nested function definitions
    // - All block scopes
    // - Parent-child relationships between scopes
}

void StaticScopeAnalyzer::analyze_variable_declarations(ASTNode* node) {
    // TODO: Walk AST nodes and find all variable declarations
    // Mark them with scope information
    std::cout << "[DEBUG] StaticScopeAnalyzer: Analyzing variable declarations (basic implementation)" << std::endl;
    
    // This would identify:
    // - var, let, const declarations
    // - Function parameter declarations
    // - Function declarations
    // - Class declarations
}

void StaticScopeAnalyzer::find_goroutine_captures(ASTNode* node, std::unordered_set<std::string>& captured_vars) {
    // TODO: Find all variables captured by goroutines
    std::cout << "[DEBUG] StaticScopeAnalyzer: Finding goroutine captures (basic implementation)" << std::endl;
    
    // This would walk the AST and identify:
    // - go function() { ... } expressions
    // - Variables referenced inside goroutine functions
    // - Variables from parent scopes that are accessed
}

void StaticScopeAnalyzer::find_callback_captures(ASTNode* node, std::unordered_set<std::string>& captured_vars) {
    // TODO: Find all variables captured by callbacks/closures
    std::cout << "[DEBUG] StaticScopeAnalyzer: Finding callback captures (basic implementation)" << std::endl;
    
    // This would identify:
    // - Callback functions passed to setTimeout, setInterval
    // - Event handlers
    // - Promise.then/catch callbacks
    // - Any function expressions that reference parent scope variables
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
