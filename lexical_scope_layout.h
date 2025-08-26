#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>

// Forward declarations
class FunctionExpression;

// *** COMPILE-TIME ONLY ***
// This entire system operates at compile-time to generate optimized assembly code.
// No runtime overhead - everything is baked into the generated assembly.

// Information about a variable within a lexical scope (COMPILE-TIME ANALYSIS ONLY)
struct ScopeVariable {
    std::string name;
    std::string type;                    // "int64", "float64", "string", "auto", etc.
    size_t offset;                       // Byte offset within the scope memory layout
    size_t size;                         // Size in bytes
    size_t alignment;                    // Required alignment (1, 2, 4, 8 bytes)
    int scope_level;                     // 0 = current scope, 1 = parent, 2 = grandparent, etc.
    bool is_let;                         // true for let, false for var
    bool escapes;                        // true if variable escapes to child scopes/goroutines
    
    ScopeVariable(const std::string& n, const std::string& t, size_t sz, size_t align = 8, bool let = false) 
        : name(n), type(t), offset(0), size(sz), alignment(align), scope_level(0), is_let(let), escapes(false) {}
    
    // Get size and alignment for different types
    static std::pair<size_t, size_t> get_type_info(const std::string& type_name) {
        if (type_name == "int8") return {1, 1};
        if (type_name == "int16") return {2, 2};
        if (type_name == "int32") return {4, 4};
        if (type_name == "int64") return {8, 8};
        if (type_name == "float32") return {4, 4};
        if (type_name == "float64") return {8, 8};
        if (type_name == "boolean") return {1, 1};
        if (type_name == "string") return {8, 8};      // Pointer to string object
        if (type_name == "array") return {8, 8};       // Pointer to array object
        if (type_name == "object") return {8, 8};      // Pointer to object
        // Default for "auto" or unknown types - assume DynamicValue pointer
        return {8, 8};
    }
};

// Information about a lexical scope's memory layout (COMPILE-TIME ANALYSIS ONLY)
struct LexicalScopeLayout {
    int scope_level;                              // 0 = current, 1 = parent, etc.
    std::vector<ScopeVariable> variables;         // Variables in this scope
    size_t total_size;                           // Total bytes needed for this scope
    bool allocate_on_heap;                       // True if scope escapes and needs heap allocation
    bool is_stack_allocated;                     // True if allocated on stack
    std::string base_register;                   // Register holding base address for this scope
    
    LexicalScopeLayout(int level) 
        : scope_level(level), total_size(0), allocate_on_heap(false), 
          is_stack_allocated(true), base_register("rbp") {}
};

// Complete lexical scope information for a function (COMPILE-TIME ANALYSIS ONLY)
struct FunctionScopeInfo {
    FunctionExpression* function;
    std::vector<std::unique_ptr<LexicalScopeLayout>> scopes;  // [0] = current, [1] = parent, etc.
    
    // Register allocation for scope addresses (COMPILE-TIME DECISION)
    // These registers will hold scope addresses: r12, r13, r14 (as per design doc)
    struct ScopeRegisterInfo {
        int scope_level;        // Which scope level this register points to
        std::string register_name; // "r12", "r13", "r14" (lowercase for x86 assembly)
    };
    std::vector<ScopeRegisterInfo> scope_registers;  // Which scopes are in registers
    
    FunctionScopeInfo(FunctionExpression* func) : function(func) {}
    
    // COMPILE-TIME: Get the scope layout for a specific level
    LexicalScopeLayout* get_scope_layout(int level) {
        if (level >= 0 && static_cast<size_t>(level) < scopes.size()) {
            return scopes[level].get();
        }
        return nullptr;
    }
    
    // COMPILE-TIME: Find variable info across all scopes
    const ScopeVariable* find_variable(const std::string& name) const {
        for (const auto& scope : scopes) {
            for (const auto& var : scope->variables) {
                if (var.name == name) {
                    return &var;
                }
            }
        }
        return nullptr;
    }
};

// *** PURE COMPILE-TIME ANALYSIS ENGINE ***
// Manages lexical scope layout and generates optimized assembly code.
// This class operates ONLY during compilation - no runtime functions!
class LexicalScopeManager {
private:
    // Map from function to its scope information (COMPILE-TIME DATA ONLY)
    std::unordered_map<FunctionExpression*, std::unique_ptr<FunctionScopeInfo>> function_scopes_;
    
public:
    LexicalScopeManager() = default;
    ~LexicalScopeManager() = default;
    
