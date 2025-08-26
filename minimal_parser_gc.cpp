#include "minimal_parser_gc.h"
#include "compiler.h"  // For DataType enum
#include <iostream>


void MinimalParserGCIntegration::enter_scope(const std::string& scope_name, bool is_function) {
    ScopeInfo scope;
    scope.scope_id = next_scope_id_++;
    scope.scope_name = scope_name;
    scope.is_function_scope = is_function;
    
    scope_stack_.push_back(scope);
    
    std::cout << "[MinimalGC] Entered scope '" << scope_name << "' (id=" << scope.scope_id << ")" << std::endl;
}

void MinimalParserGCIntegration::exit_scope() {
    if (scope_stack_.empty()) {
        std::cerr << "[MinimalGC] ERROR: Attempting to exit scope when none exists!" << std::endl;
        return;
    }
    
    ScopeInfo& current = scope_stack_.back();
    
    std::cout << "[MinimalGC] Exiting scope '" << current.scope_name << "' (id=" << current.scope_id 
              << ") with " << current.declared_variables.size() << " variables" << std::endl;
    
    scope_stack_.pop_back();
}

void MinimalParserGCIntegration::declare_variable(const std::string& name, DataType type) {
    if (scope_stack_.empty()) {
        std::cerr << "[MinimalGC] ERROR: Variable declaration outside of scope!" << std::endl;
        return;
    }
    
    ScopeInfo& current = scope_stack_.back();
    current.declared_variables.insert(name);
    
    size_t variable_id = next_variable_id_++;
    current.variable_ids[name] = variable_id;
    
    variable_scopes_[name].push_back(current.scope_id);
    variable_types_[name] = type;  // Store the variable type
    
    std::cout << "[MinimalGC] Declared variable '" << name << "' in scope '" 
              << current.scope_name << "' (id=" << variable_id << ")" << std::endl;
}

void MinimalParserGCIntegration::assign_variable(const std::string& name) {
    if (!is_variable_in_scope(name)) {
        std::cout << "[MinimalGC] Variable '" << name << "' assigned but not in current scope - possible escape" << std::endl;
        propagate_escape_to_parents(name);
    }
    
    use_variable(name);
}

void MinimalParserGCIntegration::use_variable(const std::string& name) {
    std::cout << "[MinimalGC] Using variable '" << name << "'" << std::endl;
}

void MinimalParserGCIntegration::mark_function_call(const std::string& func_name, const std::vector<std::string>& args) {
    std::cout << "[MinimalGC] Function call '" << func_name << "' with " << args.size() << " arguments" << std::endl;
    
    for (const auto& arg : args) {
        std::cout << "[MinimalGC] Argument '" << arg << "' escapes via function call" << std::endl;
        propagate_escape_to_parents(arg);
    }
}

void MinimalParserGCIntegration::mark_property_assignment(const std::string& obj, const std::string& prop) {
    std::cout << "[MinimalGC] Property assignment: " << obj << "." << prop << " = <value>" << std::endl;
    propagate_escape_to_parents(obj);
}

void MinimalParserGCIntegration::mark_return_value(const std::string& var_name) {
    std::cout << "[MinimalGC] Return value: '" << var_name << "' escapes" << std::endl;
    propagate_escape_to_parents(var_name);
}

void MinimalParserGCIntegration::mark_closure_capture(const std::vector<std::string>& captured_vars) {
    std::cout << "[MinimalGC] Closure captures " << captured_vars.size() << " variables" << std::endl;
    
    for (const std::string& var : captured_vars) {
        std::cout << "[MinimalGC] Variable '" << var << "' captured by closure" << std::endl;
        propagate_escape_to_parents(var);
    }
}

void MinimalParserGCIntegration::mark_goroutine_capture(const std::vector<std::string>& captured_vars) {
    std::cout << "[MinimalGC] Goroutine captures " << captured_vars.size() << " variables" << std::endl;
    
    for (const std::string& var : captured_vars) {
        std::cout << "[MinimalGC] Variable '" << var << "' captured by goroutine" << std::endl;
        propagate_escape_to_parents(var);
    }
}

void MinimalParserGCIntegration::finalize_analysis() {
    std::cout << "[MinimalGC] Finalizing escape analysis..." << std::endl;
    std::cout << "[MinimalGC] Total variables tracked: " << variable_scopes_.size() << std::endl;
    
    if (!scope_stack_.empty()) {
        std::cerr << "[MinimalGC] WARNING: " << scope_stack_.size() << " scopes remain open!" << std::endl;
    }
}

bool MinimalParserGCIntegration::is_variable_in_scope(const std::string& name) const {
    for (auto it = scope_stack_.rbegin(); it != scope_stack_.rend(); ++it) {
        if (it->declared_variables.find(name) != it->declared_variables.end()) {
            return true;
        }
    }
    return false;
}

size_t MinimalParserGCIntegration::get_variable_id(const std::string& name) const {
    for (auto it = scope_stack_.rbegin(); it != scope_stack_.rend(); ++it) {
        auto id_it = it->variable_ids.find(name);
        if (id_it != it->variable_ids.end()) {
            return id_it->second;
        }
    }
    return 0;
}

void MinimalParserGCIntegration::propagate_escape_to_parents(const std::string& var_name) {
    auto scopes_it = variable_scopes_.find(var_name);
    if (scopes_it != variable_scopes_.end()) {
        for (size_t scope_id : scopes_it->second) {
            for (const ScopeInfo& scope : scope_stack_) {
                if (scope.scope_id == scope_id) {
                    auto id_it = scope.variable_ids.find(var_name);
                    if (id_it != scope.variable_ids.end()) {
                        std::cout << "[MinimalGC] Marking variable '" << var_name 
                                  << "' (id=" << id_it->second << ") as escaped" << std::endl;
                        
                        // Store the escaped variable information
                        EscapedVariableInfo info;
                        info.name = var_name;
                        info.variable_id = id_it->second;
                        info.escape_reason = SimpleEscapeType::GOROUTINE;  // Default to goroutine for now
                        
                        // Get the variable type
                        auto type_it = variable_types_.find(var_name);
                        if (type_it != variable_types_.end()) {
                            info.type = type_it->second;
                        } else {
                            info.type = DataType::ANY;  // Default fallback
                        }
                        
                        // Avoid duplicates
                        bool already_exists = false;
                        for (const auto& existing : escaped_variables_) {
                            if (existing.name == var_name) {
                                already_exists = true;
                                break;
                            }
                        }
                        
                        if (!already_exists) {
                            escaped_variables_.push_back(info);
                            std::cout << "[MinimalGC] Added escaped variable '" << var_name 
                                      << "' to collection" << std::endl;
                        }
                    }
                    break;
                }
            }
        }
    }
}

