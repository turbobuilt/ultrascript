#include "console_log_overhaul.h"
#include "runtime.h"
#include "ultra_performance_array.h"
#include "x86_codegen_v2.h"  // For advanced floating-point support
#include <iostream>
#include <iomanip>
#include <mutex>
#include <cmath>

// ============================================================================
// STATIC MEMBER DEFINITIONS
// ============================================================================

const std::unordered_map<DataType, std::string> TypeAwareConsoleLog::type_function_map = {
    {DataType::INT8, "__console_log_int8"},
    {DataType::INT16, "__console_log_int16"},
    {DataType::INT32, "__console_log_int32"},
    {DataType::INT64, "__console_log_int64"},
    {DataType::UINT8, "__console_log_uint8"},
    {DataType::UINT16, "__console_log_uint16"},
    {DataType::UINT32, "__console_log_uint32"},
    {DataType::UINT64, "__console_log_uint64"},
    {DataType::FLOAT32, "__console_log_float32"},
    {DataType::FLOAT64, "__console_log_float64"},
    {DataType::BOOLEAN, "__console_log_boolean"},
    {DataType::STRING, "__console_log_string_ptr"},
    {DataType::ARRAY, "__console_log_array_ptr"},
    {DataType::CLASS_INSTANCE, "__console_log_object_ptr"},
    {DataType::FUNCTION, "__console_log_function_ptr"}
};

// ============================================================================
// TYPE-AWARE CONSOLE.LOG IMPLEMENTATION
// ============================================================================

void TypeAwareConsoleLog::generate_console_log_code(
    CodeGenerator& gen, 
    TypeInference& types,
    const std::vector<ExpressionNode*>& arguments
) {
    // For each argument, emit a space (except first) and then the typed output
    for (size_t i = 0; i < arguments.size(); i++) {
        bool is_first = (i == 0);
        
        if (!is_first) {
            // Emit space separator between arguments
            gen.emit_call("__console_log_space_separator");
        }
        
        // Generate type-specific code for this argument
        generate_typed_argument_code(gen, types, arguments[i], is_first);
    }
    
    // Emit final newline
    gen.emit_call("__console_log_final_newline");
}

void TypeAwareConsoleLog::generate_typed_argument_code(
    CodeGenerator& gen,
    TypeInference& types,
    ExpressionNode* argument,
    bool is_first_argument
) {
    // Generate code for the argument expression
    argument->generate_code(gen);
    
    // Get the result type from the argument
    DataType arg_type = argument->result_type;
    
    // Handle ANY types with C++ fallback
    if (arg_type == DataType::ANY) {
        generate_any_type_code(gen, types, argument);
        return;
    }
    
    // Handle FLOAT64 directly
    if (arg_type == DataType::FLOAT64) {
        // Already handled by get_console_log_function_name below
    }
    
    // Get the runtime function name for this type
    std::string func_name = get_console_log_function_name(arg_type);
    
    if (func_name.empty()) {
        // Fallback to any type for unknown types
        generate_any_type_code(gen, types, argument);
        return;
    }
    
    // Generate code based on type
    if (needs_special_handling(arg_type)) {
        // For pointer types (STRING, ARRAY, OBJECT, FUNCTION)
        // Argument result is already in RAX as a pointer
        gen.emit_mov_reg_reg(7, 0); // RDI = RAX
        gen.emit_call(func_name);
    } else if (is_floating_point_type(arg_type)) {
        // For floating-point types (FLOAT32, FLOAT64)
        // Use proper x86-64 calling convention - value must go to XMM0
        // Check if we have the advanced X86CodeGenV2 with floating-point support
        if (auto x86_gen_v2 = dynamic_cast<X86CodeGenV2*>(&gen)) {
            // Use high-performance floating-point calling convention
            x86_gen_v2->emit_call_with_double_arg(func_name, 0);  // RAX contains the bit pattern
        } else {
            // Fallback for other code generators - pass as integer (will print wrong but won't crash)
            gen.emit_mov_reg_reg(7, 0); // RDI = RAX
            gen.emit_call(func_name);
        }
    } else {
        // For integer types (INT*, UINT*, BOOLEAN)
        // Argument result is already in RAX as the value
        gen.emit_mov_reg_reg(7, 0); // RDI = RAX  
        gen.emit_call(func_name);
    }
}

void TypeAwareConsoleLog::generate_any_type_code(
    CodeGenerator& gen,
    TypeInference& types,
    ExpressionNode* argument
) {
    // For ANY types, we need to figure out the actual runtime type
    // Since ANY variables currently store raw values, we need to inspect what kind of expression this is
    // and generate appropriate code to detect the type
    
    // For now, fall back to the generic any type handler
    // The argument result is already in RAX
    gen.emit_mov_reg_reg(7, 0); // RDI = RAX (first argument register)
    gen.emit_call("__console_log_any_value_inspect");
}

std::string TypeAwareConsoleLog::get_console_log_function_name(DataType type) {
    auto it = type_function_map.find(type);
    return (it != type_function_map.end()) ? it->second : "";
}

bool TypeAwareConsoleLog::needs_special_handling(DataType type) {
    return type == DataType::STRING || 
           type == DataType::ARRAY || 
           type == DataType::CLASS_INSTANCE ||
           type == DataType::FUNCTION;
}

bool TypeAwareConsoleLog::is_floating_point_type(DataType type) {
    return type == DataType::FLOAT32 || type == DataType::FLOAT64;
}


// ============================================================================
// RUNTIME FUNCTION IMPLEMENTATIONS  
// ============================================================================

static std::mutex console_mutex;

