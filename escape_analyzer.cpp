#include "escape_analyzer.h"
#include "compiler.h"  // Contains AST node definitions
#include <iostream>
#include <algorithm>

void EscapeAnalyzer::register_consumer(EscapeConsumer* consumer) {
    if (consumer) {
        consumers_.push_back(consumer);
        std::cout << "[EscapeAnalyzer] Registered consumer" << std::endl;
    }
}

void EscapeAnalyzer::unregister_consumer(EscapeConsumer* consumer) {
    consumers_.erase(
        std::remove(consumers_.begin(), consumers_.end(), consumer),
        consumers_.end()
    );
}

void EscapeAnalyzer::set_current_scope_variables(const std::unordered_map<std::string, std::string>& variables) {
    current_scope_variables_ = variables;
    std::cout << "[EscapeAnalyzer] Set current scope with " << variables.size() << " variables:" << std::endl;
    for (const auto& [name, type] : variables) {
        std::cout << "[EscapeAnalyzer]   - " << name << " : " << type << std::endl;
    }
}

void EscapeAnalyzer::add_variable_to_scope(const std::string& var_name, const std::string& var_type) {
    current_scope_variables_[var_name] = var_type;
    std::cout << "[EscapeAnalyzer] Added variable to scope: " << var_name << " : " << var_type << std::endl;
}

void EscapeAnalyzer::add_local_variable(const std::string& var_name, const std::string& var_type) {
    local_variables_[var_name] = var_type;
    std::cout << "[EscapeAnalyzer] Added local variable: " << var_name << " : " << var_type << std::endl;
}

void EscapeAnalyzer::clear_local_variables() {
    local_variables_.clear();
    std::cout << "[EscapeAnalyzer] Cleared local variables" << std::endl;
}

void EscapeAnalyzer::analyze_function_for_escapes(FunctionExpression* func, ASTNode* body) {
    if (!func || !body) {
        return;
    }
    
    std::cout << "[EscapeAnalyzer] Starting escape analysis for function" << std::endl;
    
    // Note: Local variable tracking disabled for now to avoid double-free issues
    // clear_local_variables();
    
    notify_analysis_start(func);
    
    // Traverse the function body looking for variable references
    traverse_node_for_variables(body, func);
    
    std::cout << "[EscapeAnalyzer] Completed escape analysis for function" << std::endl;
    notify_analysis_complete(func);
}

void EscapeAnalyzer::traverse_node_for_variables(ASTNode* node, FunctionExpression* capturing_func) {
    if (!node) {
        return;
    }
    
    std::cout << "[EscapeAnalyzer] Traversing AST node type: " << typeid(*node).name() << std::endl;
    
    // Use dynamic_cast to determine AST node types (UltraScript pattern)
    if (auto var = dynamic_cast<Variable*>(node)) {
        std::string var_name = var->name;
        
        std::cout << "[EscapeAnalyzer] Found variable reference: " << var_name << std::endl;
        
        // Check if this variable exists in the current scope (parent scope)
        if (is_variable_in_scope(var_name)) {
            std::cout << "[EscapeAnalyzer] Variable " << var_name << " ESCAPES to goroutine!" << std::endl;
            notify_escape(var_name, capturing_func);
        } else {
            std::cout << "[EscapeAnalyzer] Variable " << var_name << " not in parent scope" << std::endl;
        }
    }
    else if (auto identifier = dynamic_cast<Identifier*>(node)) {
        std::string var_name = identifier->name;
        
        std::cout << "[EscapeAnalyzer] Found identifier reference: " << var_name << std::endl;
        
        // Check if this identifier refers to a variable in the current scope (parent scope)
        if (is_variable_in_scope(var_name)) {
            std::cout << "[EscapeAnalyzer] Identifier " << var_name << " ESCAPES to goroutine!" << std::endl;
            notify_escape(var_name, capturing_func);
        } else {
            // TODO: Handle local variable identifier escapes properly  
            // For now, skip local variable escape analysis to avoid crashes
            std::cout << "[EscapeAnalyzer] Identifier " << var_name << " not in parent scope - SKIPPING for now" << std::endl;
        }
    }
    else if (auto call = dynamic_cast<MethodCall*>(node)) {
        // Check the object being called on
        std::cout << "[EscapeAnalyzer] Found method call on: " << call->object_name << std::endl;
        if (is_variable_in_scope(call->object_name)) {
            std::cout << "[EscapeAnalyzer] Method call object " << call->object_name << " ESCAPES to goroutine!" << std::endl;
            notify_escape(call->object_name, capturing_func);
        }
        
        // Traverse arguments
        std::cout << "[EscapeAnalyzer] Method call has " << call->arguments.size() << " arguments" << std::endl;
        for (size_t i = 0; i < call->arguments.size(); i++) {
            std::cout << "[EscapeAnalyzer] Processing argument " << i << std::endl;
            traverse_node_for_variables(call->arguments[i].get(), capturing_func);
        }
    }
    else if (auto binop = dynamic_cast<BinaryOp*>(node)) {
        traverse_node_for_variables(binop->left.get(), capturing_func);
        traverse_node_for_variables(binop->right.get(), capturing_func);
    }
    else if (auto assign = dynamic_cast<Assignment*>(node)) {
        // Check the variable being assigned to
        std::cout << "[EscapeAnalyzer] Found assignment to: " << assign->variable_name << std::endl;
        
        if (is_variable_in_scope(assign->variable_name)) {
            // Variable exists in parent scope - it escapes
            std::cout << "[EscapeAnalyzer] Assignment target " << assign->variable_name << " ESCAPES to goroutine!" << std::endl;
            notify_escape(assign->variable_name, capturing_func);
        } else {
            // TODO: Handle local variable escapes properly
            // For now, skip local variable escape analysis to avoid crashes
            std::cout << "[EscapeAnalyzer] Assignment to local variable " << assign->variable_name << " in goroutine - SKIPPING for now" << std::endl;
        }
        
        // Traverse the value being assigned
        traverse_node_for_variables(assign->value.get(), capturing_func);
    }
    else if (auto func_call = dynamic_cast<FunctionCall*>(node)) {
        // Check arguments for variable references
        std::cout << "[EscapeAnalyzer] Found function call: " << func_call->name << std::endl;
        for (auto& arg : func_call->arguments) {
            traverse_node_for_variables(arg.get(), capturing_func);
        }
    }
    // Note: UltraScript doesn't have Block nodes - function bodies are just vectors of statements
    // which are handled at a higher level
    
    std::cout << "[EscapeAnalyzer] Processed AST node (using dynamic_cast)" << std::endl;
}

