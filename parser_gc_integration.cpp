#include "parser_gc_integration.h"
#include "compiler.h"
#include <iostream>


void ParserGCIntegration::enter_scope(const std::string& scope_name, bool is_function) {
    ScopeInfo scope;
    scope.scope_id = next_scope_id_++;
    scope.scope_name = scope_name;
    scope.is_function_scope = is_function;
    
    scope_stack_.push_back(scope);
    
    // Register with escape analyzer
    GCEscapeAnalyzer::instance().enter_scope(scope.scope_id);
    
    std::cout << "[GC-Parser] Entered scope '" << scope_name << "' (id=" << scope.scope_id << ")" << std::endl;
}

void ParserGCIntegration::exit_scope() {
    if (scope_stack_.empty()) {
        std::cerr << "[GC-Parser] ERROR: Attempting to exit scope when none exists!" << std::endl;
        return;
    }
    
    ScopeInfo& current = scope_stack_.back();
    
    // Register scope exit with escape analyzer
    GCEscapeAnalyzer::instance().exit_scope(current.scope_id);
    
    std::cout << "[GC-Parser] Exiting scope '" << current.scope_name << "' (id=" << current.scope_id 
              << ") with " << current.declared_variables.size() << " variables" << std::endl;
    
    scope_stack_.pop_back();
}

void ParserGCIntegration::declare_variable(const std::string& name, DataType type) {
    if (scope_stack_.empty()) {
        std::cerr << "[GC-Parser] ERROR: Variable declaration outside of scope!" << std::endl;
        return;
    }
    
    ScopeInfo& current = scope_stack_.back();
    current.declared_variables.insert(name);
    
    // Create unique variable ID and register with escape analyzer
    size_t variable_id = next_variable_id_++;
    current.variable_ids[name] = variable_id;
    
    GCEscapeAnalyzer::instance().register_variable(variable_id, name, current.scope_id);
    
    // Track variable across scopes
    variable_scopes_[name].push_back(current.scope_id);
    
    std::cout << "[GC-Parser] Declared variable '" << name << "' in scope '" 
              << current.scope_name << "' (id=" << variable_id << ")" << std::endl;
}

void ParserGCIntegration::assign_variable(const std::string& name, ExpressionNode* value) {
    if (!is_variable_in_scope(name)) {
        std::cout << "[GC-Parser] Variable '" << name << "' assigned but not in current scope - possible escape" << std::endl;
        // This might be assignment to outer scope variable - mark as escape
        propagate_escape_to_parents(name);
    }
    
    // TODO: Analyze the value expression for escapes
    use_variable(name);
}

void ParserGCIntegration::use_variable(const std::string& name) {
    // Track variable use for escape analysis
    std::cout << "[GC-Parser] Using variable '" << name << "'" << std::endl;
}

void ParserGCIntegration::mark_function_call(const std::string& func_name, const std::vector<std::unique_ptr<ExpressionNode>>& args) {
    std::cout << "[GC-Parser] Function call '" << func_name << "' with " << args.size() << " arguments" << std::endl;
    
    // All arguments passed to functions potentially escape
    for (size_t i = 0; i < args.size(); i++) {
        // Mark argument as escaping via function call
        mark_expression_escape(args[i].get(), EscapeType::FUNCTION_ARG);
    }
}

void ParserGCIntegration::mark_property_assignment(const std::string& obj, const std::string& prop, ExpressionNode* value) {
    std::cout << "[GC-Parser] Property assignment: " << obj << "." << prop << " = <value>" << std::endl;
    
    // Value assigned to object property escapes
    mark_expression_escape(value, EscapeType::OBJECT_ASSIGN);
}

void ParserGCIntegration::mark_return_value(ExpressionNode* value) {
    std::cout << "[GC-Parser] Return value escapes" << std::endl;
    
    // Returned values escape
    mark_expression_escape(value, EscapeType::RETURN_VALUE);
}

