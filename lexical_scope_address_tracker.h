#pragma once

#include "escape_analyzer.h"
#include "lexical_scope_layout.h"
#include <unordered_map>
#include <unordered_set>
#include <memory>

// Forward declarations
class FunctionExpression;
class GoroutineExpression;

// *** COMPILE-TIME SCOPE ADDRESS TRACKER ***
// This class analyzes escape events and generates scope address passing strategies
class LexicalScopeAddressTracker : public EscapeConsumer {
public:
    // Track goroutines and their lexical scope capture requirements (made public for ast_codegen access)
    struct GoroutineScopeInfo {
        GoroutineExpression* goroutine;
        FunctionExpression* parent_function;
        std::vector<std::string> captured_variables;  // Variables captured from parent scopes
        std::vector<int> needed_scope_levels;         // Which scope levels are needed
    };

private:
    std::unique_ptr<LexicalScopeManager> scope_manager_;
    
    // Track which functions need scope addresses for their children
    std::unordered_map<FunctionExpression*, std::vector<int>> function_needed_scopes_;
    
    std::vector<GoroutineScopeInfo> goroutine_scope_info_;
    
    // STATIC ANALYSIS: Track variable declarations per function for compile-time scope level determination
    struct VariableDeclarationInfo {
        std::string var_name;
        FunctionExpression* declaring_func;
        std::string var_type;
        size_t offset;  // offset within the declaring function's scope
    };
    std::unordered_map<std::string, VariableDeclarationInfo> variable_declarations_;
    
    // Current context tracking  
    FunctionExpression* current_parent_function_ = nullptr;
    
public:
    LexicalScopeAddressTracker() : scope_manager_(std::make_unique<LexicalScopeManager>()) {}
    
    // EscapeConsumer implementation - called by EscapeAnalyzer
    void on_variable_escaped(const std::string& var_name, 
                            FunctionExpression* capturing_func,
                            const std::string& var_type = "") override;
    
    // EscapeConsumer implementation - analysis lifecycle
    void on_function_analysis_start(FunctionExpression* func) override;
    void on_function_analysis_complete(FunctionExpression* func) override;
    
    // COMPILE-TIME: Set the current scope context (called by parser)
    void set_current_function_scope(FunctionExpression* func, 
                                   const std::unordered_set<std::string>& current_scope_variables);
    
    // COMPILE-TIME: Set current parent function context for escape analysis
    void set_current_parent_function(FunctionExpression* parent_func);
    
    // COMPILE-TIME: Register a goroutine and its lexical scope needs
    void register_goroutine_scope_capture(GoroutineExpression* goroutine, 
                                         FunctionExpression* parent_function,
                                         const std::vector<std::string>& captured_vars);
    
    // COMPILE-TIME: Calculate optimal scope address passing for all functions
    void calculate_all_scope_address_strategies();
    
    // ASSEMBLY GENERATION: Generate assembly for variable access in goroutines
    std::string generate_goroutine_variable_access_asm(GoroutineExpression* goroutine, 
                                                      const std::string& var_name) const;
    
    // SIMPLIFIED API: Generate assembly for variable access (for integration with ast_codegen)
    std::vector<std::string> generate_goroutine_variable_access_asm(const std::string& var_name) const;
    
    // SIMPLIFIED API: Generate assembly for variable assignment (for integration with ast_codegen)
    std::vector<std::string> generate_goroutine_variable_assignment_asm(const std::string& var_name) const;
    
    // QUERY API: Check if a variable is captured by goroutines
    bool is_variable_captured(const std::string& var_name) const;
    
    // COMPILE-TIME STATIC ANALYSIS: New methods for proper scope level determination
    void register_variable_declaration(const std::string& var_name, FunctionExpression* declaring_func, const std::string& var_type);
    int determine_variable_scope_level(const std::string& var_name, FunctionExpression* accessing_func) const;
    std::string get_register_for_scope_level(int scope_level) const;
    std::vector<std::string> generate_variable_access_asm_with_static_analysis(const std::string& var_name, FunctionExpression* accessing_func) const;
    
    // ASSEMBLY GENERATION: Generate assembly for setting up scope addresses when calling goroutines
    std::string generate_goroutine_scope_setup_asm(GoroutineExpression* goroutine) const;
    
    // COMPILE-TIME: Get scope layout manager for direct access
    LexicalScopeManager* get_scope_manager() { return scope_manager_.get(); }
    
    // DEBUG: Print all scope address tracking information
    void print_scope_address_analysis() const;
    
    // COMPILE-TIME: Analyze which scope levels a goroutine needs
    std::vector<int> analyze_goroutine_scope_requirements(const std::vector<std::string>& captured_vars,
                                                         FunctionExpression* parent_function) const;
    
    // COMPILE-TIME: Calculate which parent scope levels need to be passed down through call chain
    void calculate_scope_propagation_requirements();
    
    // PUBLIC ACCESS: Get goroutine scope info for variable lookup (needed by ast_codegen)
    const std::vector<GoroutineScopeInfo>& get_goroutine_scope_info() const {
        return goroutine_scope_info_;
    }
};
