#pragma once

#include "compiler.h"
#include <vector>
#include <unordered_map>
#include <unordered_set>

/**
 * UltraScript Function Instance System
 * 
 * Implements the high-performance closure system from FUNCTION.md with:
 * - Pure assembly code generation (no runtime function calls)
 * - Optimal scope register allocation (R12/R13/R14 for frequent scopes)
 * - Function instance creation with exact scope dependencies
 * - Hidden parameter passing for lexical scopes
 * - Three function variable strategies (static, typed, dynamic)
 */

// Forward declarations
struct FunctionDecl;
struct FunctionExpression;
struct LexicalScopeNode;
class CodeGenerator;

// Forward declarations for existing types to avoid redefinition
struct DynamicValue; // Already defined in ultra_performance_array.h
struct FunctionInstance; // Already defined in function_instance.h

// Enhanced function analysis with complete scope dependency computation
struct CompleteFunctionAnalysis {
    // Core scope dependency data
    std::vector<int> needed_parent_scopes;              // Scope depths this function accesses
    std::vector<int> priority_sorted_parent_scopes;     // Same scopes, sorted by access frequency
    
    // Register allocation mapping
    std::vector<int> parent_location_indexes;           // child_register_index -> parent_source_index
    int num_registers_needed = 0;                       // 0-3 (r12/r13/r14)
    bool needs_r12 = false, needs_r13 = false, needs_r14 = false;
    
    // Function instance sizing
    size_t function_instance_size = 0;                  // 16 + (num_scopes * 8)
    size_t local_scope_size = 0;                        // From lexical scope analysis
    
    // Hidden parameter specification for function calls
    struct HiddenParameter {
        int source_register;        // Which register/stack location in caller: 12,13,14,-1(R15),-2(stack)
        int target_parameter_index; // Which hidden parameter position: 0,1,2,... 
        int scope_depth;            // Which scope depth this represents
    };
    std::vector<HiddenParameter> hidden_parameters;     // What to pass when calling this function
    
    // Descendant dependency tracking (for function variable sizing)
    std::unordered_set<int> all_descendant_scope_needs; // All scopes needed by this and descendants
};

// Function variable storage strategy classifier
enum class FunctionVariableStrategy {
    STATIC_SINGLE_ASSIGNMENT,    // Strategy 1: var f = function(){...}; (never reassigned)
    FUNCTION_TYPED,             // Strategy 2: var f: Function = ...; (Conservative Maximum Size)  
    ANY_TYPED_DYNAMIC           // Strategy 3: var f = function(){}; f = 5; (DynamicValue)
};

/**
 * Complete Function Instance System
 * Handles all aspects of function creation, calling, and closure management
 */
class FunctionInstanceSystem {
private:
    // Analysis results storage
    std::unordered_map<std::string, CompleteFunctionAnalysis> function_analysis_cache_;
    std::unordered_map<std::string, FunctionVariableStrategy> variable_strategies_;
    std::unordered_map<std::string, size_t> variable_max_function_sizes_;
    
    // Runtime state tracking
    std::unordered_map<std::string, void*> function_code_addresses_;
    std::vector<FunctionInstance*> heap_allocated_instances_;

public:
    // Phase 1: Complete static analysis computation
    void compute_complete_function_analysis(FunctionDecl* function, 
                                          const std::unordered_map<int, LexicalScopeNode*>& all_scopes);
    void compute_function_variable_strategies(const std::unordered_map<int, LexicalScopeNode*>& all_scopes);
    
    // Phase 2: Pure assembly function instance creation
    void emit_function_instance_creation_pure_asm(CodeGenerator& gen, 
                                                   FunctionDecl* function,
                                                   const CompleteFunctionAnalysis& analysis);
    void emit_function_instance_creation_pure_asm(CodeGenerator& gen, 
                                                   FunctionExpression* function,
                                                   const CompleteFunctionAnalysis& analysis);
    
