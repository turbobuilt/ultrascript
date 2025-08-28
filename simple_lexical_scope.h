#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <iostream>

// Forward declare DataType enum
enum class DataType;

// Simple variable declaration info - tracks where variables are declared
struct VariableDeclarationInfo {
    int depth;                    // Absolute depth where declared (0 = global, 1 = first nested, etc.)
    std::string declaration_type; // "let", "const", "var"
    DataType data_type;          // Actual data type for size calculation
    size_t usage_count = 0;      // How many times this declaration is accessed
    
    // Packing information (calculated at scope close)
    size_t size_bytes = 0;       // Size in bytes
    size_t alignment = 0;        // Alignment requirement
    size_t offset = 0;           // Memory offset within scope
    
    VariableDeclarationInfo() : depth(0), declaration_type(""), data_type(static_cast<DataType>(0)) {}  // Default constructor
    VariableDeclarationInfo(int d, const std::string& decl_type, DataType dt) 
        : depth(d), declaration_type(decl_type), data_type(dt) {}
};

// Dependency info for lexical scopes
struct ScopeDependency {
    std::string variable_name;
    int definition_depth;        // Absolute depth where variable was defined
    size_t access_count = 1;     // How many times accessed from this scope
    
    ScopeDependency(const std::string& name, int depth) 
        : variable_name(name), definition_depth(depth) {}
};

// Forward declaration
class LexicalScopeNode;

// Main lexical scope analyzer - simple and parse-time integrated
class SimpleLexicalScopeAnalyzer {
private:
    // Core data structure: variable_name -> list of declarations at different depths
    std::unordered_map<std::string, std::vector<VariableDeclarationInfo>> variable_declarations_;
    
    // Stack of active lexical scope NODES during parsing (immediate creation)
    std::vector<std::unique_ptr<LexicalScopeNode>> scope_stack_;
    
    // NEW: Map from depth to actual LexicalScopeNode objects for direct access
    std::unordered_map<int, LexicalScopeNode*> depth_to_scope_node_;
    
    int current_depth_ = 0;      // Current absolute depth
    
public:
    SimpleLexicalScopeAnalyzer() = default;
    ~SimpleLexicalScopeAnalyzer() = default;
    
    // Called when entering a new lexical scope (function, block, etc.)
    void enter_scope();
    
    // Called when exiting a lexical scope - returns LexicalScopeNode with all scope info
    std::unique_ptr<LexicalScopeNode> exit_scope();
    
    // Called when a variable is declared
    void declare_variable(const std::string& name, const std::string& declaration_type, DataType data_type);
    void declare_variable(const std::string& name, const std::string& declaration_type); // Legacy overload
    
    // Called when a variable is accessed
    void access_variable(const std::string& name);
    
    // Get the current depth
    int get_current_depth() const { return current_depth_; }
    
    // Get the absolute depth where a variable was last declared
    int get_variable_definition_depth(const std::string& name) const;
    
    // NEW: Get direct pointer to scope node for a given depth (always available now)
    LexicalScopeNode* get_scope_node_for_depth(int depth) const;
    
    // NEW: Get direct pointer to scope node where a variable was defined (always available)
    LexicalScopeNode* get_definition_scope_for_variable(const std::string& name) const;
    
    // NEW: Get direct pointer to the current scope node (always available)
    LexicalScopeNode* get_current_scope_node() const;
    
    // Debug: Print current state
    void print_debug_info() const;
    
private:
    // Clean up variable declarations for the depth we're exiting
    void cleanup_declarations_at_depth(int depth);
    
    // Variable packing utilities
    size_t get_datatype_size(DataType type) const;
    size_t get_datatype_alignment(DataType type) const;
    void pack_scope_variables(const std::unordered_set<std::string>& variables, 
                             std::unordered_map<std::string, size_t>& offsets,
                             std::vector<std::string>& packed_order,
                             size_t& total_size) const;
};