extern "C" void __console_log_int8(int8_t value) {
    std::lock_guard<std::mutex> lock(console_mutex);
    std::cout << static_cast<int>(value);
    std::cout.flush();
}

extern "C" void __console_log_int16(int16_t value) {
    std::lock_guard<std::mutex> lock(console_mutex);
    std::cout << value;
    std::cout.flush();
}

extern "C" void __console_log_int32(int32_t value) {
    std::lock_guard<std::mutex> lock(console_mutex);
    std::cout << value;
    std::cout.flush();
}

extern "C" void __console_log_int64(int64_t value) {
    std::lock_guard<std::mutex> lock(console_mutex);
    std::cout << value;
    std::cout.flush();
}

extern "C" void __console_log_uint8(uint8_t value) {
    std::lock_guard<std::mutex> lock(console_mutex);
    std::cout << static_cast<unsigned int>(value);
    std::cout.flush();
}

extern "C" void __console_log_uint16(uint16_t value) {
    std::lock_guard<std::mutex> lock(console_mutex);
    std::cout << value;
    std::cout.flush();
}

extern "C" void __console_log_uint32(uint32_t value) {
    std::lock_guard<std::mutex> lock(console_mutex);
    std::cout << value;
    std::cout.flush();
}

extern "C" void __console_log_uint64(uint64_t value) {
    std::lock_guard<std::mutex> lock(console_mutex);
    std::cout << value;
    std::cout.flush();
}

extern "C" void __console_log_float32(float value) {
    std::lock_guard<std::mutex> lock(console_mutex);
    std::cout << value;
    std::cout.flush();
}

extern "C" void __console_log_float64(double value) {
    std::lock_guard<std::mutex> lock(console_mutex);
    std::cout << "[DEBUG] __console_log_float64 received: " << value << std::endl;
    std::cout << value;
    std::cout.flush();
}

extern "C" void __console_log_boolean(bool value) {
    std::lock_guard<std::mutex> lock(console_mutex);
    std::cout << (value ? "true" : "false");
    std::cout.flush();
}

extern "C" void __console_log_string_ptr(void* string_ptr) {
    std::lock_guard<std::mutex> lock(console_mutex);
    if (string_ptr) {
        // Handle GoTSString objects properly
        GoTSString* gots_str = static_cast<GoTSString*>(string_ptr);
        // Use data() and size() to properly handle null bytes
        std::cout.write(gots_str->data(), gots_str->size());
    } else {
        std::cout << "null";
    }
    std::cout.flush();
}

extern "C" void __console_log_array_ptr(void* array_ptr) {
    std::lock_guard<std::mutex> lock(console_mutex);
    if (array_ptr) {
        std::cout << "[Array]";
    } else {
        std::cout << "null";
    }
    std::cout.flush();
}

extern "C" void __console_log_object_ptr(void* object_ptr) {
    std::lock_guard<std::mutex> lock(console_mutex);
    if (object_ptr) {
        std::cout << "[Object]";
    } else {
        std::cout << "null";
    }
    std::cout.flush();
}

extern "C" void __console_log_function_ptr(void* function_ptr) {
    std::lock_guard<std::mutex> lock(console_mutex);
    if (function_ptr) {
        std::cout << "[Function]";
    } else {
        std::cout << "null";
    }
    std::cout.flush();
}

extern "C" void __console_log_space_separator() {
    std::lock_guard<std::mutex> lock(console_mutex);
    std::cout << " ";
    std::cout.flush();
}

extern "C" void __console_log_final_newline() {
    std::lock_guard<std::mutex> lock(console_mutex);
    std::cout << std::endl;
}

extern "C" void __console_log_any_value_inspect(void* dynamic_value_ptr) {
    std::lock_guard<std::mutex> lock(console_mutex);
    
    if (!dynamic_value_ptr) {
        std::cout << "null";
        std::cout.flush();
        return;
    }
    
    // Cast to DynamicValue and read the stored type and value
    DynamicValue* dyn_val = static_cast<DynamicValue*>(dynamic_value_ptr);
    
    // Debug output to see what type we got
    std::cout << "[DEBUG] DynamicValue type: " << static_cast<int>(dyn_val->type) << std::endl;
    std::cout.flush();
    
    // Use the stored type information to print correctly
    switch (dyn_val->type) {
        case DataType::INT8:
            std::cout << static_cast<int>(dyn_val->as<int8_t>());
            break;
        case DataType::INT16:
            std::cout << dyn_val->as<int16_t>();
            break;
        case DataType::INT32:
            std::cout << dyn_val->as<int32_t>();
            break;
        case DataType::INT64:
            std::cout << dyn_val->as<int64_t>();
            break;
        case DataType::UINT8:
            std::cout << static_cast<unsigned int>(dyn_val->as<uint8_t>());
            break;
        case DataType::UINT16:
            std::cout << dyn_val->as<uint16_t>();
            break;
        case DataType::UINT32:
            std::cout << dyn_val->as<uint32_t>();
            break;
        case DataType::UINT64:
            std::cout << dyn_val->as<uint64_t>();
            break;
        case DataType::FLOAT32:
            std::cout << dyn_val->as<float>();
            break;
        case DataType::FLOAT64:
            std::cout << dyn_val->as<double>();
            break;
        case DataType::BOOLEAN:
            std::cout << (dyn_val->as<bool>() ? "true" : "false");
            break;
        case DataType::STRING:
            std::cout << dyn_val->as<std::string>();
            break;
        default:
            std::cout << "[DynamicValue with unknown type " 
                      << static_cast<int>(dyn_val->type) << "]";
            break;
    }
    
    std::cout.flush();
}