    // Phase 3: Pure assembly function calling with hidden parameters
    void emit_function_call_with_hidden_parameters(CodeGenerator& gen,
                                                   const std::string& function_name,
                                                   const std::vector<std::unique_ptr<ASTNode>>& arguments);
    void emit_function_instance_call_pure_asm(CodeGenerator& gen,
                                             const std::string& function_variable,
                                             const std::vector<std::unique_ptr<ASTNode>>& arguments);
    
    // Phase 4: Pure assembly function prologue with scope register loading
    void emit_function_prologue_with_scope_loading(CodeGenerator& gen,
                                                   FunctionDecl* function,
                                                   const CompleteFunctionAnalysis& analysis);
    void emit_function_epilogue_with_scope_restoration(CodeGenerator& gen,
                                                       FunctionDecl* function,
                                                       const CompleteFunctionAnalysis& analysis);
    
    // Function variable storage emission
    void emit_function_variable_storage(CodeGenerator& gen,
                                       const std::string& variable_name,
                                       FunctionVariableStrategy strategy,
                                       size_t conservative_max_size = 0);
    
    // Type conversion for parameter passing (pure assembly)
    void emit_type_conversion_pure_asm(CodeGenerator& gen,
                                      DataType source_type,
                                      DataType target_type);
    
    // DynamicValue manipulation (pure assembly)
    void emit_dynamicvalue_creation_pure_asm(CodeGenerator& gen,
                                            DataType source_type);
    void emit_dynamicvalue_extraction_pure_asm(CodeGenerator& gen,
                                              DataType target_type);
    void emit_dynamicvalue_type_check_pure_asm(CodeGenerator& gen,
                                              DataType expected_type,
                                              const std::string& error_label);
    
    // Analysis result accessors
    const CompleteFunctionAnalysis& get_function_analysis(const std::string& function_name) const;
    FunctionVariableStrategy get_variable_strategy(const std::string& variable_name) const;
    size_t get_variable_max_function_size(const std::string& variable_name) const;
    
    // Memory management
    void cleanup_heap_instances();
    
private:
    // Analysis computation helpers
    void compute_scope_dependencies(FunctionDecl* function, 
                                   LexicalScopeNode* function_scope,
                                   const std::unordered_map<int, LexicalScopeNode*>& all_scopes,
                                   CompleteFunctionAnalysis& analysis);
    void compute_register_allocation(CompleteFunctionAnalysis& analysis);
    void compute_hidden_parameter_specification(const CompleteFunctionAnalysis& analysis,
                                               const std::unordered_map<int, LexicalScopeNode*>& all_scopes);
    void compute_descendant_dependencies(FunctionDecl* function,
                                       const std::unordered_map<int, LexicalScopeNode*>& all_scopes,
                                       CompleteFunctionAnalysis& analysis);
    
    // Assembly generation helpers
    void emit_scope_address_capture(CodeGenerator& gen,
                                   int scope_depth,
                                   int target_offset);
    void emit_register_scope_loading(CodeGenerator& gen,
                                    int source_parameter_index,
                                    int target_register);
    void emit_stack_scope_loading(CodeGenerator& gen,
                                 int source_parameter_index,
                                 int stack_offset);
    
    // Type system integration
    size_t compute_function_instance_size(const std::vector<int>& captured_scopes) const;
    size_t compute_conservative_max_size(const std::string& variable_name,
                                        const std::unordered_map<int, LexicalScopeNode*>& all_scopes) const;
};

// Global function instance system
extern FunctionInstanceSystem g_function_system;

// Utility functions for pure assembly generation
void emit_malloc_pure_asm(CodeGenerator& gen, size_t size);
void emit_memcpy_pure_asm(CodeGenerator& gen, void* dest, const void* src, size_t size);
void emit_heap_allocation_pure_asm(CodeGenerator& gen, size_t size);

// Function address patching integration
void register_function_for_address_patching(const std::string& function_name, 
                                          FunctionInstance* instance,
                                          size_t code_address_offset);
void patch_all_function_addresses();
