#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <iostream>

// Simple variable declaration info - tracks where variables are declared
struct VariableDeclarationInfo {
    int depth;                    // Absolute depth where declared (0 = global, 1 = first nested, etc.)
    std::string type;            // "let", "const", "var"
    size_t usage_count = 0;      // How many times this declaration is accessed
    
    VariableDeclarationInfo() : depth(0), type("") {}  // Default constructor
    VariableDeclarationInfo(int d, const std::string& t) : depth(d), type(t) {}
};

// Dependency info for lexical scopes
struct ScopeDependency {
    std::string variable_name;
    int definition_depth;        // Absolute depth where variable was defined
    size_t access_count = 1;     // How many times accessed from this scope
    
    ScopeDependency(const std::string& name, int depth) 
        : variable_name(name), definition_depth(depth) {}
};

// Information about a lexical scope
struct LexicalScopeInfo {
    int depth;                                           // Absolute depth of this scope
    std::unordered_set<std::string> declared_variables; // Variables declared in THIS scope
    std::vector<ScopeDependency> self_dependencies;     // Variables accessed in this scope from outer scopes
    std::unordered_set<ScopeDependency*> child_dependencies; // Child scopes need these variables
    
    // Register allocation (computed after analysis)
    std::unordered_map<int, std::string> depth_to_register; // Maps absolute depth to register/stack location
    
    LexicalScopeInfo() : depth(0) {}  // Default constructor
    LexicalScopeInfo(int d) : depth(d) {}
};

// Main lexical scope analyzer - simple and parse-time integrated
class SimpleLexicalScopeAnalyzer {
private:
    // Core data structure: variable_name -> list of declarations at different depths
    std::unordered_map<std::string, std::vector<VariableDeclarationInfo>> variable_declarations_;
    
    // Stack of active lexical scopes during parsing
    std::vector<std::unique_ptr<LexicalScopeInfo>> scope_stack_;
    
    int current_depth_ = 0;      // Current absolute depth
    
public:
    SimpleLexicalScopeAnalyzer() = default;
    ~SimpleLexicalScopeAnalyzer() = default;
    
    // Called when entering a new lexical scope (function, block, etc.)
    void enter_scope();
    
    // Called when exiting a lexical scope
    void exit_scope();
    
    // Called when a variable is declared
    void declare_variable(const std::string& name, const std::string& type);
    
    // Called when a variable is accessed
    void access_variable(const std::string& name);
    
    // Get the current depth
    int get_current_depth() const { return current_depth_; }
    
    // Get the absolute depth where a variable was last declared
    int get_variable_definition_depth(const std::string& name) const;
    
    // Get register/stack allocation for a specific depth in current scope
    std::string get_register_for_depth(int depth) const;
    
    // Debug: Print current state
    void print_debug_info() const;
    
private:
    // Propagate dependency up the scope chain immediately
    void propagate_dependency_upward(const std::string& var_name, int definition_depth);
    
    // Assign registers based on frequency after scope analysis
    void assign_registers_for_scope(LexicalScopeInfo& scope);
    
    // Clean up variable declarations for the depth we're exiting
    void cleanup_declarations_at_depth(int depth);
};

// Forward declaration - LexicalScopeNode is defined in compiler.h
class LexicalScopeNode;