void ParserGCIntegration::mark_closure_capture(const std::vector<std::string>& captured_vars) {
    std::cout << "[GC-Parser] Closure captures " << captured_vars.size() << " variables" << std::endl;
    
    for (const std::string& var : captured_vars) {
        size_t variable_id = get_variable_id(var);
        if (variable_id != 0) {
            GCEscapeAnalyzer::instance().register_escape(variable_id, EscapeType::CALLBACK, 0);
        }
        std::cout << "[GC-Parser] Variable '" << var << "' captured by closure" << std::endl;
    }
}

void ParserGCIntegration::mark_goroutine_capture(const std::vector<std::string>& captured_vars) {
    std::cout << "[GC-Parser] Goroutine captures " << captured_vars.size() << " variables" << std::endl;
    
    // Goroutine captures behave like closures but potentially cross threads
    for (const std::string& var : captured_vars) {
        size_t variable_id = get_variable_id(var);
        if (variable_id != 0) {
            GCEscapeAnalyzer::instance().register_escape(variable_id, EscapeType::GOROUTINE, 0);
        }
        std::cout << "[GC-Parser] Variable '" << var << "' captured by goroutine" << std::endl;
    }
}

void ParserGCIntegration::finalize_analysis() {
    std::cout << "[GC-Parser] Finalizing escape analysis..." << std::endl;
    std::cout << "[GC-Parser] Total variables tracked: " << variable_scopes_.size() << std::endl;
    
    if (!scope_stack_.empty()) {
        std::cerr << "[GC-Parser] WARNING: " << scope_stack_.size() << " scopes remain open!" << std::endl;
    }
}

void ParserGCIntegration::mark_expression_escape(ExpressionNode* expr, EscapeType escape_type) {
    if (!expr) return;
    
    // Try to extract variable name from expression
    // This is a simplified implementation - in practice we'd need full AST traversal
    std::cout << "[GC-Parser] Expression escapes with type " << static_cast<int>(escape_type) << std::endl;
    
    // TODO: Implement full expression analysis to identify all variables that escape
}

size_t ParserGCIntegration::get_variable_id(const std::string& name) const {
    // Find variable ID in current or parent scopes
    for (auto it = scope_stack_.rbegin(); it != scope_stack_.rend(); ++it) {
        auto id_it = it->variable_ids.find(name);
        if (id_it != it->variable_ids.end()) {
            return id_it->second;
        }
    }
    return 0; // Not found
}

bool ParserGCIntegration::is_variable_in_scope(const std::string& name) const {
    // Check from innermost scope outward
    for (auto it = scope_stack_.rbegin(); it != scope_stack_.rend(); ++it) {
        if (it->declared_variables.find(name) != it->declared_variables.end()) {
            return true;
        }
    }
    return false;
}

size_t ParserGCIntegration::get_variable_scope(const std::string& name) const {
    // Find the scope where this variable was declared
    for (auto it = scope_stack_.rbegin(); it != scope_stack_.rend(); ++it) {
        if (it->declared_variables.find(name) != it->declared_variables.end()) {
            return it->scope_id;
        }
    }
    return 0; // Global scope fallback
}

void ParserGCIntegration::propagate_escape_to_parents(const std::string& var_name) {
    // Mark all allocation sites for this variable as escaping
    auto scopes_it = variable_scopes_.find(var_name);
    if (scopes_it != variable_scopes_.end()) {
        for (size_t scope_id : scopes_it->second) {
            // Find the scope and mark its allocation sites as escaping
            for (const ScopeInfo& scope : scope_stack_) {
                if (scope.scope_id == scope_id) {
                    auto id_it = scope.variable_ids.find(var_name);
                    if (id_it != scope.variable_ids.end()) {
                        // Mark the variable as escaped in EscapeAnalyzer
                        GCEscapeAnalyzer::instance().register_escape(id_it->second, EscapeType::GLOBAL_ASSIGN, 0);
                        std::cout << "[GC-Parser] Marking variable '" << var_name 
                                  << "' (id=" << id_it->second << ") as escaped" << std::endl;
                    }
                    break;
                }
            }
        }
    }
}