    // COMPILE-TIME: Create scope information for a function
    FunctionScopeInfo* create_function_scope_info(FunctionExpression* func);
    
    // COMPILE-TIME: Add a variable to a specific scope level
    void add_variable_to_scope(FunctionExpression* func, int scope_level, 
                              const std::string& var_name, const std::string& var_type);
    
    // COMPILE-TIME: Mark a variable as escaped (captured by child functions/goroutines)
    void mark_variable_as_escaped(FunctionExpression* func, const std::string& var_name);
    
    // COMPILE-TIME: Calculate memory layout for all scopes in a function
    void calculate_scope_layouts(FunctionExpression* func);
    
    // COMPILE-TIME: Determine which scope addresses need to be passed in registers
    void allocate_scope_registers(FunctionExpression* func);
    
    // COMPILE-TIME: Get the complete scope information for a function
    const FunctionScopeInfo* get_function_scope_info(FunctionExpression* func) const;
    
    // ASSEMBLY GENERATION: Generate assembly code for accessing a variable
    // Example output: "mov rax, [r12 + 24]  ; access parent scope variable 'x'"
    std::string generate_variable_access_asm(FunctionExpression* func, const std::string& var_name) const;
    
    // ASSEMBLY GENERATION: Generate assembly code for function prologue (setting up scope registers)
    // Example output: "mov r12, [rbp - 8]  ; load parent scope address into r12"
    std::string generate_function_prologue_asm(FunctionExpression* func) const;
    
    // COMPILE-TIME: Check if a function needs lexical scope address passing
    bool function_needs_scope_addresses(FunctionExpression* func) const;
    
    // COMPILE-TIME: Check if any scope in the function needs heap allocation
    bool function_needs_heap_allocation(FunctionExpression* func) const;
    
    // COMPILE-TIME: Get total heap size needed for all scopes that escape
    size_t get_total_heap_size(FunctionExpression* func) const;
    
    // COMPILE-TIME: Determine allocation strategy (stack vs heap) for each scope
    void determine_allocation_strategy(FunctionScopeInfo* info);
    
    // COMPILE-TIME: Analyze which parent scope levels a function needs access to
    void analyze_scope_dependencies(FunctionExpression* parent_func, FunctionExpression* child_func);
    
    // ASSEMBLY GENERATION: Generate assembly for passing scope addresses to child function
    // Handles register passing (r12, r13, r14) and stack overflow for 4+ scopes
    std::string generate_scope_address_passing_asm(FunctionExpression* parent_func, 
                                                   FunctionExpression* child_func) const;
    
    // ASSEMBLY GENERATION: Generate assembly for callback scope capture
    // Complex case where callback must carry lexical scope addresses
    std::string generate_callback_scope_capture_asm(FunctionExpression* callback_func,
                                                    const std::vector<int>& needed_scope_levels) const;
    
    // DEBUG COMPILE-TIME: Print all scope information
    void print_all_scope_info() const;
    
    // COMPILE-TIME: Get scope layout by level
    const LexicalScopeLayout* get_scope_layout(FunctionExpression* func, int scope_level) const;
    
    // COMPILE-TIME: Find variable in any scope
    const ScopeVariable* find_variable(FunctionExpression* func, const std::string& var_name) const;
    
    // COMPILE-TIME: Check if function requires lexical scope
    bool requires_lexical_scope(FunctionExpression* func) const;
    
    // DEBUG: Get debug information string
    std::string get_debug_info(FunctionExpression* func) const;
    
    // COMPILE-TIME: Clear scope info for a function
    void clear_scope_info(FunctionExpression* func);
    
    // COMPILE-TIME: Clear all scope information
    void clear_all();
    
private:
    // COMPILE-TIME: Calculate the size needed for a variable type
    size_t get_variable_size(const std::string& type) const;
    
    // COMPILE-TIME: Assign byte offsets to variables within a scope
    void calculate_variable_offsets(LexicalScopeLayout* scope_layout);
    
    // COMPILE-TIME: Determine optimal register allocation for scope addresses
    // Follows design pattern: r12 = parent scope, r13 = grandparent scope, r14 = great-grandparent
    void determine_register_allocation(FunctionScopeInfo* info);
};
