#include "function_runtime.h"
#include "function_instance.h"
#include <iostream>
#include <stdexcept>
#include <cstdlib>
#include <cstring>

//=============================================================================
// RUNTIME FUNCTION INSTANCE SUPPORT IMPLEMENTATION
//=============================================================================

// Runtime function copying for parameter passing
// This is called by generated code when functions need to be passed as parameters
extern "C" void* __runtime_copy_function_to_heap(void* local_function_var) {
    if (!local_function_var) {
        std::cerr << "[RUNTIME] ERROR: Cannot copy null function variable to heap" << std::endl;
        return nullptr;
    }
    
    // Extract function instance pointer from local variable
    void** var_ptr = static_cast<void**>(local_function_var);
    if (var_ptr[0] != reinterpret_cast<void*>(FUNCTION_TYPE_TAG)) {
        std::cerr << "[RUNTIME] ERROR: Variable is not a function (type tag mismatch)" << std::endl;
        return nullptr; // Not a function
    }
    
    void* function_instance = var_ptr[1];
    uint64_t* instance_data = static_cast<uint64_t*>(function_instance);
    uint64_t instance_size = instance_data[0];
    
    std::cout << "[RUNTIME] Copying function instance to heap (size: " << instance_size << " bytes)" << std::endl;
    
    // Allocate heap memory for type tag + pointer + instance data
    void* heap_copy = malloc(16 + instance_size);
    if (!heap_copy) {
        std::cerr << "[RUNTIME] ERROR: Failed to allocate heap memory for function copy" << std::endl;
        return nullptr;
    }
    
    uint64_t* heap_data = static_cast<uint64_t*>(heap_copy);
    heap_data[0] = FUNCTION_TYPE_TAG;
    heap_data[1] = reinterpret_cast<uint64_t>(heap_data + 2); // Points to following data
    
    // Copy function instance data
    memcpy(heap_data + 2, function_instance, instance_size);
    
    std::cout << "[RUNTIME] Successfully created heap copy of function instance" << std::endl;
    return heap_copy;
}

// Runtime type error handling
extern "C" void __throw_type_error(const char* message) {
    std::string error_msg = "TypeError: ";
    if (message) {
        error_msg += message;
    } else {
        error_msg += "Type error in function call";
    }
    
    std::cerr << "[RUNTIME] " << error_msg << std::endl;
    throw std::runtime_error(error_msg);
}

// Function instance allocation
extern "C" void* __allocate_function_instance(size_t total_size) {
    void* instance = malloc(total_size);
    if (!instance) {
        std::cerr << "[RUNTIME] ERROR: Failed to allocate function instance (" << total_size << " bytes)" << std::endl;
        return nullptr;
    }
    
    std::cout << "[RUNTIME] Allocated function instance: " << total_size << " bytes at " << instance << std::endl;
    return instance;
}

extern "C" void __deallocate_function_instance(void* instance) {
    if (instance) {
        std::cout << "[RUNTIME] Deallocating function instance at " << instance << std::endl;
        free(instance);
    }
}

// Lexical scope allocation for function execution
extern "C" void* __allocate_lexical_scope_heap_object(size_t scope_size) {
    if (scope_size == 0) {
        return nullptr; // No allocation needed for empty scopes
    }
    
    void* scope = malloc(scope_size);
    if (!scope) {
        std::cerr << "[RUNTIME] ERROR: Failed to allocate lexical scope (" << scope_size << " bytes)" << std::endl;
        return nullptr;
    }
    
    // Zero-initialize the scope memory
    memset(scope, 0, scope_size);
    
    std::cout << "[RUNTIME] Allocated lexical scope: " << scope_size << " bytes at " << scope << std::endl;
    return scope;
}

extern "C" void __deallocate_lexical_scope_heap_object(void* scope) {
    if (scope) {
        std::cout << "[RUNTIME] Deallocating lexical scope at " << scope << std::endl;
        free(scope);
    }
}

//=============================================================================
// UTILITY FUNCTIONS FOR DEBUGGING AND DIAGNOSTICS
//=============================================================================

// Debug function to print function instance details
extern "C" void __debug_print_function_instance(void* function_var, bool is_dynamic_value = false) {
    std::cout << "[RUNTIME_DEBUG] Function instance details:" << std::endl;
    
    if (is_dynamic_value) {
        FunctionDynamicValue* dyn_val = static_cast<FunctionDynamicValue*>(function_var);
        std::cout << "  Type: FunctionDynamicValue (Strategy 3)" << std::endl;
        std::cout << "  Type tag: 0x" << std::hex << dyn_val->type_tag << std::dec << std::endl;
        
        if (dyn_val->is_function()) {
            FunctionInstance* instance = dyn_val->get_function_instance();
            std::cout << "  Function instance size: " << instance->size << " bytes" << std::endl;
            std::cout << "  Function code address: " << instance->function_code_addr << std::endl;
            
            size_t scope_count = (instance->size - 16) / 8;
            std::cout << "  Captured scopes: " << scope_count << std::endl;
            
            void** scope_addrs = instance->get_scope_addresses();
            for (size_t i = 0; i < scope_count; i++) {
                std::cout << "    Scope " << i << ": " << scope_addrs[i] << std::endl;
            }
        } else {
            std::cout << "  Value is not a function" << std::endl;
        }
    } else {
        FunctionVariable* func_var = static_cast<FunctionVariable*>(function_var);
        std::cout << "  Type: FunctionVariable (Strategy 1/2)" << std::endl;
        std::cout << "  Type tag: 0x" << std::hex << func_var->type_tag << std::dec << std::endl;
        
        if (func_var->type_tag == FUNCTION_TYPE_TAG) {
            FunctionInstance* instance = static_cast<FunctionInstance*>(func_var->function_instance);
            std::cout << "  Function instance size: " << instance->size << " bytes" << std::endl;
            std::cout << "  Function code address: " << instance->function_code_addr << std::endl;
            
            size_t scope_count = (instance->size - 16) / 8;
            std::cout << "  Captured scopes: " << scope_count << std::endl;
            
            void** scope_addrs = instance->get_scope_addresses();
            for (size_t i = 0; i < scope_count; i++) {
                std::cout << "    Scope " << i << ": " << scope_addrs[i] << std::endl;
            }
        } else {
            std::cout << "  Variable is not a function" << std::endl;
        }
    }
}
