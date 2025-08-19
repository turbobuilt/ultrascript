#pragma once
#include "compiler.h"
#include "gc_system.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>


// Forward declarations
struct ASTNode;
struct ExpressionNode;
struct Variable;
enum class DataType;

/**
 * GC Integration for the existing Parser class
 * This integrates escape analysis directly into the parsing process
 */
class ParserGCIntegration {
private:
    // Scope tracking
    struct ScopeInfo {
        size_t scope_id;
        std::string scope_name;
        bool is_function_scope;
        std::unordered_set<std::string> declared_variables;
        std::unordered_map<std::string, size_t> variable_ids;
    };
    
    std::vector<ScopeInfo> scope_stack_;
    size_t next_scope_id_ = 1;
    size_t next_variable_id_ = 1;
    
    // Variable tracking across scopes
    std::unordered_map<std::string, std::vector<size_t>> variable_scopes_;
    
public:
    // Called by Parser during parsing
    void enter_scope(const std::string& scope_name, bool is_function);
    void exit_scope();
    
    // Variable lifecycle tracking
    void declare_variable(const std::string& name, DataType type);
    void assign_variable(const std::string& name, ExpressionNode* value);
    void use_variable(const std::string& name);
    
    // Escape analysis triggers
    void mark_function_call(const std::string& func_name, const std::vector<std::unique_ptr<ExpressionNode>>& args);
    void mark_property_assignment(const std::string& obj, const std::string& prop, ExpressionNode* value);
    void mark_return_value(ExpressionNode* value);
    void mark_closure_capture(const std::vector<std::string>& captured_vars);
    void mark_goroutine_capture(const std::vector<std::string>& captured_vars);
    
    // Integration with existing EscapeAnalyzer
    void finalize_analysis();
    
    // Helper methods for Parser
    bool is_variable_in_scope(const std::string& name) const;
    size_t get_current_scope_depth() const { return scope_stack_.size(); }
    
private:
    size_t get_variable_scope(const std::string& name) const;
    void propagate_escape_to_parents(const std::string& var_name);
    void mark_expression_escape(ExpressionNode* expr, EscapeType escape_type);
    size_t get_variable_id(const std::string& name) const;
};