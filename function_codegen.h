#pragma once

#include "compiler.h"
#include "function_instance.h"
#include "simple_lexical_scope.h"

//=============================================================================
// FUNCTION CALL CODE GENERATION
// Implementation of the three function call strategies from FUNCTION.md
//=============================================================================

class CodeGenerator; // Forward declaration

// Function call strategy determination
enum class FunctionCallStrategy {
    DIRECT_CALL,           // Strategy 1: Direct call, zero indirection
    POINTER_INDIRECTION,   // Strategy 2: Single pointer indirection
    DYNAMIC_TYPE_CHECK     // Strategy 3: Type check + branch + indirection
};

// Determine which calling strategy to use for a function variable
FunctionCallStrategy determine_function_call_strategy(const std::string& var_name, 
                                                     SimpleLexicalScopeAnalyzer* analyzer);

//=============================================================================
// FUNCTION CALL CODE GENERATION METHODS
//=============================================================================

// Generate function call code based on the appropriate strategy
void generate_function_call_code(CodeGenerator& gen, 
                                const std::string& function_var_name,
                                SimpleLexicalScopeAnalyzer* analyzer,
                                LexicalScopeNode* current_scope);

// Strategy 1: Direct function call (fastest)
void generate_direct_function_call(CodeGenerator& gen,
                                  const std::string& function_var_name,
                                  size_t variable_offset,
                                  bool is_local_scope);

// Strategy 2: Function-typed variable call (fast)
void generate_function_typed_call(CodeGenerator& gen,
                                 const std::string& function_var_name,
                                 size_t variable_offset,
                                 bool is_local_scope);

// Strategy 3: Dynamic type-checked call (slower but safe)
void generate_dynamic_function_call(CodeGenerator& gen,
                                   const std::string& function_var_name,
                                   size_t variable_offset,
                                   bool is_local_scope);

//=============================================================================
// FUNCTION CLOSURE SETUP CODE GENERATION
//=============================================================================

// Generate function prologue that sets up lexical scope registers
void generate_function_prologue_with_closure(CodeGenerator& gen,
                                            FunctionInstance* function_instance,
                                            const std::vector<LexicalScopeNode*>& captured_scopes);

// Generate code to load captured scope addresses into registers
void generate_scope_register_setup(CodeGenerator& gen,
                                  FunctionInstance* function_instance,
                                  const std::vector<int>& priority_sorted_scopes);

// Generate code to allocate local scope on heap
void generate_local_scope_allocation(CodeGenerator& gen,
                                   size_t local_scope_size);

//=============================================================================
// FUNCTION VARIABLE INITIALIZATION CODE GENERATION
//=============================================================================

// Generate initialization code for function variables based on strategy
void generate_function_variable_initialization(CodeGenerator& gen,
                                              const std::string& var_name,
                                              void* function_code_addr,
                                              const std::vector<void*>& captured_scopes,
                                              SimpleLexicalScopeAnalyzer::FunctionVariableStrategy strategy,
                                              size_t variable_offset,
                                              size_t max_function_size);

// Strategy 1 & 2: Initialize LocalFunctionInstance
void generate_local_function_instance_init(CodeGenerator& gen,
                                          size_t variable_offset,
                                          void* function_code_addr,
                                          const std::vector<void*>& captured_scopes,
                                          size_t total_function_size);

// Strategy 3: Initialize DynamicValue with function
void generate_dynamic_value_function_init(CodeGenerator& gen,
                                         size_t variable_offset,
                                         void* function_code_addr,
                                         const std::vector<void*>& captured_scopes,
                                         size_t max_function_size);

//=============================================================================
// FUNCTION PARAMETER PASSING CODE GENERATION
//=============================================================================

// Generate code to pass function as parameter (requires heap allocation)
void generate_function_parameter_passing(CodeGenerator& gen,
                                        const std::string& function_var_name,
                                        size_t variable_offset,
                                        bool is_dynamic_value);

// Generate code to receive function parameter in callee
void generate_function_parameter_receiving(CodeGenerator& gen,
                                          int parameter_index,
                                          bool is_typed_function_parameter);

//=============================================================================
// UTILITY FUNCTIONS FOR ASSEMBLY GENERATION
//=============================================================================

// Generate type checking assembly code
void generate_function_type_check(CodeGenerator& gen,
                                 size_t variable_offset,
                                 const std::string& error_label,
                                 bool is_dynamic_value);

// Generate error handling for "not a function" cases
void generate_function_call_error(CodeGenerator& gen,
                                 const std::string& variable_name);

// Generate scope register save/restore code
void generate_scope_register_save(CodeGenerator& gen);
void generate_scope_register_restore(CodeGenerator& gen);
