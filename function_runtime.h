#pragma once

#include "function_instance.h"
#include <cstdlib>
#include <cstring>

//=============================================================================
// RUNTIME FUNCTION INSTANCE SUPPORT
// These functions are called by generated code for function operations
//=============================================================================

// Runtime function copying for function parameter passing
// Called when functions need to be passed as parameters and require heap allocation
extern "C" void* __runtime_copy_function_to_heap(void* local_function_var);

// Runtime function type checking and error handling
extern "C" void __throw_type_error(const char* message);

// Function instance allocation helpers
extern "C" void* __allocate_function_instance(size_t total_size);
extern "C" void __deallocate_function_instance(void* instance);

// Lexical scope allocation for function execution
extern "C" void* __allocate_lexical_scope_heap_object(size_t scope_size);
extern "C" void __deallocate_lexical_scope_heap_object(void* scope);

//=============================================================================
// FUNCTION INSTANCE MANAGEMENT
//=============================================================================

// Create a function instance with the specified number of captured scopes
inline FunctionInstance* create_function_instance(void* function_code_addr, 
                                                 size_t scope_count,
                                                 void** captured_scopes = nullptr) {
    size_t total_size = 16 + (scope_count * 8);
    FunctionInstance* instance = static_cast<FunctionInstance*>(malloc(total_size));
    
    if (!instance) {
        return nullptr;
    }
    
    instance->size = total_size;
    instance->function_code_addr = function_code_addr;
    
    // Copy captured scope addresses if provided
    if (captured_scopes && scope_count > 0) {
        void** scope_addrs = instance->get_scope_addresses();
        memcpy(scope_addrs, captured_scopes, scope_count * 8);
    }
    
    return instance;
}

// Initialize a function variable with Conservative Maximum Size allocation
inline void initialize_function_variable(void* variable_memory, 
                                        void* function_code_addr,
                                        size_t scope_count,
                                        void** captured_scopes,
                                        size_t max_function_instance_size) {
    FunctionVariable* func_var = static_cast<FunctionVariable*>(variable_memory);
    
    // Set type tag
    func_var->type_tag = FUNCTION_TYPE_TAG;
    
    // Point to inline function instance data (right after the pointer)
    func_var->function_instance = reinterpret_cast<uint8_t*>(func_var) + 16;
    
    // Initialize the inline function instance
    FunctionInstance* instance = static_cast<FunctionInstance*>(func_var->function_instance);
    instance->size = 16 + (scope_count * 8);
    instance->function_code_addr = function_code_addr;
    
    // Copy captured scope addresses
    if (captured_scopes && scope_count > 0) {
        void** scope_addrs = instance->get_scope_addresses();
        memcpy(scope_addrs, captured_scopes, scope_count * 8);
    }
}

// Initialize a FunctionDynamicValue function variable
inline void initialize_dynamic_function_variable(void* variable_memory,
                                                void* function_code_addr,
                                                size_t scope_count,
                                                void** captured_scopes) {
    FunctionDynamicValue* dyn_val = static_cast<FunctionDynamicValue*>(variable_memory);
    
    // Set type tag to function
    dyn_val->type_tag = FUNCTION_TYPE_TAG;
    
    // Initialize the inline function instance (right after FunctionDynamicValue)
    FunctionInstance* instance = dyn_val->get_function_instance();
    instance->size = 16 + (scope_count * 8);
    instance->function_code_addr = function_code_addr;
    
    // Copy captured scope addresses
    if (captured_scopes && scope_count > 0) {
        void** scope_addrs = instance->get_scope_addresses();
        memcpy(scope_addrs, captured_scopes, scope_count * 8);
    }
}

// Check if a variable contains a callable function
inline bool is_callable_function(const void* variable_memory, bool is_dynamic_value = false) {
    if (is_dynamic_value) {
        const FunctionDynamicValue* dyn_val = static_cast<const FunctionDynamicValue*>(variable_memory);
        return dyn_val->is_function();
    } else {
        const FunctionVariable* func_var = static_cast<const FunctionVariable*>(variable_memory);
        return func_var->type_tag == FUNCTION_TYPE_TAG;
    }
}

// Get function instance from a variable (works for both FunctionVariable and FunctionDynamicValue)
inline FunctionInstance* get_function_instance_from_variable(void* variable_memory, bool is_dynamic_value = false) {
    if (is_dynamic_value) {
        FunctionDynamicValue* dyn_val = static_cast<FunctionDynamicValue*>(variable_memory);
        return dyn_val->get_function_instance();
    } else {
        FunctionVariable* func_var = static_cast<FunctionVariable*>(variable_memory);
        return static_cast<FunctionInstance*>(func_var->function_instance);
    }
}
