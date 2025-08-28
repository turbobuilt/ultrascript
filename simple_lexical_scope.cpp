#include "simple_lexical_scope.h"
#include <algorithm>
#include <iostream>
#include <set>
#include <unordered_map>

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
    
    // Propagate dependencies to parent scope when this scope closes
    if (scope_stack_.size() > 1) {  // If there's a parent scope
        auto& parent_scope = *scope_stack_[scope_stack_.size() - 2];
        
        // Add all self_dependencies to parent's descendant_dependencies
        for (const auto& dep : current_scope.self_dependencies) {
            // Check if this dependency already exists in parent's descendant_dependencies
            bool found = false;
            for (auto& parent_dep : parent_scope.descendant_dependencies) {
                if (parent_dep.variable_name == dep.variable_name && 
                    parent_dep.definition_depth == dep.definition_depth) {
                    parent_dep.access_count += dep.access_count;
                    found = true;
                    break;
                }
            }
            
            if (!found) {
                parent_scope.descendant_dependencies.push_back(dep);
            }
        }
        
        // Add all descendant_dependencies to parent's descendant_dependencies
        for (const auto& dep : current_scope.descendant_dependencies) {
            // Check if this dependency already exists in parent's descendant_dependencies
            bool found = false;
            for (auto& parent_dep : parent_scope.descendant_dependencies) {
                if (parent_dep.variable_name == dep.variable_name && 
                    parent_dep.definition_depth == dep.definition_depth) {
                    parent_dep.access_count += dep.access_count;
                    found = true;
                    break;
                }
            }
            
            if (!found) {
                parent_scope.descendant_dependencies.push_back(dep);
            }
        }
        
        std::cout << "[SimpleLexicalScope] Propagated " << current_scope.self_dependencies.size() 
                  << " self dependencies and " << current_scope.descendant_dependencies.size() 
                  << " descendant dependencies to parent scope" << std::endl;
    }
    
    // Create priority-sorted vector: SELF dependencies first (sorted by access count), 
    // then descendant dependencies not in SELF (in any order, just for propagation)
    
    // First, sort SELF dependencies by access count per depth
    std::unordered_map<int, int> self_depth_access_counts;
    for (const auto& dep : current_scope.self_dependencies) {
        self_depth_access_counts[dep.definition_depth] += dep.access_count;
    }
    
    // Convert self dependencies to vector and sort by access count (highest first)
    std::vector<std::pair<int, int>> self_depth_counts;
    for (const auto& pair : self_depth_access_counts) {
        self_depth_counts.emplace_back(pair.first, pair.second);  // (depth, total_count)
    }
    
    std::sort(self_depth_counts.begin(), self_depth_counts.end(),
        [](const std::pair<int, int>& a, const std::pair<int, int>& b) {
            return a.second > b.second;  // Sort by access count (descending)
        });
    
    // Add SELF depths first (these get r12+8 style access)
    current_scope.priority_sorted_parent_scopes.clear();
    current_scope.priority_sorted_parent_scopes.reserve(
        self_depth_counts.size() + current_scope.descendant_dependencies.size());
    
    std::unordered_set<int> self_depths;
    for (const auto& pair : self_depth_counts) {
        current_scope.priority_sorted_parent_scopes.push_back(pair.first);
        self_depths.insert(pair.first);
    }
    
    // Then add descendant depths that are NOT already in SELF (just for propagation)
    for (const auto& dep : current_scope.descendant_dependencies) {
        if (self_depths.find(dep.definition_depth) == self_depths.end()) {
            // This depth is not in SELF dependencies, so add it (if not already added)
            bool already_added = false;
            for (int existing_depth : current_scope.priority_sorted_parent_scopes) {
                if (existing_depth == dep.definition_depth) {
                    already_added = true;
                    break;
                }
            }
            if (!already_added) {
                current_scope.priority_sorted_parent_scopes.push_back(dep.definition_depth);
            }
        }
    }
    
    std::cout << "[SimpleLexicalScope] Priority-sorted parent scopes: ";
    for (int depth : current_scope.priority_sorted_parent_scopes) {
        std::cout << depth << " ";
    }
    std::cout << std::endl;
    
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
    
    // Add as self-dependency to current scope (if accessing from different depth)
    if (!scope_stack_.empty() && definition_depth != current_depth_) {
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
        std::cout << "\nCurrent scope self dependencies:" << std::endl;
        for (const auto& dep : current_scope.self_dependencies) {
            std::cout << "  " << dep.variable_name << " from depth " << dep.definition_depth 
                      << " (accessed " << dep.access_count << " times)" << std::endl;
        }
        
        std::cout << "\nCurrent scope descendant dependencies:" << std::endl;
        for (const auto& dep : current_scope.descendant_dependencies) {
            std::cout << "  " << dep.variable_name << " from depth " << dep.definition_depth 
                      << " (accessed " << dep.access_count << " times)" << std::endl;
        }
    }
    std::cout << std::endl;
}

// Private methods

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
