#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <set>
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
    size_t modification_count = 0; // How many times this variable is modified after first declaration
    
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
struct Identifier; // Forward declare Identifier

// Main lexical scope analyzer - simple and parse-time integrated
class SimpleLexicalScopeAnalyzer {
private:
    // Core data structure: variable_name -> list of declarations at different depths
    // Variable declarations tracking - use pointers for stable addresses
    std::unordered_map<std::string, std::vector<std::unique_ptr<VariableDeclarationInfo>>> variable_declarations_;
    
    // Structure to track unresolved variable references with their access context
    struct UnresolvedReference {
        Identifier* identifier;
        int access_depth;
        
        UnresolvedReference(Identifier* id, int depth) : identifier(id), access_depth(depth) {}
    };
    
    // Track unresolved variable references that need to be resolved when definitions are found
    std::unordered_map<std::string, std::vector<UnresolvedReference>> unresolved_references_;
    
    // Stack of active lexical scope NODES during parsing (immediate creation)
    std::vector<std::shared_ptr<LexicalScopeNode>> scope_stack_;
    
    // NEW: Map from depth to actual LexicalScopeNode objects for direct access
    std::unordered_map<int, LexicalScopeNode*> depth_to_scope_node_;
    
    // Storage for completed scope nodes to keep them alive
    std::vector<std::shared_ptr<LexicalScopeNode>> completed_scopes_;
    
    // Function assignment tracking for Conservative Maximum Size approach
    // Maps variable_name -> set of function instance sizes assigned to it
    std::unordered_map<std::string, std::set<size_t>> variable_function_sizes_;
    // Maps variable_name -> maximum function size (cached for performance)
    std::unordered_map<std::string, size_t> variable_max_function_size_;
    
    // NEW: Function declaration conflict tracking for hoisting
    // Maps variable_name -> FunctionDecl* if there's a conflicting function declaration
    std::unordered_map<std::string, class FunctionDecl*> function_declaration_conflicts_;
    // Track which variables have been promoted to DYNAMIC_VALUE due to conflicts
    std::unordered_set<std::string> hoisting_conflict_variables_;
    
    // NEW: Variable assignment type tracking for function variable classification
    // Maps variable_name -> set of DataTypes that have been assigned to this variable
    std::unordered_map<std::string, std::set<DataType>> variable_assignment_types_;
    // Track variables that have received non-function assignments after function assignments
    std::unordered_set<std::string> mixed_assignment_variables_;
    
    int current_depth_ = 0;      // Current absolute depth
    
public:
    SimpleLexicalScopeAnalyzer() {
        std::cout << "[SimpleLexicalScope] CONSTRUCTOR: initializing with current_depth_ = " << current_depth_ << std::endl;
        current_depth_ = 0;  // Make sure it's 0
        std::cout << "[SimpleLexicalScope] CONSTRUCTOR: after explicit set, current_depth_ = " << current_depth_ << std::endl;
    }
    ~SimpleLexicalScopeAnalyzer() {
        try {
            std::cout << "[SimpleLexicalScope] DESTRUCTOR: Starting cleanup" << std::endl;
            
            // Clear all containers safely
            if (!variable_declarations_.empty()) {
                std::cout << "[SimpleLexicalScope] DESTRUCTOR: Clearing variable declarations" << std::endl;
                variable_declarations_.clear();
            }
            
            if (!unresolved_references_.empty()) {
                std::cout << "[SimpleLexicalScope] DESTRUCTOR: Clearing unresolved references" << std::endl;
                unresolved_references_.clear();
            }
            
            if (!scope_stack_.empty()) {
                std::cout << "[SimpleLexicalScope] DESTRUCTOR: Clearing scope stack" << std::endl;
                scope_stack_.clear();
            }
            
            if (!depth_to_scope_node_.empty()) {
                std::cout << "[SimpleLexicalScope] DESTRUCTOR: Clearing depth to scope node map" << std::endl;
                depth_to_scope_node_.clear();
            }
            
            if (!completed_scopes_.empty()) {
                std::cout << "[SimpleLexicalScope] DESTRUCTOR: Clearing " << completed_scopes_.size() << " completed scopes" << std::endl;
                completed_scopes_.clear();
            }
            
            std::cout << "[SimpleLexicalScope] DESTRUCTOR: Cleanup completed successfully" << std::endl;
        } catch (const std::exception& e) {
            std::cout << "[SimpleLexicalScope] DESTRUCTOR: Exception during cleanup: " << e.what() << std::endl;
        } catch (...) {
            std::cout << "[SimpleLexicalScope] DESTRUCTOR: Unknown exception during cleanup" << std::endl;
        }
    }
    
