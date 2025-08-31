#pragma once

#include <cstdint>
#include <cstddef>

// Type tag constants for runtime type checking (defined first for all structures)
constexpr uint64_t FUNCTION_TYPE_TAG = 0xF0000000F0000000ULL;
constexpr uint64_t NUMBER_TYPE_TAG = 0x1000000000000000ULL;
constexpr uint64_t STRING_TYPE_TAG = 0x2000000000000000ULL;
constexpr uint64_t BOOLEAN_TYPE_TAG = 0x3000000000000000ULL;

// Forward declarations
enum class DataType;

//=============================================================================
// FUNCTION INSTANCE STRUCTURES - Core memory layouts for the function system
//=============================================================================

// Raw function instance structure - allocated inline in lexical scopes
// This matches the assembly layout described in FUNCTION.md
struct FunctionInstance {
    uint64_t size;              // Total size of this instance (since it's variable-length)
    void* function_code_addr;   // Address of the actual function machine code
    // Followed by variable number of lexical scope addresses:
    // void* lex_addr1;         // Most frequent scope (-> R12)
    // void* lex_addr2;         // 2nd most frequent scope (-> R13) 
    // void* lex_addr3;         // 3rd most frequent scope (-> R14)
    // ... additional scope addresses for stack if needed
    
    // Helper method to get lexical scope address by index
    void** get_scope_addresses() {
        return reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(this) + 16);
    }
    
    // Helper method to get scope address by index
    void* get_scope_address(size_t index) {
        void** scope_addrs = get_scope_addresses();
        return scope_addrs[index];
    }
    
    // Helper method to set scope address by index
    void set_scope_address(size_t index, void* addr) {
        void** scope_addrs = get_scope_addresses();
        scope_addrs[index] = addr;
    }
};

// Function variable structure for lexical scope storage
// Used for Strategy 1 (Static Single Assignment) and Strategy 2 (Function-Typed)
struct FunctionVariable {
    uint64_t type_tag;          // FUNCTION_TYPE_TAG for type identification
    void* function_instance;    // Pointer to inline FunctionInstance data
    // Followed by inline FunctionInstance data using Conservative Maximum Size
};

// DynamicValue wrapper for Strategy 3 (Any-Typed Variables with Mixed Assignment)
// This allows variables to hold functions OR other types with runtime type safety
struct FunctionDynamicValue {
    uint64_t type_tag;          // Runtime type identifier
    union {
        double number_value;
        void* pointer_value;
        bool boolean_value;
        // For functions: inline FunctionInstance data follows this structure
        // using Conservative Maximum Size allocation
    } value;
    
    // Constructors for different types
    FunctionDynamicValue() : type_tag(0) { value.number_value = 0.0; }
    
    FunctionDynamicValue(double num) : type_tag(NUMBER_TYPE_TAG) { 
        value.number_value = num; 
    }
    
    FunctionDynamicValue(bool b) : type_tag(BOOLEAN_TYPE_TAG) { 
        value.boolean_value = b; 
    }
    
    FunctionDynamicValue(void* ptr) : type_tag(STRING_TYPE_TAG) { 
        value.pointer_value = ptr; 
    }
    
    // Helper methods for function handling
    bool is_function() const {
        return type_tag == FUNCTION_TYPE_TAG;
    }
    
    FunctionInstance* get_function_instance() {
        if (!is_function()) return nullptr;
        // Function instance data follows immediately after this structure
        return reinterpret_cast<FunctionInstance*>(reinterpret_cast<uint8_t*>(this) + sizeof(FunctionDynamicValue));
    }
};

//=============================================================================
// FUNCTION VARIABLE SIZE COMPUTATION
//=============================================================================

// Compute total size for a function variable based on strategy
inline size_t compute_function_variable_size(size_t max_function_instance_size) {
    // From FUNCTION.md: Total size = 16 + max_function_instance_size
    // 16 bytes = 8 bytes type tag + 8 bytes function instance pointer
    return 16 + max_function_instance_size;
}

// Compute size for FunctionDynamicValue function variable
inline size_t compute_dynamic_function_variable_size(size_t max_function_instance_size) {
    // FunctionDynamicValue header + max function instance size
    return sizeof(FunctionDynamicValue) + max_function_instance_size;
}

//=============================================================================
// LEXICAL SCOPE REGISTER ALLOCATION CONSTANTS
//=============================================================================

// Register allocation for lexical scopes (from FUNCTION.md)
constexpr int CURRENT_SCOPE_REGISTER = 15;  // R15: Always current function's local scope
constexpr int PARENT_SCOPE_1_REGISTER = 12; // R12: Most frequent ancestor scope
constexpr int PARENT_SCOPE_2_REGISTER = 13; // R13: 2nd most frequent ancestor scope
constexpr int PARENT_SCOPE_3_REGISTER = 14; // R14: 3rd most frequent ancestor scope

// Maximum number of parent scopes that can be stored in registers
// Additional scopes beyond this are stored on stack
constexpr size_t MAX_REGISTER_PARENT_SCOPES = 3;
