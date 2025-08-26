#pragma once
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <mutex>
#include <iostream>


// Forward declarations - DataType will be provided by compiler.h
enum class DataType; // Forward declaration

// Simple escape reasons for minimal GC (avoid circular dependencies)
enum class SimpleEscapeType {
    NONE,           
    FUNCTION_ARG,   
    CALLBACK,       
    OBJECT_ASSIGN,  
    RETURN_VALUE,   
    GLOBAL_ASSIGN,  
    GOROUTINE       
};

// Structure for escaped variable information
struct EscapedVariableInfo {
    std::string name;
    DataType type;
    size_t variable_id;
    SimpleEscapeType escape_reason;
};

// Minimal parser GC integration without compiler dependencies
class MinimalParserGCIntegration {
private:
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
    std::unordered_map<std::string, std::vector<size_t>> variable_scopes_;
    std::vector<EscapedVariableInfo> escaped_variables_;  // Store escaped variables
    std::unordered_map<std::string, DataType> variable_types_;  // Store variable types
    
public:
    void enter_scope(const std::string& scope_name, bool is_function = false);
    void exit_scope();
    
    void declare_variable(const std::string& name, DataType type);
    void assign_variable(const std::string& name);
    void use_variable(const std::string& name);
    
    void mark_function_call(const std::string& func_name, const std::vector<std::string>& args);
    void mark_property_assignment(const std::string& obj, const std::string& prop);
    void mark_return_value(const std::string& var_name);
    void mark_closure_capture(const std::vector<std::string>& captured_vars);
    void mark_goroutine_capture(const std::vector<std::string>& captured_vars);
    
    void finalize_analysis();
    
    // Get escaped variables for code generation
    const std::vector<EscapedVariableInfo>& get_escaped_variables() const { return escaped_variables_; }
    
private:
    bool is_variable_in_scope(const std::string& name) const;
    size_t get_variable_id(const std::string& name) const;
    void propagate_escape_to_parents(const std::string& var_name);
};

