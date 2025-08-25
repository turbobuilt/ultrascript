#pragma once

#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <string>
#include <memory>
#include "compiler.h"

// Information about a variable's lexical scope and access pattern
struct LexicalScopeInfo {
    int scope_level;                // 0 = current, 1 = parent, 2 = grandparent, etc.
    std::string variable_name;
    size_t offset_in_scope;         // Byte offset within that scope
    bool escapes_current_function;  // Captured by goroutines/callbacks
    DataType type;                  // Variable type for optimization
    size_t size_bytes;              // Size of variable in bytes
};

// Represents function-level scope analysis results
struct FunctionScopeAnalysis {
    std::string function_name;
    bool has_escaping_variables;                        // Any variables escape?
    std::unordered_set<int> required_parent_scopes;     // Which parent scope levels need saving
    std::unordered_map<std::string, LexicalScopeInfo> variables; // Variable info
    size_t total_stack_space_needed;                    // For non-escaping variables
    size_t total_heap_scope_size;                       // For escaping variables
    
    // REGISTER ALLOCATION FOR HIGH-PERFORMANCE LEXICAL SCOPE ACCESS
    std::unordered_map<int, int> scope_level_to_register; // scope_level -> register_id (12=R12, 13=R13, 14=R14, 15=R15, 3=RBX)
    std::unordered_set<int> used_scope_registers;       // Which registers are allocated for scopes
    bool needs_stack_fallback = false;                  // If more than 5 scope levels, use stack fallback
};

class StaticScopeAnalyzer {
private:
    std::unordered_map<std::string, LexicalScopeInfo> variable_scope_map_;
    std::vector<std::vector<std::string>> scope_stack_;  // Track nested scopes
    std::unordered_map<std::string, FunctionScopeAnalysis> function_analyses_;
    
    // Current analysis state
    std::string current_function_name_;
    int current_scope_level_;
    int current_goroutine_depth_ = 0;  // Track nested goroutine depth
    
    // Internal helper methods for AST walking
    void walk_ast_for_scopes(ASTNode* node);
    void add_variable_to_scope(const std::string& name, int scope_level, DataType type);
    void record_variable_usage(const std::string& name, int usage_scope_level);
    void determine_register_allocation(const std::string& function_name);
    
public:
    StaticScopeAnalyzer();
    
    // Main analysis entry point
    void analyze_function(const std::string& function_name, ASTNode* function_node);
    
    // Query methods for code generation
    LexicalScopeInfo get_variable_info(const std::string& var_name) const;
    FunctionScopeAnalysis get_function_analysis(const std::string& function_name) const;
    bool function_has_escaping_variables(const std::string& function_name) const;
    
    // Variable classification
    bool is_non_escaping_function(const std::string& function_name) const;
    std::vector<std::string> get_stack_variables(const std::string& function_name) const;
    std::vector<std::string> get_heap_scope_variables(const std::string& function_name) const;
    
private:
    // Analysis helpers
    void build_scope_hierarchy(ASTNode* function_node);
    void analyze_variable_declarations(ASTNode* node);
    void analyze_variable_escapes(ASTNode* node);
    void calculate_required_parent_scopes(const std::string& function_name);
    void calculate_memory_layouts(const std::string& function_name);
    
    // Variable escape detection
    bool check_if_variable_escapes(const std::string& var_name, ASTNode* function_node);
    void find_goroutine_captures(ASTNode* node, std::unordered_set<std::string>& captured_vars);
    void find_callback_captures(ASTNode* node, std::unordered_set<std::string>& captured_vars);
    
    // Scope level calculation
    int calculate_scope_level(const std::string& var_name) const;
    size_t calculate_variable_offset(const std::string& var_name, bool in_heap_scope) const;
    size_t get_variable_size(DataType type) const;
    
    // Memory layout optimization
    void optimize_stack_layout(const std::string& function_name);
    void optimize_heap_scope_layout(const std::string& function_name);
};

// Integration with existing TypeInference system
class LexicalScopeIntegration {
private:
    std::unique_ptr<StaticScopeAnalyzer> analyzer_;
    
public:
    LexicalScopeIntegration();
    
    // Methods to replace existing TypeInference escape analysis
    bool variable_escapes(const std::string& function_name, const std::string& var_name) const;
    int64_t get_variable_offset(const std::string& function_name, const std::string& var_name) const;
    bool function_needs_r15_register(const std::string& function_name) const;
    
    // New methods for the updated system
    bool should_use_heap_scope(const std::string& function_name) const;
    std::vector<int> get_required_parent_scope_levels(const std::string& function_name) const;
    size_t get_heap_scope_size(const std::string& function_name) const;
    
    // HIGH-PERFORMANCE REGISTER-BASED SCOPE ACCESS
    int get_register_for_scope_level(const std::string& function_name, int scope_level) const;
    std::unordered_set<int> get_used_scope_registers(const std::string& function_name) const;
    bool needs_stack_fallback(const std::string& function_name) const;
    
    // Analysis trigger
    void analyze_function(const std::string& function_name, ASTNode* function_node);
};