bool EscapeAnalyzer::is_variable_in_scope(const std::string& var_name) const {
    return current_scope_variables_.find(var_name) != current_scope_variables_.end();
}

std::string EscapeAnalyzer::get_variable_type(const std::string& var_name) const {
    // First check local variables (current function scope)
    auto local_it = local_variables_.find(var_name);
    if (local_it != local_variables_.end()) {
        return local_it->second;
    }
    
    // Then check parent scope variables
    auto parent_it = current_scope_variables_.find(var_name);
    return (parent_it != current_scope_variables_.end()) ? parent_it->second : "";
}

void EscapeAnalyzer::notify_escape(const std::string& var_name, FunctionExpression* capturing_func) {
    std::cout << "[EscapeAnalyzer] notify_escape called for: " << var_name << std::endl;
    
    std::string var_type = get_variable_type(var_name);
    std::cout << "[EscapeAnalyzer] Got variable type: '" << var_type << "'" << std::endl;
    
    if (var_type.empty()) {
        // Variable not found in parent scope - assume it's a local variable
        var_type = "auto"; // Default type for local variables
        std::cout << "[EscapeAnalyzer] Variable type was empty, defaulted to 'auto'" << std::endl;
    }
    std::cout << "[EscapeAnalyzer] NOTIFYING ESCAPE: " << var_name << " (" << var_type << ")" << std::endl;
    
    std::cout << "[EscapeAnalyzer] About to notify " << consumers_.size() << " consumers" << std::endl;
    for (size_t i = 0; i < consumers_.size(); i++) {
        std::cout << "[EscapeAnalyzer] Notifying consumer " << i << std::endl;
        consumers_[i]->on_variable_escaped(var_name, capturing_func, var_type);
        std::cout << "[EscapeAnalyzer] Consumer " << i << " notified successfully" << std::endl;
    }
    std::cout << "[EscapeAnalyzer] All consumers notified" << std::endl;
}

void EscapeAnalyzer::notify_analysis_start(FunctionExpression* func) {
    for (auto* consumer : consumers_) {
        consumer->on_function_analysis_start(func);
    }
}

void EscapeAnalyzer::notify_analysis_complete(FunctionExpression* func) {
    for (auto* consumer : consumers_) {
        consumer->on_function_analysis_complete(func);
    }
}

// Check if a type is a reference type that could escape when passed to functions
bool EscapeAnalyzer::is_reference_type(const std::string& type) const {
    // Reference types that could contain heap-allocated data
    if (type == "auto" || type == "any" || type == "DynamicValue") {
        return true; // Could contain objects/arrays
    }
    if (type == "Array" || type.find("Array") != std::string::npos) {
        return true; // Arrays are heap-allocated
    }
    if (type == "Object" || type.find("Object") != std::string::npos) {
        return true; // Objects are heap-allocated  
    }
    if (type == "string" || type == "String") {
        return false; // Strings are copied by value in UltraScript
    }
    
    // Primitive value types (pass by value, no escape)
    if (type == "int64" || type == "int32" || type == "int16" || type == "int8" ||
        type == "float64" || type == "float32" || 
        type == "number" || type == "bool" || type == "boolean") {
        return false;
    }
    
    // Default: assume reference type for safety
    return true;
}
