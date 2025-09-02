#pragma once

#include "compiler.h"
#include <vector>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <string>

// Forward declarations
struct LexicalScopeNode;
struct FunctionDecl;
struct FunctionExpression;
enum class DataType;

/**
 * Minimal Parse-Time Scope Tracker
 * 
 * This class performs ONLY the minimal scope tracking needed during parsing:
 * 1. Track scope entry/exit to build LexicalScopeNode hierarchy
 * 2. Record variable declarations in their declaring scope
 * 3. Register function declarations for hoisting
 * 
 * NO static analysis, variable packing, or reference resolution is done here.
 * Those are deferred to the StaticAnalyzer pass.
 */
class ParseTimeScopeTracker {
public:
    ParseTimeScopeTracker();
    ~ParseTimeScopeTracker();
    
    // Core scope management
    void enter_scope(bool is_function_scope = false);
    std::unique_ptr<LexicalScopeNode> exit_scope();
    
    // Variable declaration recording (no analysis)
    void declare_variable(const std::string& name, const std::string& declaration_type, DataType data_type);
    void declare_variable(const std::string& name, const std::string& declaration_type); // Legacy overload
    
    // Function registration for hoisting
    void register_function_in_current_scope(FunctionDecl* func_decl);
    void register_function_expression_in_current_scope(FunctionExpression* func_expr);
    
    // Basic getters (no analysis)
    int get_current_depth() const { return current_depth_; }
    LexicalScopeNode* get_current_scope_node() const;
    LexicalScopeNode* get_scope_node_for_depth(int depth) const;
    
    // Legacy compatibility methods (these will delegate to StaticAnalyzer later)
    void access_variable(const std::string& name) { /* No-op during parsing */ }
    void modify_variable(const std::string& name) { /* No-op during parsing */ }
    
private:
    // Minimal state for parse-time tracking
    std::vector<std::shared_ptr<LexicalScopeNode>> scope_stack_;
    std::unordered_map<int, LexicalScopeNode*> depth_to_scope_node_;
    std::vector<std::shared_ptr<LexicalScopeNode>> completed_scopes_;
    int current_depth_;
    
    // Helper methods
    void cleanup_declarations_at_depth(int depth);
};
