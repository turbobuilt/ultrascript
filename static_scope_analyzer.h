#pragma once

#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <string>
#include <memory>
#include "compiler.h"

// Use Assignment's DeclarationKind enum instead of our own
using DeclarationKind = Assignment::DeclarationKind;

// Memory layout information for testing
struct VariableLayoutInfo {
    std::string variable_name;
    int scope_level;
    size_t offset;
    size_t size;
    size_t alignment;
};

struct MemoryLayoutInfo {
    std::vector<VariableLayoutInfo> variable_layouts;
    size_t total_size;
    bool optimization_complete;
};

// Information about a variable's lexical scope and access pattern
struct LexicalScopeInfo {
    int scope_level;                // 0 = current, 1 = parent, 2 = grandparent, etc.
    std::string variable_name;
    size_t offset_in_scope;         // Byte offset within that scope
    bool escapes_current_function;  // Captured by goroutines/callbacks
    DataType type;                  // Variable type for optimization
    size_t size_bytes;              // Size of variable in bytes
    
    // NEW: Variable declaration type and scoping
    DeclarationKind declaration_kind;
    bool is_block_scoped;           // true for let/const, false for var
    bool is_loop_iteration_scoped;  // true for let/const in for loops (special case)
    
    // Variable ordering and access optimization
    int access_frequency;           // How often this variable is accessed
    std::vector<std::string> co_accessed_variables; // Variables accessed together
    int optimal_order_index;        // Optimized position in scope (0 = first)
    bool is_hot_variable;           // Frequently accessed, should be at low offsets
    size_t alignment_requirement;   // Memory alignment (4, 8, 16 bytes)
};

// Represents function-level scope analysis results
struct FunctionScopeAnalysis {
    std::string function_name;
    bool has_escaping_variables;                        // Any variables escape?
    
    // ULTIMATE OPTIMIZATION: Separate self needs from descendant needs
    std::unordered_set<int> self_parent_scope_needs;    // Parent levels THIS function directly accesses
    std::unordered_set<int> descendant_parent_scope_needs; // Parent levels needed ONLY by descendants
    std::unordered_set<int> required_parent_scopes;     // Combined: self + descendant (for compatibility)
    
    std::unordered_map<std::string, LexicalScopeInfo> variables; // Variable info
    size_t total_stack_space_needed;                    // For non-escaping variables
    size_t total_heap_scope_size;                       // For escaping variables
    
    // PRIORITY REGISTER ALLOCATION FOR MAXIMUM PERFORMANCE
    std::unordered_map<int, int> fast_register_allocation; // parent_level -> register_id (r12, r13, r14)
    std::unordered_map<int, int> stack_allocation;         // parent_level -> stack_offset
    std::unordered_set<int> used_scope_registers;          // Which registers are allocated for scopes
    bool needs_stack_fallback = false;                     // If more than 3 scope levels, use stack fallback
    
    // NEW: Variable ordering and offset optimization
    struct ScopeLayoutInfo {
        std::vector<std::string> variable_order;        // Optimized variable ordering in scope
        std::unordered_map<std::string, size_t> variable_offsets; // Variable name -> byte offset
        size_t total_scope_size;                         // Total bytes needed for this scope
        std::vector<std::pair<std::string, std::string>> access_patterns; // Co-accessed variable pairs
        bool has_hot_variables;                          // Contains frequently accessed vars
        
        // NEW: Block scoping optimization
        bool is_block_scope;                             // true if this is a block scope (let/const)
        bool needs_actual_scope;                         // false if we can optimize away (var-only blocks)
        std::string scope_type;                          // "function", "block", "loop", "loop-iteration"
        bool can_be_optimized_away;                      // Performance optimization flag
    };
    
    std::unordered_map<int, ScopeLayoutInfo> scope_layouts; // scope_level -> layout info
    bool layout_optimization_complete = false;              // Has offset calculation been done?
    
    // NEW: Block scoping analysis results
    std::unordered_map<int, bool> scope_contains_let_const;  // scope_level -> contains let/const
    std::unordered_map<int, int> optimized_scope_mapping;    // logical_level -> actual_level (for optimized-away scopes)
    int actual_scope_count = 0;                              // Number of actual scopes after optimization
    int logical_scope_count = 0;                             // Number of logical scopes before optimization
};

class StaticScopeAnalyzer {
public:
    StaticScopeAnalyzer();
    
    // Main analysis entry point
    void analyze_function(const std::string& function_name, ASTNode* function_node);
    
    // ES6 Block Scoping Methods
    void begin_function_analysis(const std::string& function_name);
    void end_function_analysis();
    void add_variable_with_declaration_kind(const std::string& name, DeclarationKind kind, int scope_level, int usage_order);
    void optimize_scope_allocation(const std::string& function_name);
    bool analyze_block_needs_scope(ASTNode* block_node);
    void analyze_loop_scoping(ASTNode* loop_node);
    
    // Variable ordering and memory layout methods
    void optimize_variable_ordering(const std::string& function_name);
    void compute_variable_offsets(const std::string& function_name);
    
