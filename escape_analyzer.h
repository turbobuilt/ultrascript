#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>

// Forward declarations
class FunctionExpression;
class ASTNode;
class Variable;
class Identifier;
class MethodCall;
class BinaryOp;

// Abstract consumer interface for escape detection
class EscapeConsumer {
public:
    virtual ~EscapeConsumer() = default;
    
    // Called when a variable is captured by a function (goroutine)
    virtual void on_variable_escaped(const std::string& var_name, 
                                   FunctionExpression* capturing_func,
                                   const std::string& var_type = "") = 0;
    
    // Called when escape analysis starts for a function
    virtual void on_function_analysis_start(FunctionExpression* func) = 0;
    
    // Called when escape analysis completes for a function
    virtual void on_function_analysis_complete(FunctionExpression* func) = 0;
};

// Shared escape detector - traverses AST and notifies consumers
class EscapeAnalyzer {
private:
    std::vector<EscapeConsumer*> consumers_;
    std::unordered_map<std::string, std::string> current_scope_variables_; // parent scope: var_name -> type
    std::unordered_map<std::string, std::string> local_variables_; // current function scope: var_name -> type
    
public:
    EscapeAnalyzer() = default;
    ~EscapeAnalyzer() = default;
    
    // Register a consumer to be notified of escapes
    void register_consumer(EscapeConsumer* consumer);
    
    // Remove a consumer
    void unregister_consumer(EscapeConsumer* consumer);
    
    // Analyze a function for variable escapes
    void analyze_function_for_escapes(FunctionExpression* func, ASTNode* body);
    
    // Set the current scope variables (from parent scope)
    void set_current_scope_variables(const std::unordered_map<std::string, std::string>& variables);
    
    // Add a variable to current scope
    void add_variable_to_scope(const std::string& var_name, const std::string& var_type);
    
    // Add a local variable to current function scope
    void add_local_variable(const std::string& var_name, const std::string& var_type);
    
    // Clear local variables (called when starting new function analysis)
    void clear_local_variables();
    
private:
    // Traverse AST nodes looking for variable references
    void traverse_node_for_variables(ASTNode* node, FunctionExpression* capturing_func);
    
    // Notify all consumers of an escape
    void notify_escape(const std::string& var_name, FunctionExpression* capturing_func);
    
    // Notify consumers of analysis start/complete
    void notify_analysis_start(FunctionExpression* func);
    void notify_analysis_complete(FunctionExpression* func);
    
    // Check if a variable exists in current scope
    bool is_variable_in_scope(const std::string& var_name) const;
    
    // Get variable type from current scope
    std::string get_variable_type(const std::string& var_name) const;
    
    // Check if a type is a reference type (needs escape analysis)
    bool is_reference_type(const std::string& type) const;
};
