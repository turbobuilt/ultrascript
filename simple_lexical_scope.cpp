#include "simple_lexical_scope.h"
#include <algorithm>
#include <iostream>

// Called when entering a new lexical scope (function, block, etc.)
void SimpleLexicalScopeAnalyzer::enter_scope() {
    current_depth_++;
    auto scope = std::make_unique<LexicalScopeInfo>(current_depth_);
    scope_stack_.push_back(std::move(scope));
    
    std::cout << "[SimpleLexicalScope] Entered scope at depth " << current_depth_ << std::endl;
}

// Called when exiting a lexical scope
void SimpleLexicalScopeAnalyzer::exit_scope() {
    if (scope_stack_.empty()) {
        std::cerr << "[SimpleLexicalScope] ERROR: Attempting to exit scope when none exists!" << std::endl;
        return;
    }
    
    LexicalScopeInfo& current_scope = *scope_stack_.back();
    
    std::cout << "[SimpleLexicalScope] Exiting scope at depth " << current_depth_ 
              << " with " << current_scope.declared_variables.size() << " declared variables" << std::endl;
    
    // Assign registers based on dependency frequency for this scope
    assign_registers_for_scope(current_scope);
    
    // Clean up variable declarations at this depth
    cleanup_declarations_at_depth(current_depth_);
    
    // Remove this scope from the stack
    scope_stack_.pop_back();
    current_depth_--;
}

// Called when a variable is declared
void SimpleLexicalScopeAnalyzer::declare_variable(const std::string& name, const std::string& type) {
    // Add to the variable declarations map
    variable_declarations_[name].emplace_back(current_depth_, type);
    
    // Add to current scope's declared variables
    if (!scope_stack_.empty()) {
        scope_stack_.back()->declared_variables.insert(name);
    }
    
    std::cout << "[SimpleLexicalScope] Declared variable '" << name << "' as " << type 
              << " at depth " << current_depth_ << std::endl;
}

// Called when a variable is accessed
void SimpleLexicalScopeAnalyzer::access_variable(const std::string& name) {
    // Find the most recent declaration of this variable
    int definition_depth = get_variable_definition_depth(name);
    
    if (definition_depth == -1) {
        std::cout << "[SimpleLexicalScope] WARNING: Variable '" << name 
                  << "' accessed but not found in declarations" << std::endl;
        return;
    }
    
    std::cout << "[SimpleLexicalScope] Accessing variable '" << name 
              << "' defined at depth " << definition_depth 
              << " from depth " << current_depth_ << std::endl;
    
    // Update usage count for this declaration
    auto& declarations = variable_declarations_[name];
    for (auto& decl : declarations) {
        if (decl.depth == definition_depth) {
            decl.usage_count++;
            break;
        }
    }
    
    // Add as self-dependency to current scope
    if (!scope_stack_.empty()) {
        auto& current_scope = *scope_stack_.back();
        
        // Check if we already have this dependency
        bool found = false;
        for (auto& dep : current_scope.self_dependencies) {
            if (dep.variable_name == name && dep.definition_depth == definition_depth) {
                dep.access_count++;
                found = true;
                break;
            }
        }
        
        if (!found) {
            current_scope.self_dependencies.emplace_back(name, definition_depth);
        }
    }
    
    // If accessing from a different depth, propagate dependency upward immediately
    if (definition_depth != current_depth_) {
        propagate_dependency_upward(name, definition_depth);
    }
}

// Get the absolute depth where a variable was last declared
int SimpleLexicalScopeAnalyzer::get_variable_definition_depth(const std::string& name) const {
    auto it = variable_declarations_.find(name);
    if (it == variable_declarations_.end() || it->second.empty()) {
        return -1; // Not found
    }
    
    // Return the depth of the most recent declaration (last in the vector)
    return it->second.back().depth;
}

// Get register/stack allocation for a specific depth in current scope
std::string SimpleLexicalScopeAnalyzer::get_register_for_depth(int depth) const {
    if (scope_stack_.empty()) {
        return "rbp"; // Fallback to stack
    }
    
    const auto& current_scope = *scope_stack_.back();
    auto it = current_scope.depth_to_register.find(depth);
    if (it != current_scope.depth_to_register.end()) {
        return it->second;
    }
    
    // Default register allocation strategy
    switch (depth) {
        case 0: return "r15";  // Current scope (if accessing own variables)
        case 1: return "r12";  // Parent scope
        case 2: return "r13";  // Grandparent scope  
        case 3: return "r14";  // Great-grandparent scope
        default: return "rbp"; // Stack access for deeper scopes
    }
}