    // Query methods for code generation
    LexicalScopeInfo& get_variable_info(const std::string& var_name);
    LexicalScopeInfo get_variable_info(const std::string& var_name) const;
    FunctionScopeAnalysis get_function_analysis(const std::string& function_name) const;
    bool function_has_escaping_variables(const std::string& function_name) const;
    
    // NEW: Variable ordering and offset calculation
    void optimize_variable_layout(const std::string& function_name);
    void calculate_variable_offsets(const std::string& function_name);
    size_t get_variable_offset_in_scope(const std::string& function_name, const std::string& var_name) const;
    std::vector<std::string> get_optimized_variable_order(const std::string& function_name, int scope_level) const;
    bool is_layout_optimization_complete(const std::string& function_name) const;
    
    // NEW: Block scoping and performance optimization queries
    bool scope_needs_actual_allocation(const std::string& function_name, int scope_level) const;
    int get_optimized_scope_count(const std::string& function_name) const;
    int get_actual_scope_level(const std::string& function_name, int logical_scope_level) const;
    std::vector<std::string> get_var_only_scopes(const std::string& function_name) const;
    bool has_let_const_in_scope(const std::string& function_name, int scope_level) const;
    
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
    
    // Memory layout query methods  
    MemoryLayoutInfo get_memory_layout(const std::string& function_name) const;
    
private:
    std::unordered_map<std::string, LexicalScopeInfo> variable_scope_map_;
    std::vector<std::vector<std::string>> scope_stack_;  // Track nested scopes
    std::unordered_map<std::string, FunctionScopeAnalysis> function_analyses_;
    
    // Descendant function tracking
    std::unordered_map<std::string, std::vector<std::string>> function_descendants_; // function -> list of nested functions
    std::unordered_map<std::string, std::string> function_parent_;  // function -> parent function
    std::unordered_map<std::string, int> function_scope_level_;     // function -> scope level where it's defined
    
    // Current analysis state
    std::string current_function_name_;
    int current_scope_level_;
    int current_goroutine_depth_ = 0;  // Track nested goroutine depth
    
    // Internal helper methods for AST walking
    void walk_ast_for_scopes(ASTNode* node);
    void add_variable_to_scope(const std::string& name, int scope_level, DataType type);
    void record_variable_usage(const std::string& name, int usage_scope_level);
    void determine_register_allocation(const std::string& function_name);
    void analyze_parent_scope_dependencies(const std::string& function_name);
    void analyze_descendant_scope_needs(const std::string& function_name, ASTNode* function_node);
    void find_nested_functions(ASTNode* node, const std::string& parent_function, int current_level);
    void propagate_descendant_needs_bottom_up();
    void print_function_analysis(const std::string& function_name);
    
    // Internal block scoping analysis methods
    bool can_optimize_away_scope(int scope_level);
    void merge_var_only_scopes(const std::string& function_name);
    
    // Variable ordering and offset calculation helpers
    void analyze_variable_access_patterns(const std::string& function_name, ASTNode* function_node);
    void calculate_access_frequencies(const std::string& function_name, ASTNode* function_node);
    void identify_co_accessed_variables(const std::string& function_name, ASTNode* function_node);
    void optimize_variable_ordering_by_frequency(const std::string& function_name, int scope_level);
    void optimize_variable_ordering_by_locality(const std::string& function_name, int scope_level);
    size_t calculate_aligned_offset(size_t current_offset, size_t alignment) const;
    size_t get_variable_alignment_requirement(DataType type) const;
};

// Integration with existing TypeInference system
class LexicalScopeIntegration {
private:
    std::unique_ptr<StaticScopeAnalyzer> analyzer_;
    std::unordered_set<std::string> goroutine_functions_;  // Track which functions are goroutines
    
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
    
    // HIGH-PERFORMANCE PRIORITY-BASED REGISTER ALLOCATION
    int get_register_for_scope_level(const std::string& function_name, int scope_level) const;
    int get_stack_offset_for_scope_level(const std::string& function_name, int scope_level) const;
    bool scope_level_uses_fast_register(const std::string& function_name, int scope_level) const;
    bool scope_level_uses_stack(const std::string& function_name, int scope_level) const;
    std::unordered_set<int> get_used_scope_registers(const std::string& function_name) const;
    bool needs_stack_fallback(const std::string& function_name) const;
    
    // PRIORITY ANALYSIS QUERIES
    std::unordered_set<int> get_self_parent_scope_needs(const std::string& function_name) const;
    std::unordered_set<int> get_descendant_parent_scope_needs(const std::string& function_name) const;
    
    // VARIABLE ACCESS HELPER - Returns how a variable should be accessed with priority info
    std::string get_variable_access_pattern(const std::string& function_name, const std::string& var_name) const;
    
    // Analysis trigger
    void analyze_function(const std::string& function_name, ASTNode* function_node);
    
    // Goroutine tracking
    void mark_function_as_goroutine(const std::string& function_name);
    bool is_function_goroutine(const std::string& function_name) const;
};