    // Called when entering a new lexical scope (function, block, etc.)
    void enter_scope(bool is_function_scope = false);
    
    // Called when exiting a lexical scope - returns LexicalScopeNode with all scope info
    std::unique_ptr<LexicalScopeNode> exit_scope();
    
    // Called when a variable is declared
    void declare_variable(const std::string& name, const std::string& declaration_type, DataType data_type);
    void declare_variable(const std::string& name, const std::string& declaration_type); // Legacy overload
    
    // Called when a variable is accessed
    void access_variable(const std::string& name);
    
    // Called when a variable is modified/assigned to (tracks modification count)
    void modify_variable(const std::string& name);
    
    // Function registration methods
    void register_function_in_current_scope(class FunctionDecl* func_decl);
    void register_function_expression_in_current_scope(class FunctionExpression* func_expr);
    
    // Find the nearest function scope for proper function hoisting
    LexicalScopeNode* find_nearest_function_scope();
    
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
    
    // NEW: Get direct pointer to variable declaration info for ultra-fast access
    VariableDeclarationInfo* get_variable_declaration_info(const std::string& name) const;
    
    // Get the modification count for a variable (number of assignments after declaration)
    size_t get_variable_modification_count(const std::string& name) const;
    
    // Function instance size computation (based on FUNCTION.md specification)
    size_t compute_function_instance_size(const LexicalScopeNode* lexical_scope) const;
    
    // Function assignment tracking for variable-size function assignments
    void track_function_assignment(const std::string& var_name, size_t function_instance_size);
    void finalize_function_variable_sizes();
    size_t get_max_function_size(const std::string& var_name) const;
    bool has_tracked_function_sizes(const std::string& var_name) const;
    
    // NEW: Function variable type classification (for FUNCTION.md strategies)
    enum class FunctionVariableStrategy {
        STATIC_SINGLE_ASSIGNMENT,    // Strategy 1: Static single function assignment
        FUNCTION_TYPED,             // Strategy 2: Function-typed variables  
        ANY_TYPED_DYNAMIC          // Strategy 3: Any-typed variables with mixed assignment
    };
    
    FunctionVariableStrategy classify_function_variable_strategy(const std::string& var_name) const;
    bool is_static_single_function_assignment(const std::string& var_name) const;
    bool is_function_typed_variable(const std::string& var_name) const;
    bool is_mixed_assignment_variable(const std::string& var_name) const;
    DataType get_function_variable_storage_type(const std::string& var_name) const;
    
    // NEW: Variable assignment type tracking
    void track_variable_assignment_type(const std::string& var_name, DataType assigned_type);
    bool has_mixed_type_assignments(const std::string& var_name) const;
    
    // NEW: Function declaration conflict detection and hoisting support
    bool has_function_declaration_conflict(const std::string& var_name) const;
    class FunctionDecl* get_conflicting_function_declaration(const std::string& var_name) const;
    void mark_variable_as_hoisting_conflict(const std::string& var_name);
    bool is_hoisting_conflict_variable(const std::string& var_name) const;
    void resolve_hoisting_conflicts_in_current_scope(); // NEW: Resolve conflicts at scope close
    
    // NEW: Unresolved reference tracking for hoisting
    void add_unresolved_reference(const std::string& var_name, Identifier* identifier);
    void resolve_references_for_variable(const std::string& var_name);
    void resolve_all_unresolved_references(); // Called at end of parsing
    
    // NEW: Deferred variable packing (called during AST generation)
    void perform_deferred_packing_for_scope(LexicalScopeNode* scope_node);
    
    // NEW: Function static analysis for pure machine code generation (Phase 1)
    void compute_function_static_analysis(class FunctionDecl* function);
    void compute_all_function_static_analysis();
    
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
                             size_t& total_size,
                             const LexicalScopeNode* scope_node) const;
    
    // Phase 1: Function static analysis helpers
    void compute_parent_child_scope_mappings();
    void compute_scope_mapping_for_function(class FunctionDecl* child_func, LexicalScopeNode* parent_scope);
};
