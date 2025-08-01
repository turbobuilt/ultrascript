#pragma once

#include "compiler.h"
#include <string>
#include <vector>
#include <unordered_map>

namespace ultraScript {

// ============================================================================
// TYPE-AWARE CONSOLE.LOG SYSTEM
// ============================================================================

class TypeAwareConsoleLog {
public:
    // Generate type-specific JIT code for console.log arguments
    static void generate_console_log_code(
        CodeGenerator& gen, 
        TypeInference& types,
        const std::vector<ExpressionNode*>& arguments
    );
    
    // Generate code for a single argument based on its type
    static void generate_typed_argument_code(
        CodeGenerator& gen,
        TypeInference& types,
        ExpressionNode* argument,
        bool is_first_argument
    );
    
    // Get the runtime function name for a specific data type
    static std::string get_console_log_function_name(DataType type);
    
    // Check if a type needs special handling
    static bool needs_special_handling(DataType type);
    
    // Generate code for any type (C++ fallback)
    static void generate_any_type_code(
        CodeGenerator& gen,
        TypeInference& types,
        ExpressionNode* argument
    );

private:
    // Map DataType to console.log function names
    static const std::unordered_map<DataType, std::string> type_function_map;
};

// ============================================================================
// RUNTIME FUNCTION DECLARATIONS
// ============================================================================

// Type-specific console.log functions
extern "C" void __console_log_int8(int8_t value);
extern "C" void __console_log_int16(int16_t value);
extern "C" void __console_log_int32(int32_t value);
extern "C" void __console_log_int64(int64_t value);
extern "C" void __console_log_uint8(uint8_t value);
extern "C" void __console_log_uint16(uint16_t value);
extern "C" void __console_log_uint32(uint32_t value);
extern "C" void __console_log_uint64(uint64_t value);
extern "C" void __console_log_float32(float value);
extern "C" void __console_log_float64(double value);
extern "C" void __console_log_boolean(bool value);
extern "C" void __console_log_string_ptr(void* string_ptr);
extern "C" void __console_log_array_ptr(void* array_ptr);
extern "C" void __console_log_object_ptr(void* object_ptr);
extern "C" void __console_log_function_ptr(void* function_ptr);

// Utility functions
extern "C" void __console_log_space_separator();
extern "C" void __console_log_final_newline();

// Any type console.log (reads DynamicValue and prints with proper type information)
extern "C" void __console_log_any_value_inspect(void* dynamic_value_ptr);

}