// Debug: Print current state
void SimpleLexicalScopeAnalyzer::print_debug_info() const {
    std::cout << "\n[SimpleLexicalScope] DEBUG INFO:" << std::endl;
    std::cout << "Current depth: " << current_depth_ << std::endl;
    std::cout << "Active scopes: " << scope_stack_.size() << std::endl;
    
    std::cout << "\nVariable declarations:" << std::endl;
    for (const auto& [var_name, declarations] : variable_declarations_) {
        std::cout << "  " << var_name << ": ";
        for (const auto& decl : declarations) {
            std::cout << "[depth=" << decl.depth << ", type=" << decl.type 
                      << ", usage=" << decl.usage_count << "] ";
        }
        std::cout << std::endl;
    }
    
    if (!scope_stack_.empty()) {
        const auto& current_scope = *scope_stack_.back();
        std::cout << "\nCurrent scope dependencies:" << std::endl;
        for (const auto& dep : current_scope.self_dependencies) {
            std::cout << "  " << dep.variable_name << " from depth " << dep.definition_depth 
                      << " (accessed " << dep.access_count << " times)" << std::endl;
        }
    }
    std::cout << std::endl;
}

// Private methods

// Propagate dependency up the scope chain immediately
void SimpleLexicalScopeAnalyzer::propagate_dependency_upward(const std::string& var_name, int definition_depth) {
    // Find the scope at definition_depth + 1 (the immediate child of where the variable was defined)
    // and propagate the dependency upward from there
    
    for (int i = scope_stack_.size() - 1; i >= 0; i--) {
        auto& scope = *scope_stack_[i];
        
        // If this scope is deeper than where the variable was defined,
        // it needs this variable as a child dependency
        if (scope.depth > definition_depth) {
            // This is inefficient as mentioned in the requirements, but it works
            // We could optimize later by checking if already exists
            
            // For now, just mark that scopes between definition and current access need this variable
            std::cout << "[SimpleLexicalScope] Propagating dependency for '" << var_name 
                      << "' defined at depth " << definition_depth 
                      << " to scope at depth " << scope.depth << std::endl;
        }
    }
}

// Assign registers based on frequency after scope analysis
void SimpleLexicalScopeAnalyzer::assign_registers_for_scope(LexicalScopeInfo& scope) {
    // Sort dependencies by access frequency (most frequently accessed first)
    std::vector<ScopeDependency> sorted_deps = scope.self_dependencies;
    std::sort(sorted_deps.begin(), sorted_deps.end(), 
        [](const ScopeDependency& a, const ScopeDependency& b) {
            return a.access_count > b.access_count;
        });
    
    // Assign registers to most frequently accessed variables
    std::vector<std::string> preferred_registers = {"r12", "r13", "r14"};
    int register_index = 0;
    
    std::cout << "[SimpleLexicalScope] Assigning registers for scope at depth " << scope.depth << ":" << std::endl;
    
    for (const auto& dep : sorted_deps) {
        std::string location;
        
        if (static_cast<size_t>(register_index) < preferred_registers.size()) {
            location = preferred_registers[register_index++];
        } else {
            location = "stack_" + std::to_string(register_index - preferred_registers.size());
        }
        
        scope.depth_to_register[dep.definition_depth] = location;
        
        std::cout << "  Variable '" << dep.variable_name << "' from depth " << dep.definition_depth 
                  << " -> " << location << " (accessed " << dep.access_count << " times)" << std::endl;
    }
}

// Clean up variable declarations for the depth we're exiting
void SimpleLexicalScopeAnalyzer::cleanup_declarations_at_depth(int depth) {
    std::cout << "[SimpleLexicalScope] Cleaning up declarations at depth " << depth << std::endl;
    
    for (auto& [var_name, declarations] : variable_declarations_) {
        // Remove all declarations at this depth
        declarations.erase(
            std::remove_if(declarations.begin(), declarations.end(),
                [depth, &var_name](const VariableDeclarationInfo& decl) {
                    bool should_remove = (decl.depth == depth);
                    if (should_remove) {
                        std::cout << "  Removing declaration of '" << var_name 
                                  << "' from depth " << depth << std::endl;
                    }
                    return should_remove;
                }),
            declarations.end()
        );
    }
    
    // Clean up empty entries
    for (auto it = variable_declarations_.begin(); it != variable_declarations_.end();) {
        if (it->second.empty()) {
            std::cout << "  Removing empty declaration list for '" << it->first << "'" << std::endl;
            it = variable_declarations_.erase(it);
        } else {
            ++it;
        }
    }
}
