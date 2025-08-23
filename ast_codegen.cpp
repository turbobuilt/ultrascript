#include "compiler.h"
#include "x86_codegen_improved.h"  // For X86CodeGenImproved class
#include "runtime.h"
#include "runtime_object.h"
#include "compilation_context.h"
#include "function_compilation_manager.h"
#include "console_log_overhaul.h"
#include "class_runtime_interface.h"
#include "dynamic_properties.h"
#include <iostream>
#include <unordered_map>
#include <cstring>
#include <cstdlib>
#include <stdexcept>
#include <string>

#include <queue>

// Simple global constant storage for imported constants
static std::unordered_map<std::string, double> global_imported_constants;

// Forward declarations for function ID registry
void __register_function_id(int64_t function_id, const std::string& function_name);
void ensure_lookup_function_by_id_registered();

// Forward declaration for fast runtime function lookup
extern "C" void* __lookup_function_fast(uint16_t func_id);

// Helper function to get the global deferred functions list
static std::vector<std::pair<std::string, FunctionExpression*>>& get_deferred_functions() {
    static std::vector<std::pair<std::string, FunctionExpression*>> deferred_functions;
    return deferred_functions;
}

// Static member definition
GoTSCompiler* ConstructorDecl::current_compiler_context = nullptr;

void NumberLiteral::generate_code(CodeGenerator& gen, TypeInference& types) {
    std::cout << "[DEBUG] NumberLiteral::generate_code - value=" << value << std::endl;
    std::cout.flush();
    
    // Check if we're in a property assignment context with a specific type
    DataType property_context = types.get_current_property_assignment_type();
    
    // Check if we're in an array element context with a specific type
    DataType element_context = types.get_current_element_type_context();
    
    // Priority: property assignment context > array element context > default
    DataType target_type = DataType::ANY;
    if (property_context != DataType::ANY) {
        target_type = property_context;
        std::cout << "[DEBUG] NumberLiteral: Using property assignment context: " << static_cast<int>(target_type) << std::endl;
    } else if (element_context != DataType::ANY) {
        target_type = element_context;
        std::cout << "[DEBUG] NumberLiteral: Using array element context: " << static_cast<int>(target_type) << std::endl;
    }
    
    if (target_type != DataType::ANY) {
        // We're in a typed context - generate the value according to the target type
        switch (target_type) {
            case DataType::INT64:
            case DataType::UINT64: {
                // Convert to int64
                int64_t int_value = static_cast<int64_t>(value);
                std::cout << "[DEBUG] NumberLiteral: Converting " << value << " to int64: " << int_value << std::endl;
                gen.emit_mov_reg_imm(0, int_value);
                result_type = target_type;
                break;
            }
            case DataType::INT32:
            case DataType::UINT32: {
                // Convert to int32
                int32_t int_value = static_cast<int32_t>(value);
                std::cout << "[DEBUG] NumberLiteral: Converting " << value << " to int32: " << int_value << std::endl;
                gen.emit_mov_reg_imm(0, static_cast<int64_t>(int_value));
                result_type = target_type;
                break;
            }
            case DataType::INT16:
            case DataType::UINT16: {
                // Convert to int16
                int16_t int_value = static_cast<int16_t>(value);
                std::cout << "[DEBUG] NumberLiteral: Converting " << value << " to int16: " << int_value << std::endl;
                gen.emit_mov_reg_imm(0, static_cast<int64_t>(int_value));
                result_type = target_type;
                break;
            }
            case DataType::INT8:
            case DataType::UINT8: {
                // Convert to int8
                int8_t int_value = static_cast<int8_t>(value);
                std::cout << "[DEBUG] NumberLiteral: Converting " << value << " to int8: " << int_value << std::endl;
                gen.emit_mov_reg_imm(0, static_cast<int64_t>(int_value));
                result_type = target_type;
                break;
            }
            case DataType::FLOAT32: {
                // Convert to float32
                float float_value = static_cast<float>(value);
                union { float f; int32_t i; } converter32;
                converter32.f = float_value;
                std::cout << "[DEBUG] NumberLiteral: Converting " << value << " to float32: " << float_value << std::endl;
                gen.emit_mov_reg_imm(0, static_cast<int64_t>(converter32.i));
                result_type = DataType::FLOAT32;
                break;
            }
            case DataType::BOOLEAN: {
                // Convert number to boolean (0 = false, non-zero = true)
                bool bool_value = (value != 0.0);
                std::cout << "[DEBUG] NumberLiteral: Converting " << value << " to boolean: " << bool_value << std::endl;
                gen.emit_mov_reg_imm(0, bool_value ? 1 : 0);
                result_type = DataType::BOOLEAN;
                break;
            }
            case DataType::FLOAT64:
            default: {
                // Default float64 behavior (original behavior)
                union { double d; int64_t i; } converter;
                converter.d = value;
                std::cout << "[DEBUG] NumberLiteral: double value " << value << " converts to int64 bits: " << converter.i << std::endl;
                gen.emit_mov_reg_imm(0, converter.i);
                result_type = DataType::FLOAT64;
                break;
            }
        }
    } else {
        // Original behavior for non-array contexts
        union { double d; int64_t i; } converter;
        converter.d = value;
        std::cout << "[DEBUG] NumberLiteral: double value " << value << " converts to int64 bits: " << converter.i << std::endl;
        gen.emit_mov_reg_imm(0, converter.i);
        result_type = DataType::FLOAT64;  // JavaScript compatibility: number literals are float64
    }
    
    std::cout.flush();
}

void StringLiteral::generate_code(CodeGenerator& gen, TypeInference& types) {
    std::cout << "[DEBUG] StringLiteral::generate_code - value=\"" << value << "\"" << std::endl;
    std::cout.flush();
    
    // Check if we're in a property assignment context with a specific type
    DataType property_context = types.get_current_property_assignment_type();
    
    // Check if we're in an array element context with a specific type
    DataType element_context = types.get_current_element_type_context();
    
    // Priority: property assignment context > array element context > default
    DataType target_type = DataType::ANY;
    if (property_context != DataType::ANY) {
        target_type = property_context;
        std::cout << "[DEBUG] StringLiteral: Using property assignment context: " << static_cast<int>(target_type) << std::endl;
    } else if (element_context != DataType::ANY) {
        target_type = element_context;
        std::cout << "[DEBUG] StringLiteral: Using array element context: " << static_cast<int>(target_type) << std::endl;
    }
    
    // For non-string target types, we need to convert or fail
    if (target_type != DataType::ANY && target_type != DataType::STRING) {
        // Handle type conversion from string literal to other types
        switch (target_type) {
            case DataType::INT64:
            case DataType::UINT64: {
                // Try to parse string as integer
                try {
                    int64_t int_value = std::stoll(value);
                    std::cout << "[DEBUG] StringLiteral: Converting \"" << value << "\" to int64: " << int_value << std::endl;
                    gen.emit_mov_reg_imm(0, int_value);
                    result_type = target_type;
                    return;
                } catch (const std::exception&) {
                    throw std::runtime_error("Cannot convert string literal \"" + value + "\" to integer type");
                }
            }
            case DataType::INT32:
            case DataType::UINT32: {
                // Try to parse string as 32-bit integer
                try {
                    int32_t int_value = std::stoi(value);
                    std::cout << "[DEBUG] StringLiteral: Converting \"" << value << "\" to int32: " << int_value << std::endl;
                    gen.emit_mov_reg_imm(0, static_cast<int64_t>(int_value));
                    result_type = target_type;
                    return;
                } catch (const std::exception&) {
                    throw std::runtime_error("Cannot convert string literal \"" + value + "\" to 32-bit integer type");
                }
            }
            case DataType::FLOAT64: {
                // Try to parse string as double
                try {
                    double float_value = std::stod(value);
                    union { double d; int64_t i; } converter;
                    converter.d = float_value;
                    std::cout << "[DEBUG] StringLiteral: Converting \"" << value << "\" to float64: " << float_value << std::endl;
                    gen.emit_mov_reg_imm(0, converter.i);
                    result_type = DataType::FLOAT64;
                    return;
                } catch (const std::exception&) {
                    throw std::runtime_error("Cannot convert string literal \"" + value + "\" to float64 type");
                }
            }
            case DataType::FLOAT32: {
                // Try to parse string as float
                try {
                    float float_value = std::stof(value);
                    union { float f; int32_t i; } converter32;
                    converter32.f = float_value;
                    std::cout << "[DEBUG] StringLiteral: Converting \"" << value << "\" to float32: " << float_value << std::endl;
                    gen.emit_mov_reg_imm(0, static_cast<int64_t>(converter32.i));
                    result_type = DataType::FLOAT32;
                    return;
                } catch (const std::exception&) {
                    throw std::runtime_error("Cannot convert string literal \"" + value + "\" to float32 type");
                }
            }
            case DataType::BOOLEAN: {
                // Convert string to boolean based on JavaScript rules
                bool bool_value = !value.empty() && value != "0" && value != "false";
                std::cout << "[DEBUG] StringLiteral: Converting \"" << value << "\" to boolean: " << bool_value << std::endl;
                gen.emit_mov_reg_imm(0, bool_value ? 1 : 0);
                result_type = DataType::BOOLEAN;
                return;
            }
            default:
                throw std::runtime_error("Cannot convert string literal to target type " + std::to_string(static_cast<int>(target_type)));
        }
    }
    
    // Default string handling - high-performance string creation using interned strings for literals
    // This provides both memory efficiency and extremely fast string creation
    
    if (value.empty()) {
        // Handle empty string efficiently - call __string_create_empty()
        gen.emit_call("__string_create_empty");
    } else {
        // SAFE APPROACH: Use string interning with proper fixed StringPool
        // The StringPool now uses std::string keys instead of char* keys,
        // which makes it safe to use with temporary string data
        
        // Store the string content safely for the call
        // We need to ensure the string data is available during the __string_intern call
        static std::unordered_map<std::string, const char*> literal_storage;
        
        // Check if we already have this literal stored
        auto it = literal_storage.find(value);
        const char* str_ptr;
        if (it != literal_storage.end()) {
            str_ptr = it->second;
        } else {
            // Allocate permanent storage for this literal
            char* permanent_str = new char[value.length() + 1];
            strcpy(permanent_str, value.c_str());
            literal_storage[value] = permanent_str;
            str_ptr = permanent_str;
        }
        
        uint64_t str_literal_addr = reinterpret_cast<uint64_t>(str_ptr);
        gen.emit_mov_reg_imm(7, static_cast<int64_t>(str_literal_addr)); // RDI = first argument
        
        // Use string interning for memory efficiency
        gen.emit_call("__string_intern");
    }
    
    // Result is now in RAX (pointer to GoTSString)
    result_type = DataType::STRING;
}

void RegexLiteral::generate_code(CodeGenerator& gen, TypeInference&) {
    // Create a runtime regex object from pattern and flags
    
    // Store pattern string
    static std::unordered_map<std::string, const char*> pattern_storage;
    
    // Check if we already have this pattern stored
    auto pattern_it = pattern_storage.find(pattern);
    const char* pattern_ptr;
    if (pattern_it != pattern_storage.end()) {
        pattern_ptr = pattern_it->second;
    } else {
        // Allocate permanent storage for this pattern
        char* permanent_pattern = new char[pattern.length() + 1];
        strcpy(permanent_pattern, pattern.c_str());
        pattern_storage[pattern] = permanent_pattern;
        pattern_ptr = permanent_pattern;
    }
    
    // Store flags string
    static std::unordered_map<std::string, const char*> flags_storage;
    
    auto flags_it = flags_storage.find(flags);
    const char* flags_ptr;
    if (flags_it != flags_storage.end()) {
        flags_ptr = flags_it->second;
    } else {
        // Allocate permanent storage for this flags string
        char* permanent_flags = new char[flags.length() + 1];
        strcpy(permanent_flags, flags.c_str());
        flags_storage[flags] = permanent_flags;
        flags_ptr = permanent_flags;
    }
    
    // CREATIVE FIX: Use a safer method to pass the pattern
    // Instead of loading address directly, use a pattern ID lookup system
    
    // Store pattern in a safe global registry with integer IDs
    static std::unordered_map<std::string, int> pattern_registry;
    static int next_pattern_id = 1;
    
    int pattern_id;
    auto registry_it = pattern_registry.find(pattern);
    if (registry_it != pattern_registry.end()) {
        pattern_id = registry_it->second;
    } else {
        pattern_id = next_pattern_id++;
        pattern_registry[pattern] = pattern_id;
    }
    
    // Register the pattern with the runtime first
    gen.emit_mov_reg_imm(7, reinterpret_cast<int64_t>(pattern_ptr)); // RDI = pattern string (permanent storage)
    gen.emit_call("__register_regex_pattern");
    
    // The function returns the pattern ID in RAX, use it to create the regex
    gen.emit_mov_reg_reg(7, 0); // RDI = RAX (pattern ID returned)
    gen.emit_call("__regex_create_by_id");
    
    // Result is now in RAX (pointer to GoTSRegExp)
    result_type = DataType::REGEX;
}

void Identifier::generate_code(CodeGenerator& gen, TypeInference& types) {
    // SPECIAL CASE: Handle boolean literals
    if (name == "true") {
        // Check if we're in a property assignment context for type conversion
        DataType property_context = types.get_current_property_assignment_type();
        DataType element_context = types.get_current_element_type_context();
        DataType target_type = (property_context != DataType::ANY) ? property_context : 
                              (element_context != DataType::ANY) ? element_context : DataType::BOOLEAN;
        
        std::cout << "[DEBUG] Boolean literal 'true' with target type: " << static_cast<int>(target_type) << std::endl;
        
        switch (target_type) {
            case DataType::BOOLEAN:
                gen.emit_mov_reg_imm(0, 1);
                result_type = DataType::BOOLEAN;
                break;
            case DataType::INT64:
            case DataType::UINT64:
            case DataType::INT32:
            case DataType::UINT32:
            case DataType::INT16:
            case DataType::UINT16:
            case DataType::INT8:
            case DataType::UINT8:
                gen.emit_mov_reg_imm(0, 1);
                result_type = target_type;
                break;
            case DataType::FLOAT64: {
                union { double d; int64_t i; } converter;
                converter.d = 1.0;
                gen.emit_mov_reg_imm(0, converter.i);
                result_type = DataType::FLOAT64;
                break;
            }
            case DataType::FLOAT32: {
                union { float f; int32_t i; } converter32;
                converter32.f = 1.0f;
                gen.emit_mov_reg_imm(0, static_cast<int64_t>(converter32.i));
                result_type = DataType::FLOAT32;
                break;
            }
            default:
                gen.emit_mov_reg_imm(0, 1);
                result_type = DataType::BOOLEAN;
                break;
        }
        return;
    }
    
    if (name == "false") {
        // Check if we're in a property assignment context for type conversion
        DataType property_context = types.get_current_property_assignment_type();
        DataType element_context = types.get_current_element_type_context();
        DataType target_type = (property_context != DataType::ANY) ? property_context : 
                              (element_context != DataType::ANY) ? element_context : DataType::BOOLEAN;
        
        std::cout << "[DEBUG] Boolean literal 'false' with target type: " << static_cast<int>(target_type) << std::endl;
        
        switch (target_type) {
            case DataType::BOOLEAN:
                gen.emit_mov_reg_imm(0, 0);
                result_type = DataType::BOOLEAN;
                break;
            case DataType::INT64:
            case DataType::UINT64:
            case DataType::INT32:
            case DataType::UINT32:
            case DataType::INT16:
            case DataType::UINT16:
            case DataType::INT8:
            case DataType::UINT8:
                gen.emit_mov_reg_imm(0, 0);
                result_type = target_type;
                break;
            case DataType::FLOAT64: {
                union { double d; int64_t i; } converter;
                converter.d = 0.0;
                gen.emit_mov_reg_imm(0, converter.i);
                result_type = DataType::FLOAT64;
                break;
            }
            case DataType::FLOAT32: {
                union { float f; int32_t i; } converter32;
                converter32.f = 0.0f;
                gen.emit_mov_reg_imm(0, static_cast<int64_t>(converter32.i));
                result_type = DataType::FLOAT32;
                break;
            }
            default:
                gen.emit_mov_reg_imm(0, 0);
                result_type = DataType::BOOLEAN;
                break;
        }
        return;
    }
    
    // SPECIAL CASE: Handle "runtime" global object
    if (name == "runtime") {
        // The runtime object is a special global that doesn't need any code generation
        // PropertyAccess and MethodCall nodes will optimize runtime.x.y() calls
        result_type = DataType::RUNTIME_OBJECT;
        return;
    }
    
    // Check if this is a global imported constant first
    auto it = global_imported_constants.find(name);
    if (it != global_imported_constants.end()) {
        // Load the constant value directly as an immediate using bit preservation
        union {
            double f;
            int64_t i;
        } converter;
        converter.f = it->second;
        gen.emit_mov_reg_imm(0, converter.i);
        result_type = DataType::FLOAT64;
        return;
    }
    
    // For now, let function names fall through to variable lookup
    // TODO: Implement proper function reference handling
    
    // Fall back to local variable lookup
    DataType var_type = types.get_variable_type(name);
    
    // If variable not found locally, try implicit 'this.property' access
    if (var_type == DataType::ANY && !types.variable_exists(name)) {
        std::string current_class = types.get_current_class_context();
        if (!current_class.empty()) {
            // We're in a method - check if this identifier could be a class property
            auto* compiler = get_current_compiler();
            if (compiler) {
                ClassInfo* class_info = compiler->get_class(current_class);
                if (class_info) {
                    // Check if this property exists on the current class
                    for (size_t i = 0; i < class_info->fields.size(); ++i) {
                        if (class_info->fields[i].name == name) {
                            std::cout << "[DEBUG] Identifier: Converting '" << name << "' to implicit 'this." << name << "'" << std::endl;
                            
                            // PERFORMANCE: Direct property access with calculated offset
                            // Object layout: [class_name_ptr][property_count][ref_count][dynamic_map_ptr][property0][property1]...
                            // Properties start at offset 32 (4 * 8 bytes for metadata)
                            int64_t property_offset = OBJECT_PROPERTIES_START_OFFSET + (i * 8); // Each property is 8 bytes
                            DataType property_type = class_info->fields[i].type;
                            
                            // Load 'this' from stack offset -8 (where method prologue stored it)
                            gen.emit_mov_reg_mem(0, -8); // RAX = object_address (this)
                            
                            // LIGHTNING FAST: Direct offset access - zero performance penalty like C++
                            gen.emit_mov_reg_reg_offset(0, 0, property_offset); // RAX = [RAX + property_offset]
                            
                            result_type = property_type;
                            std::cout << "[DEBUG] Identifier: Generated implicit this." << name << " access at offset " << property_offset << std::endl;
                            return;
                        }
                    }
                }
            }
        }
        
        // If we reach here, the identifier was not found as a local variable or class property
        throw std::runtime_error("Undefined variable: " + name);
    }
    
    result_type = var_type;
    
    // Get the actual stack offset for this variable
    int64_t offset = types.get_variable_offset(name);
    if (offset == 0) {
        // Default to -8 for backward compatibility
        offset = -8;
    }
    
    gen.emit_mov_reg_mem(0, offset);
}

void BinaryOp::generate_code(CodeGenerator& gen, TypeInference& types) {
    if (left) {
        left->generate_code(gen, types);
        // Push left operand result onto stack to protect it during right operand evaluation
        gen.emit_sub_reg_imm(4, 8);   // sub rsp, 8 (allocate stack space)
        // Store to RSP-relative location to match the RSP-relative load later
        gen.emit_mov_mem_rsp_reg(0, 0);   // mov [rsp], rax (save left operand on stack)
    }
    
    if (right) {
        right->generate_code(gen, types);
    }
    
    DataType left_type = left ? left->result_type : DataType::ANY;
    DataType right_type = right ? right->result_type : DataType::ANY;
    
    switch (op) {
        case TokenType::PLUS:
            if (left_type == DataType::STRING || right_type == DataType::STRING) {
                result_type = DataType::STRING;
                if (left) {
                    // String concatenation - extremely optimized
                    // Right operand (string) is in RAX
                    gen.emit_mov_reg_reg(6, 0);   // mov rsi, rax (right operand -> second argument)
                    
                    // Pop left operand from stack
                    auto* x86_gen = static_cast<X86CodeGenImproved*>(&gen);
                    x86_gen->emit_mov_reg_mem(7, 0);   // mov rdi, [rsp] (left operand -> first argument)
                    gen.emit_add_reg_imm(4, 8);   // add rsp, 8 (restore stack)
                    
                    // Robust string concatenation with proper type handling
                    if (left_type == DataType::STRING && right_type == DataType::STRING) {
                        // Both are GoTSString* - use optimized __string_concat
                        // Parameters: RDI = left GoTSString*, RSI = right GoTSString*
                        gen.emit_call("__string_concat");
                    } else if (left_type == DataType::STRING && right_type != DataType::STRING) {
                        // Left is GoTSString*, right needs conversion to string
                        // For now, assume right operand is a C string or can be cast to one
                        // In future: add runtime type conversion here
                        // Parameters: RDI = left GoTSString*, RSI = right const char*
                        gen.emit_call("__string_concat_cstr");
                    } else if (left_type != DataType::STRING && right_type == DataType::STRING) {
                        // Left needs conversion to string, right is GoTSString*
                        // Parameters: RDI = left const char*, RSI = right GoTSString*
                        gen.emit_call("__string_concat_cstr_left");
                    } else {
                        // Neither operand is a string - fallback to regular concatenation
                        // This should not happen in string concatenation context, but handle gracefully
                        // Convert both to strings first, then concatenate
                        // For robust implementation, we'd add runtime type conversion here
                        gen.emit_call("__string_concat");
                    }
                    // Result (new GoTSString*) is now in RAX
                }
            } else {
                result_type = types.get_cast_type(left_type, right_type);
                if (left) {
                    // Pop left operand from stack and add to right operand (in RAX)
                    gen.emit_mov_reg_mem_rsp(3, 0);   // mov rbx, [rsp] (load left operand from stack)
                    gen.emit_add_reg_imm(4, 8);   // add rsp, 8 (restore stack)
                    gen.emit_add_reg_reg(0, 3);   // add rax, rbx (add left to right)
                }
            }
            break;
            
        case TokenType::MINUS:
            result_type = types.get_cast_type(left_type, right_type);
            if (left) {
                // Binary minus: Pop left operand from stack and subtract right operand from it
                gen.emit_mov_reg_mem_rsp(3, 0);   // mov rbx, [rsp] (load left operand from stack)
                gen.emit_add_reg_imm(4, 8);   // add rsp, 8 (restore stack)
                gen.emit_sub_reg_reg(3, 0);   // sub rbx, rax (subtract right from left)
                gen.emit_mov_reg_reg(0, 3);   // mov rax, rbx (result in rax)
            } else {
                // Unary minus: negate the value in RAX
                gen.emit_mov_reg_imm(1, 0);   // mov rcx, 0
                gen.emit_sub_reg_reg(1, 0);   // sub rcx, rax (0 - rax)
                gen.emit_mov_reg_reg(0, 1);   // mov rax, rcx (result in rax)
                result_type = right_type;     // Result type is same as right operand for unary minus
            }
            break;
            
        case TokenType::MULTIPLY:
            result_type = types.get_cast_type(left_type, right_type);
            if (left) {
                // Pop left operand from stack and multiply with right operand
                gen.emit_mov_reg_mem_rsp(3, 0);   // mov rbx, [rsp] (load left operand from stack)
                gen.emit_add_reg_imm(4, 8);   // add rsp, 8 (restore stack)
                gen.emit_mul_reg_reg(3, 0);   // imul rbx, rax (multiply left with right)
                gen.emit_mov_reg_reg(0, 3);   // mov rax, rbx (result in rax)
            }
            break;
            
        case TokenType::POWER:
            result_type = DataType::INT64; // Power operation returns int64 for now
            if (left) {
                // For exponentiation: base ** exponent
                // x86-64 calling convention: RDI = first arg, RSI = second arg
                
                // Right operand (exponent) is currently in RAX
                gen.emit_mov_reg_reg(6, 0);   // mov rsi, rax (exponent -> second argument)
                
                // Pop left operand from stack (base)
                gen.emit_mov_reg_mem_rsp(7, 0);   // mov rdi, [rsp] (base -> first argument)
                gen.emit_add_reg_imm(4, 8);   // add rsp, 8 (restore stack)
                
                // Call the power function: __runtime_pow(base, exponent)
                gen.emit_call("__runtime_pow");
                // Result will be in RAX
            }
            break;
            
        case TokenType::DIVIDE:
            std::cerr << "\n========== DIVIDE OPERATION DETECTED ===========" << std::endl;
            std::cerr << "[BINARY_DEBUG] Processing DIVIDE operation" << std::endl;
            std::cerr << "[BINARY_DEBUG] Left type: " << (int)left_type << ", Right type: " << (int)right_type << std::endl;
            std::cerr << "[BINARY_DEBUG] This should NOT happen during console.log processing!" << std::endl;
            std::cerr.flush();
            
            result_type = types.get_cast_type(left_type, right_type);
            if (left) {
                std::cerr << "[BINARY_DEBUG] About to call emit_mov_reg_mem_rsp(1, 0) to load left operand" << std::endl;
                // Pop left operand from stack and divide by right operand
                gen.emit_mov_reg_mem_rsp(1, 0);   // mov rcx, [rsp] (load left operand from stack)
                std::cerr << "[BINARY_DEBUG] About to call emit_add_reg_imm(4, 8) to restore stack" << std::endl;
                gen.emit_add_reg_imm(4, 8);   // add rsp, 8 (restore stack)
                std::cerr << "[BINARY_DEBUG] About to call emit_div_reg_reg(1, 0) - THIS IS THE PROBLEM CALL!" << std::endl;
                std::cerr << "[BINARY_DEBUG] Calling with dst=1 (RCX), src=0 (RAX)" << std::endl;
                std::cerr.flush();
                gen.emit_div_reg_reg(1, 0);   // div rcx by rax (divide left by right)
                std::cerr << "[BINARY_DEBUG] About to call emit_mov_reg_reg(0, 1) to move result" << std::endl;
                gen.emit_mov_reg_reg(0, 1);   // mov rax, rcx (result in rax)
                std::cerr << "[BINARY_DEBUG] DIVIDE operation completed" << std::endl;
            }
            std::cerr << "=============================================" << std::endl;
            break;
            
        case TokenType::MODULO:
            result_type = types.get_cast_type(left_type, right_type);
            if (left) {
                // Use runtime function for modulo to ensure robustness
                // Right operand is in RAX, move to RSI (second argument)
                gen.emit_mov_reg_reg(6, 0);   // RSI = right operand (from RAX)
                
                // Pop left operand from stack directly to RDI (first argument)
                gen.emit_mov_reg_mem_rsp(7, 0);   // RDI = left operand from [rsp]
                gen.emit_add_reg_imm(4, 8);   // add rsp, 8 (restore stack)
                
                // Call __runtime_modulo(left, right)
                gen.emit_call("__runtime_modulo");
                // Result is now in RAX
            }
            break;
            
        case TokenType::EQUAL:
        case TokenType::NOT_EQUAL:
        case TokenType::STRICT_EQUAL:
        case TokenType::LESS:
        case TokenType::GREATER:
        case TokenType::LESS_EQUAL:
        case TokenType::GREATER_EQUAL:
            result_type = DataType::BOOLEAN;
            if (left) {
                // Pop left operand from stack and compare with right operand (in RAX)
                gen.emit_mov_reg_mem_rsp(1, 0);   // mov rcx, [rsp] (load left operand from stack)
                gen.emit_add_reg_imm(4, 8);   // add rsp, 8 (restore stack)
                
                // Optimized comparison logic with string-specific handling
                if (left_type == DataType::STRING && right_type == DataType::STRING) {
                    // Both operands are strings - use high-performance string comparison
                    // Left value is in RCX, right value is in RAX
                    gen.emit_mov_reg_reg(7, 1);   // mov rdi, rcx (left string -> first argument)
                    gen.emit_mov_reg_reg(6, 0);   // mov rsi, rax (right string -> second argument)
                    
                    switch (op) {
                        case TokenType::EQUAL:
                        case TokenType::STRICT_EQUAL:
                            gen.emit_call("__string_equals");
                            // Result (bool) is already in RAX
                            break;
                        case TokenType::NOT_EQUAL:
                            gen.emit_call("__string_equals");
                            // Invert the result: XOR with 1
                            gen.emit_mov_reg_imm(1, 1);
                            gen.emit_xor_reg_reg(0, 1);
                            break;
                        case TokenType::LESS:
                        case TokenType::GREATER:
                        case TokenType::LESS_EQUAL:
                        case TokenType::GREATER_EQUAL:
                            gen.emit_call("__string_compare");
                            // __string_compare returns -1, 0, or 1
                            // Convert to boolean based on comparison type
                            gen.emit_mov_reg_imm(1, 0);   // mov rcx, 0
                            gen.emit_compare(0, 1);       // compare result with 0
                            
                            switch (op) {
                                case TokenType::LESS:
                                    gen.emit_setl(0);     // Set AL to 1 if result < 0
                                    break;
                                case TokenType::GREATER:
                                    gen.emit_setg(0);     // Set AL to 1 if result > 0
                                    break;
                                case TokenType::LESS_EQUAL:
                                    gen.emit_setle(0);    // Set AL to 1 if result <= 0
                                    break;
                                case TokenType::GREATER_EQUAL:
                                    gen.emit_setge(0);    // Set AL to 1 if result >= 0
                                    break;
                            }
                            gen.emit_and_reg_imm(0, 0xFF); // Zero out upper bits
                            break;
                    }
                } else {
                    // Handle non-string or mixed type comparisons
                    switch (op) {
                        case TokenType::EQUAL:
                            // JavaScript-style equality with type coercion
                            // Call __runtime_js_equal(left_value, left_type, right_value, right_type)
                            // Arguments: RDI = left_value, RSI = left_type, RDX = right_value, RCX = right_type
                            
                            // Right value is already in RAX, move to RDX (3rd argument)
                            gen.emit_mov_reg_reg(2, 0);   // mov rdx, rax (right_value)
                            
                            // Left value is in RCX, move to RDI (1st argument)  
                            gen.emit_mov_reg_reg(7, 1);   // mov rdi, rcx (left_value)
                            
                            // Set type arguments - use the types determined from operands
                            // Left type (RSI) 
                            gen.emit_mov_reg_imm(6, static_cast<int64_t>(left_type));  // mov rsi, left_type
                            
                            // Right type (RCX)  
                            gen.emit_mov_reg_imm(1, static_cast<int64_t>(right_type)); // mov rcx, right_type
                            
                            // Call the JavaScript equality function
                            gen.emit_call("__runtime_js_equal");
                            // Result will be in RAX (1 for equal, 0 for not equal)
                            break;
                        default:
                            // For all other comparisons, do the compare first
                            gen.emit_compare(1, 0);       // compare rcx (left) with rax (right)
                            
                            switch (op) {
                                case TokenType::LESS:
                                    gen.emit_setl(0); // Set AL to 1 if RCX < RAX, 0 otherwise
                                    break;
                                case TokenType::GREATER:
                                    gen.emit_setg(0); // Set AL to 1 if RCX > RAX, 0 otherwise
                                    break;
                                case TokenType::NOT_EQUAL:
                                    gen.emit_setne(0); // Set AL to 1 if RCX != RAX, 0 otherwise
                                    break;
                                case TokenType::STRICT_EQUAL:
                                    // For strict equality, we need to check both value and type
                                    // For now, use same logic as EQUAL but this should be enhanced for type checking
                                    gen.emit_sete(0); // Set AL to 1 if RCX == RAX, 0 otherwise
                                    break;
                                case TokenType::LESS_EQUAL:
                                    gen.emit_setle(0); // Set AL to 1 if RCX <= RAX, 0 otherwise
                                    break;
                                case TokenType::GREATER_EQUAL:
                                    gen.emit_setge(0); // Set AL to 1 if RCX >= RAX, 0 otherwise
                                    break;
                                default:
                                    gen.emit_mov_reg_imm(0, 0); // Default to false
                                    break;
                            }
                            // Zero out the upper bits of RAX since SETcc only sets AL
                            gen.emit_and_reg_imm(0, 0xFF);
                            break;
                    }
                }
            }
            break;
            
        case TokenType::AND:
        case TokenType::OR:
            result_type = DataType::BOOLEAN;
            if (left) {
                // Generate unique labels for short-circuiting
                static int logic_counter = 0;
                std::string end_label = "__logic_end_" + std::to_string(logic_counter);
                std::string short_circuit_label = "__logic_short_" + std::to_string(logic_counter++);
                
                // Pop left operand from stack
                gen.emit_mov_reg_mem_rsp(1, 0);   // mov rcx, [rsp] (load left operand from stack)
                gen.emit_add_reg_imm(4, 8);   // add rsp, 8 (restore stack)
                
                if (op == TokenType::AND) {
                    // For AND: if left is false (0), short-circuit to false
                    gen.emit_mov_reg_imm(2, 0);       // mov rdx, 0
                    gen.emit_compare(1, 2);           // compare rcx with 0
                    gen.emit_jump_if_zero(short_circuit_label); // jump if left is false
                    
                    // Left is true, so result depends on right operand (already in RAX)
                    // Test if right operand is non-zero
                    gen.emit_compare(0, 2);           // compare rax with 0
                    gen.emit_setne(0);                // Set AL to 1 if RAX != 0
                    gen.emit_and_reg_imm(0, 0xFF);    // Zero out upper bits
                    gen.emit_jump(end_label);
                    
                    // Short-circuit: left was false, so result is false
                    gen.emit_label(short_circuit_label);
                    gen.emit_mov_reg_imm(0, 0);       // mov rax, 0
                } else { // OR
                    // For OR: if left is true (non-zero), short-circuit to true
                    gen.emit_mov_reg_imm(2, 0);       // mov rdx, 0
                    gen.emit_compare(1, 2);           // compare rcx with 0
                    gen.emit_jump_if_not_zero(short_circuit_label); // jump if left is true
                    
                    // Left is false, so result depends on right operand (already in RAX)
                    // Test if right operand is non-zero
                    gen.emit_compare(0, 2);           // compare rax with 0
                    gen.emit_setne(0);                // Set AL to 1 if RAX != 0
                    gen.emit_and_reg_imm(0, 0xFF);    // Zero out upper bits
                    gen.emit_jump(end_label);
                    
                    // Short-circuit: left was true, so result is true
                    gen.emit_label(short_circuit_label);
                    gen.emit_mov_reg_imm(0, 1);       // mov rax, 1
                }
                
                gen.emit_label(end_label);
            }
            break;
            
        case TokenType::NOT:
            result_type = DataType::BOOLEAN;
            // For unary NOT, right operand is in RAX, left should be null
            if (!left) {
                // Compare RAX with 0 to check if it's false (0)
                gen.emit_mov_reg_imm(1, 0);   // mov rcx, 0
                gen.emit_compare(0, 1);       // compare rax with 0
                gen.emit_sete(0);             // Set AL to 1 if RAX == 0 (i.e., NOT false = true)
                gen.emit_and_reg_imm(0, 0xFF); // Zero out upper bits
            }
            break;
            
        default:
            result_type = DataType::ANY;
            break;
    }
}

void TernaryOperator::generate_code(CodeGenerator& gen, TypeInference& types) {
    // Generate unique labels for the ternary branches
    static int label_counter = 0;
    std::string false_label = "__ternary_false_" + std::to_string(label_counter);
    std::string end_label = "__ternary_end_" + std::to_string(label_counter++);
    
    // Generate code for condition
    condition->generate_code(gen, types);
    
    // Test if condition is zero (false) - compare RAX with 0
    gen.emit_mov_reg_imm(1, 0); // mov rcx, 0
    gen.emit_compare(0, 1); // Compare RAX with RCX (0)
    gen.emit_jump_if_zero(false_label);
    
    // Generate code for true expression
    true_expr->generate_code(gen, types);
    gen.emit_jump(end_label);
    
    // False branch
    gen.emit_label(false_label);
    false_expr->generate_code(gen, types);
    
    // End label
    gen.emit_label(end_label);
    
    // Result type is the common type of true and false expressions
    result_type = types.get_cast_type(true_expr->result_type, false_expr->result_type);
}

void FunctionCall::generate_code(CodeGenerator& gen, TypeInference& types) {
    if (is_goroutine) {
        // For goroutines, we need to build an argument array on the stack
        if (arguments.size() > 0) {
            // Push arguments onto stack in reverse order to create array
            for (int i = arguments.size() - 1; i >= 0; i--) {
                arguments[i]->generate_code(gen, types);
                gen.emit_sub_reg_imm(4, 8);  // sub rsp, 8
                gen.emit_mov_mem_rsp_reg(0, 0);  // mov [rsp], rax
            }
            
            // Now stack contains arguments in correct order: arg0, arg1, arg2...
            gen.emit_goroutine_spawn_with_args(name, arguments.size());
            
            // Clean up the argument array from stack
            int64_t array_size = arguments.size() * 8;
            gen.emit_add_reg_imm(4, array_size);  // add rsp, array_size
        } else {
            gen.emit_goroutine_spawn(name);
        }
        result_type = DataType::PROMISE;
    } else {
        // Check for global timer functions and map them to runtime equivalents
        if (name == "setTimeout") {
            // Map setTimeout to runtime.timer.setTimeout
            for (size_t i = 0; i < arguments.size() && i < 6; i++) {
                arguments[i]->generate_code(gen, types);
                switch (i) {
                    case 0: gen.emit_mov_reg_reg(7, 0); break;  // RDI = RAX
                    case 1: gen.emit_mov_reg_reg(6, 0); break;  // RSI = RAX
                    case 2: gen.emit_mov_reg_reg(2, 0); break;  // RDX = RAX
                    case 3: gen.emit_mov_reg_reg(1, 0); break;  // RCX = RAX
                    case 4: gen.emit_mov_reg_reg(8, 0); break;  // R8 = RAX
                    case 5: gen.emit_mov_reg_reg(9, 0); break;  // R9 = RAX
                }
            }
            gen.emit_call("__gots_set_timeout");
            result_type = DataType::INT64; // Timer ID
            return; // Skip normal function call handling
        } else if (name == "setInterval") {
            for (size_t i = 0; i < arguments.size() && i < 6; i++) {
                arguments[i]->generate_code(gen, types);
                switch (i) {
                    case 0: gen.emit_mov_reg_reg(7, 0); break;  // RDI = RAX
                    case 1: gen.emit_mov_reg_reg(6, 0); break;  // RSI = RAX
                    case 2: gen.emit_mov_reg_reg(2, 0); break;  // RDX = RAX
                    case 3: gen.emit_mov_reg_reg(1, 0); break;  // RCX = RAX
                    case 4: gen.emit_mov_reg_reg(8, 0); break;  // R8 = RAX
                    case 5: gen.emit_mov_reg_reg(9, 0); break;  // R9 = RAX
                }
            }
            gen.emit_call("__gots_set_interval");
            result_type = DataType::INT64; // Timer ID
            return;
        } else if (name == "clearTimeout") {
            for (size_t i = 0; i < arguments.size() && i < 6; i++) {
                arguments[i]->generate_code(gen, types);
                switch (i) {
                    case 0: gen.emit_mov_reg_reg(7, 0); break;  // RDI = RAX
                    case 1: gen.emit_mov_reg_reg(6, 0); break;  // RSI = RAX
                    case 2: gen.emit_mov_reg_reg(2, 0); break;  // RDX = RAX
                    case 3: gen.emit_mov_reg_reg(1, 0); break;  // RCX = RAX
                    case 4: gen.emit_mov_reg_reg(8, 0); break;  // R8 = RAX
                    case 5: gen.emit_mov_reg_reg(9, 0); break;  // R9 = RAX
                }
            }
            gen.emit_call("__gots_clear_timeout");
            result_type = DataType::BOOLEAN; // Success/failure
            return;
        } else if (name == "clearInterval") {
            for (size_t i = 0; i < arguments.size() && i < 6; i++) {
                arguments[i]->generate_code(gen, types);
                switch (i) {
                    case 0: gen.emit_mov_reg_reg(7, 0); break;  // RDI = RAX
                    case 1: gen.emit_mov_reg_reg(6, 0); break;  // RSI = RAX
                    case 2: gen.emit_mov_reg_reg(2, 0); break;  // RDX = RAX
                    case 3: gen.emit_mov_reg_reg(1, 0); break;  // RCX = RAX
                    case 4: gen.emit_mov_reg_reg(8, 0); break;  // R8 = RAX
                    case 5: gen.emit_mov_reg_reg(9, 0); break;  // R9 = RAX
                }
            }
            gen.emit_call("__gots_clear_interval");
            result_type = DataType::BOOLEAN; // Success/failure
            return;
        } else if (name == "console.log") {
            // Type-aware console.log implementation with specialized JIT code
            // Generate specialized code for each argument based on its type
            
            std::vector<ExpressionNode*> arg_ptrs;
            for (const auto& arg : arguments) {
                arg_ptrs.push_back(arg.get());
            }
            
            // Use the new type-aware console.log system
            TypeAwareConsoleLog::generate_console_log_code(gen, types, arg_ptrs);
            
            result_type = DataType::VOID;
            return;
        }
        
        // Regular function call - use x86-64 calling convention
        
        // Check if this is a variable containing a function ID that needs to be resolved
        DataType var_type = types.get_variable_type(name);
        bool is_function_variable = (var_type == DataType::FUNCTION);
        
        // Generate code for arguments and place them in appropriate registers
        for (size_t i = 0; i < arguments.size() && i < 6; i++) {
            arguments[i]->generate_code(gen, types);
            
            // Handle reference counting for CLASS_INSTANCE arguments
            if (arguments[i]->result_type == DataType::CLASS_INSTANCE) {
                gen.emit_mov_reg_imm(1, 0); // RCX = 0
                gen.emit_compare(0, 1); // Compare RAX with 0
                std::string skip_arg_ref_inc = "skip_arg_ref_inc_" + std::to_string(i) + "_" + std::to_string(rand());
                gen.emit_jump_if_zero(skip_arg_ref_inc); // Skip if null
                
                // Increment reference count for the argument object (RAX = object)
                gen.emit_ref_count_increment(0); // RAX = object pointer
                
                gen.emit_label(skip_arg_ref_inc);
            }
            
            // Move result to appropriate argument register
            switch (i) {
                case 0: gen.emit_mov_reg_reg(7, 0); break;  // RDI = RAX
                case 1: gen.emit_mov_reg_reg(6, 0); break;  // RSI = RAX
                case 2: gen.emit_mov_reg_reg(2, 0); break;  // RDX = RAX
                case 3: gen.emit_mov_reg_reg(1, 0); break;  // RCX = RAX
                case 4: gen.emit_mov_reg_reg(8, 0); break;  // R8 = RAX
                case 5: gen.emit_mov_reg_reg(9, 0); break;  // R9 = RAX
            }
        }
        
        // For more than 6 arguments, push them onto stack (in reverse order)
        for (int i = arguments.size() - 1; i >= 6; i--) {
            arguments[i]->generate_code(gen, types);
            
            // Handle reference counting for CLASS_INSTANCE stack arguments  
            if (arguments[i]->result_type == DataType::CLASS_INSTANCE) {
                gen.emit_mov_reg_imm(1, 0); // RCX = 0
                gen.emit_compare(0, 1); // Compare RAX with 0
                std::string skip_stack_arg_ref_inc = "skip_stack_arg_ref_inc_" + std::to_string(i) + "_" + std::to_string(rand());
                gen.emit_jump_if_zero(skip_stack_arg_ref_inc); // Skip if null                // Increment reference count for the stack argument object (RAX = object)
                gen.emit_ref_count_increment(0); // RAX = object pointer
                
                gen.emit_label(skip_stack_arg_ref_inc);
            }
            
            // Push RAX onto stack
            gen.emit_sub_reg_imm(4, 8);  // sub rsp, 8
            gen.emit_mov_mem_reg(0, 0);  // mov [rsp], rax
        }
        
        if (is_function_variable) {
            // This is a variable containing a function ID - resolve it to function address
            
            // Ensure our lookup function is registered
            ensure_lookup_function_by_id_registered();
            
            // Load the function ID from the variable
            int64_t var_offset = types.get_variable_offset(name);
            if (var_offset == 0) var_offset = -8; // Default offset
            gen.emit_mov_reg_mem(0, var_offset);  // RAX = function_id
            
            // Call runtime function to resolve function ID to address
            gen.emit_mov_reg_reg(7, 0);  // RDI = function_id (first arg)
            gen.emit_call("__lookup_function_by_id");
            
            // RAX now contains the function address, call it
            gen.emit_call_reg(0);  // call rax
        } else {
            // Direct function call by name
            gen.emit_call(name);
        }
        
        // Look up function return type from compiler registry
        auto* compiler = get_current_compiler();
        if (compiler) {
            Function* func = compiler->get_function(name);
            if (func) {
                result_type = func->return_type;
                //           << static_cast<int>(result_type) << std::endl;
                
                // Handle reference counting for CLASS_INSTANCE return values
                if (result_type == DataType::CLASS_INSTANCE) {
                    gen.emit_mov_reg_imm(1, 0); // RCX = 0
                    gen.emit_compare(0, 1); // Compare RAX with 0
                    std::string skip_return_ref_inc = "skip_return_ref_inc_" + std::to_string(rand());
                    gen.emit_jump_if_zero(skip_return_ref_inc); // Skip if null
                    
                    // Increment reference count for the returned object (RAX = object)
                    gen.emit_ref_count_increment(0); // RAX = object pointer
                    
                    gen.emit_label(skip_return_ref_inc);
                }
            } else {
                // Function not found in registry, assume FLOAT64 for built-in functions
                result_type = DataType::FLOAT64;
            }
        } else {
            // No compiler context, fall back to default
            result_type = DataType::FLOAT64;
        }
        
        // Clean up stack if we pushed arguments
        if (arguments.size() > 6) {
            int stack_cleanup = (arguments.size() - 6) * 8;
            gen.emit_add_reg_imm(4, stack_cleanup);  // add rsp, cleanup_amount
        }
    }
    
    if (is_awaited) {
        gen.emit_promise_await(0);
    }
}

void MethodCall::generate_code(CodeGenerator& gen, TypeInference& types) {
    
    // Handle built-in methods
    if (object_name == "console") {
        if (method_name == "log") {
            // Type-aware console.log implementation with specialized JIT code
            // Generate specialized code for each argument based on its type
            
            std::vector<ExpressionNode*> arg_ptrs;
            for (const auto& arg : arguments) {
                arg_ptrs.push_back(arg.get());
            }
            
            // Use the new type-aware console.log system
            TypeAwareConsoleLog::generate_console_log_code(gen, types, arg_ptrs);
            result_type = DataType::VOID;
        } else if (method_name == "time") {
            // Call console.time built-in function
            if (arguments.size() > 0) {
                arguments[0]->generate_code(gen, types);
                // Move first argument to RDI register (x86-64 calling convention)
                gen.emit_mov_reg_reg(7, 0); // RDI = RAX
            }
            // Ensure stack is aligned for C calling convention
            gen.emit_sub_reg_imm(4, 8);  // Align stack to 16-byte boundary
            gen.emit_call("__console_time");
            gen.emit_add_reg_imm(4, 8);  // Restore stack
            result_type = DataType::VOID;
        } else if (method_name == "timeEnd") {
            // Call console.timeEnd built-in function
            if (arguments.size() > 0) {
                arguments[0]->generate_code(gen, types);
                // Move first argument to RDI register (x86-64 calling convention)
                gen.emit_mov_reg_reg(7, 0); // RDI = RAX
            }
            // Ensure stack is aligned for C calling convention
            gen.emit_sub_reg_imm(4, 8);  // Align stack to 16-byte boundary
            gen.emit_call("__console_timeEnd");
            gen.emit_add_reg_imm(4, 8);  // Restore stack
            result_type = DataType::VOID;
        } else {
            throw std::runtime_error("Unknown console method: " + method_name);
        }
    } else if (object_name == "Promise") {
        if (method_name == "all") {
            // Promise.all expects an array as its first argument
            if (arguments.size() > 0) {
                arguments[0]->generate_code(gen, types);
                // Move the array pointer to RDI (first argument register)
                gen.emit_mov_reg_reg(7, 0); // RDI = RAX
            } else {
                // No arguments, pass nullptr
                gen.emit_mov_reg_imm(7, 0); // RDI = 0 (nullptr)
            }
            gen.emit_call("__promise_all");
            result_type = DataType::PROMISE;
        } else {
            throw std::runtime_error("Unknown Promise method: " + method_name);
        }
    } else {
        // Handle variable method calls (like array.push())
        DataType object_type = types.get_variable_type(object_name);
        
        // Debug output to see what type we get
        // std::cout << "' has type " << static_cast<int>(object_type) << std::endl;
        
        if (object_type == DataType::TENSOR) {
            // Handle array/tensor methods
            if (method_name == "push") {
                // Get the proper offset for the array variable
                int64_t array_offset = types.get_variable_offset(object_name);
                gen.emit_mov_reg_mem(2, array_offset); // Load array pointer from proper offset
                
                // Call array push for each argument
                for (size_t i = 0; i < arguments.size(); i++) {
                    // Save array pointer to a temporary stack location before evaluating argument
                    // Use a safe offset that doesn't conflict with variables
                    gen.emit_mov_mem_reg(-32, 2); // Save array pointer to stack
                    
                    // Generate code for the argument (this may be a goroutine call)
                    arguments[i]->generate_code(gen, types);
                    
                    // Restore array pointer and set up call parameters
                    gen.emit_mov_reg_mem(7, -32); // RDI = array pointer from stack
                    gen.emit_mov_reg_reg(6, 0); // RSI = value to push
                    gen.emit_call("__array_push");
                }
                result_type = DataType::VOID;
            } else {
                throw std::runtime_error("Unknown array method: " + method_name);
            }
        } else if (object_type == DataType::ARRAY) {
            // Handle simplified Array methods
            if (method_name == "push") {
                std::cout << "[DEBUG] AST: Generating code for array.push() method" << std::endl;
                std::cout.flush();
                // Get the array variable offset
                int64_t array_offset = types.get_variable_offset(object_name);
                std::cout << "[DEBUG] AST: Array variable offset: " << array_offset << std::endl;
                std::cout.flush();
                
                // For each argument, call push
                for (size_t i = 0; i < arguments.size(); i++) {
                    std::cout << "[DEBUG] AST: Processing push argument " << i << std::endl;
                    std::cout.flush();
                    // Load array pointer
                    gen.emit_mov_reg_mem(7, array_offset); // RDI = array pointer
                    
                    // Generate argument value (this puts int64 bit pattern in RAX)
                    arguments[i]->generate_code(gen, types);
                    
                    // Move argument to RSI (second parameter) - RAX contains int64 bit pattern
                    gen.emit_mov_reg_reg(6, 0); // RSI = int64 bit pattern
                    
                    std::cout << "[DEBUG] AST: Calling __array_push for argument " << i << std::endl;
                    std::cout.flush();
                    // Call int64-based array push (consistent with ArrayLiteral)
                    gen.emit_call("__array_push");
                }
                result_type = DataType::VOID;
            } else if (method_name == "pop") {
                // Get the array variable offset
                int64_t array_offset = types.get_variable_offset(object_name);
                gen.emit_mov_reg_mem(7, array_offset); // RDI = array pointer
                gen.emit_call("__array_pop");
                result_type = DataType::FLOAT64;
            } else if (method_name == "slice") {
                // Get the array variable offset
                int64_t array_offset = types.get_variable_offset(object_name);
                gen.emit_mov_reg_mem(7, array_offset); // RDI = array pointer
                
                // Handle arguments: start, end, step (with defaults)
                if (arguments.size() >= 1) {
                    arguments[0]->generate_code(gen, types);
                    gen.emit_mov_reg_reg(6, 0); // RSI = start
                } else {
                    gen.emit_mov_reg_imm(6, 0); // RSI = 0 (default start)
                }
                
                if (arguments.size() >= 2) {
                    arguments[1]->generate_code(gen, types);
                    gen.emit_mov_reg_reg(2, 0); // RDX = end
                } else {
                    gen.emit_mov_reg_imm(2, -1); // RDX = -1 (default end)
                }
                
                if (arguments.size() >= 3) {
                    arguments[2]->generate_code(gen, types);
                    gen.emit_mov_reg_reg(1, 0); // RCX = step
                } else {
                    gen.emit_mov_reg_imm(1, 1); // RCX = 1 (default step)
                }
                
                // TODO: Implement array slice method for new DynamicArray system
                throw std::runtime_error("Array.slice() method not yet implemented for new array system");
            } else if (method_name == "slice_all") {
                // TODO: Implement array slice_all method for new DynamicArray system
                throw std::runtime_error("Array.slice_all() method not yet implemented for new array system");
            } else if (method_name == "toString") {
                // TODO: Implement array toString method for new DynamicArray system
                throw std::runtime_error("Array.toString() method not yet implemented for new array system");
            } else if (method_name == "sum") {
                // TODO: Implement array sum method for new DynamicArray system
                throw std::runtime_error("Array.sum() method not yet implemented for new array system");
            } else if (method_name == "mean") {
                // TODO: Implement array mean method for new DynamicArray system
                throw std::runtime_error("Array.mean() method not yet implemented for new array system");
            } else if (method_name == "max") {
                // TODO: Implement array max method for new DynamicArray system
                throw std::runtime_error("Array.max() method not yet implemented for new array system");
            } else if (method_name == "min") {
                // TODO: Implement array min method for new DynamicArray system
                throw std::runtime_error("Array.min() method not yet implemented for new array system");
            } else {
                throw std::runtime_error("Unknown Array method: " + method_name);
            }
        } else if (object_type == DataType::REGEX) {
            // Handle regex methods like test, exec
            if (method_name == "test") {
                // Get the regex variable offset
                int64_t regex_offset = types.get_variable_offset(object_name);
                
                // Load regex pointer from stack
                gen.emit_mov_reg_mem(0, regex_offset); // RAX = [RBP + offset]
                gen.emit_mov_reg_reg(12, 0); // R12 = RAX (save in callee-saved register)
                
                
                if (arguments.size() > 0) {
                    arguments[0]->generate_code(gen, types);
                    gen.emit_mov_reg_reg(6, 0); // RSI = string pointer (RAX has the string)
                    gen.emit_mov_reg_reg(7, 12); // RDI = R12 (restore regex pointer)
                    
                    gen.emit_call("__regex_test");
                    result_type = DataType::BOOLEAN;
                } else {
                    throw std::runtime_error("RegExp.test() requires a string argument");
                }
            } else if (method_name == "exec") {
                // SAFE APPROACH: Use callee-saved register to preserve regex pointer
                int64_t regex_offset = types.get_variable_offset(object_name);
                
                // Load regex pointer and save in callee-saved register
                gen.emit_mov_reg_mem(0, regex_offset); // RAX = [RBP + offset]
                gen.emit_mov_reg_reg(12, 0); // R12 = RAX (save in callee-saved register)
                
                if (arguments.size() > 0) {
                    // Generate string argument (this may call __string_intern)
                    arguments[0]->generate_code(gen, types);
                    
                    // Set up function call parameters from preserved registers
                    gen.emit_mov_reg_reg(6, 0);  // RSI = string pointer from RAX
                    gen.emit_mov_reg_reg(7, 12); // RDI = regex pointer from R12
                    
                    gen.emit_call("__regex_exec");
                    result_type = DataType::TENSOR; // Match object/array
                } else {
                    throw std::runtime_error("RegExp.exec() requires a string argument");
                }
            } else {
                throw std::runtime_error("Unknown regex method: " + method_name);
            }
        } else if (object_type == DataType::STRING) {
            // Handle string methods like match, replace, search, split
            // std::cout << "' method '" << method_name << "'" << std::endl;
            if (method_name == "match") {
                // Get the string variable offset and load the string pointer
                int64_t string_offset = types.get_variable_offset(object_name);
                gen.emit_mov_reg_mem(0, string_offset); // Load string pointer
                gen.emit_mov_mem_reg(-8, 0); // Save string at RBP-8
                
                if (arguments.size() > 0) {
                    // Generate code for regex argument
                    arguments[0]->generate_code(gen, types);
                    
                    // Set up call: string_match(string_ptr, regex_ptr)
                    gen.emit_mov_reg_mem(7, -8);  // RDI = string pointer
                    gen.emit_mov_reg_reg(6, 0);   // RSI = regex pointer
                    gen.emit_call("__string_match");
                    result_type = DataType::TENSOR; // Array of matches
                } else {
                    throw std::runtime_error("String.match() requires a regex argument");
                }
            } else {
                throw std::runtime_error("Unknown string method: " + method_name);
            }
        } else if (object_type == DataType::ANY) {
            // If object_name is not a variable, it might be a static method call
            // Generate static method call: ClassName.methodName()
            
            // Special handling for Array static methods
            if (object_name == "Array") {
                if (method_name == "zeros") {
                    // Handle Array.zeros([shape], dtype) - supports both 1 and 2 arguments
                    std::cout << "[DEBUG] AST: Generating code for Array.zeros with " << arguments.size() << " arguments" << std::endl;
                    std::cout.flush();
                    
                    if (arguments.size() >= 1) {
                        std::cout << "[DEBUG] AST: Generating shape array argument" << std::endl;
                        std::cout.flush();
                        // Generate the shape array argument
                        arguments[0]->generate_code(gen, types);
                        // RAX now contains the shape array, extract first dimension
                        gen.emit_mov_reg_reg(7, 0); // RDI = shape array
                        std::cout << "[DEBUG] AST: Calling __array_size to get first dimension" << std::endl;
                        std::cout.flush();
                        gen.emit_call("__array_size"); // Get array size (first dimension)
                        gen.emit_mov_mem_reg(-40, 0); // Save size (RAX) to stack
                        
                        // Check if dtype argument is provided
                        if (arguments.size() >= 2) {
                            std::cout << "[DEBUG] AST: Generating dtype argument for Array.zeros" << std::endl;
                            std::cout.flush();
                            
                            // Check if the dtype is a string literal for compile-time optimization
                            StringLiteral* dtype_literal = dynamic_cast<StringLiteral*>(arguments[1].get());
                            if (dtype_literal) {
                                // ULTRA-FAST PATH: Compile-time dtype detection, emit direct typed calls
                                std::string dtype = dtype_literal->value;
                                std::cout << "[DEBUG] AST: Compile-time dtype detected: " << dtype << std::endl;
                                std::cout.flush();
                                
                                // Restore size to RDI
                                gen.emit_mov_reg_mem(7, -40); // RDI = size
                                
                                // Emit direct call to specific typed array creation function
                                if (dtype == "int64") {
                                    gen.emit_call("__array_create_int64");
                                    std::cout << "[DEBUG] AST: Direct call to __array_create_int64" << std::endl;
                                } else if (dtype == "float64") {
                                    gen.emit_call("__array_create_float64");
                                    std::cout << "[DEBUG] AST: Direct call to __array_create_float64" << std::endl;
                                } else if (dtype == "int32") {
                                    gen.emit_call("__array_create_int32");
                                    std::cout << "[DEBUG] AST: Direct call to __array_create_int32" << std::endl;
                                } else if (dtype == "float32") {
                                    gen.emit_call("__array_create_float32");
                                    std::cout << "[DEBUG] AST: Direct call to __array_create_float32" << std::endl;
                                } else {
                                    // Unknown dtype, fallback to dynamic
                                    gen.emit_call("__array_create_dynamic");
                                    std::cout << "[DEBUG] AST: Unknown dtype, fallback to dynamic array" << std::endl;
                                }
                            } else {
                                // SLOW PATH: Runtime dtype resolution (only for dynamic dtypes)
                                std::cout << "[DEBUG] AST: Runtime dtype resolution required" << std::endl;
                                std::cout.flush();
                                // Generate the dtype argument (string)
                                arguments[1]->generate_code(gen, types);
                                gen.emit_mov_reg_reg(6, 0); // RSI = dtype string
                                
                                // Restore size to RDI
                                gen.emit_mov_reg_mem(7, -40); // RDI = size
                                // Call runtime dtype resolver (slow path)
                                gen.emit_call("__array_zeros_typed");
                            }
                        } else {
                            std::cout << "[DEBUG] AST: No dtype, calling __array_create_dynamic" << std::endl;
                            std::cout.flush();
                            // No dtype, create dynamic array with zeros
                            gen.emit_mov_reg_mem(7, -40); // RDI = size
                            gen.emit_call("__array_create_dynamic");
                        }
                    } else {
                        std::cout << "[DEBUG] AST: No arguments, creating empty dynamic array" << std::endl;
                        std::cout.flush();
                        gen.emit_mov_reg_imm(7, 0); // RDI = 0 (empty array)
                        gen.emit_call("__array_create_dynamic");
                    }
                    result_type = DataType::ARRAY;
                    std::cout << "[DEBUG] AST: Array.zeros code generation complete" << std::endl;
                    std::cout.flush();
                    return;
                } else if (method_name == "ones") {
                    // Handle Array.ones([shape], dtype) - supports both 1 and 2 arguments
                    if (arguments.size() >= 1) {
                        // Generate the shape array argument
                        arguments[0]->generate_code(gen, types);
                        // RAX now contains the shape array, extract first dimension
                        gen.emit_mov_reg_reg(7, 0); // RDI = shape array
                        gen.emit_call("__array_size"); // Get array size (first dimension)
                        gen.emit_mov_mem_reg(-40, 0); // Save size (RAX) to stack
                        
                        // Check if dtype argument is provided
                        if (arguments.size() >= 2) {
                            std::cout << "[DEBUG] AST: Generating dtype argument for Array.ones" << std::endl;
                            std::cout.flush();
                            
                            // Check if the dtype is a string literal for compile-time optimization
                            StringLiteral* dtype_literal = dynamic_cast<StringLiteral*>(arguments[1].get());
                            if (dtype_literal) {
                                // ULTRA-FAST PATH: Compile-time dtype detection, emit direct typed calls
                                std::string dtype = dtype_literal->value;
                                std::cout << "[DEBUG] AST: Compile-time dtype detected for ones: " << dtype << std::endl;
                                std::cout.flush();
                                
                                // Restore size to RDI
                                gen.emit_mov_reg_mem(7, -40); // RDI = size
                                
                                // Emit direct call to specific typed array ones function
                                if (dtype == "int64") {
                                    gen.emit_call("__array_ones_int64");
                                    std::cout << "[DEBUG] AST: Direct call to __array_ones_int64" << std::endl;
                                } else if (dtype == "float64") {
                                    gen.emit_call("__array_ones_float64");
                                    std::cout << "[DEBUG] AST: Direct call to __array_ones_float64" << std::endl;
                                } else if (dtype == "int32") {
                                    gen.emit_call("__array_ones_int32");
                                    std::cout << "[DEBUG] AST: Direct call to __array_ones_int32" << std::endl;
                                } else if (dtype == "float32") {
                                    gen.emit_call("__array_ones_float32");
                                    std::cout << "[DEBUG] AST: Direct call to __array_ones_float32" << std::endl;
                                } else {
                                    // Unknown dtype, fallback to dynamic
                                    gen.emit_call("__array_ones_dynamic");
                                    std::cout << "[DEBUG] AST: Unknown dtype for ones, fallback to dynamic array" << std::endl;
                                }
                            } else {
                                // SLOW PATH: Runtime dtype resolution - not implemented for ones yet
                                std::cout << "[DEBUG] AST: Runtime dtype resolution for ones not implemented, using dynamic" << std::endl;
                                std::cout.flush();
                                // Restore size to RDI
                                gen.emit_mov_reg_mem(7, -40); // RDI = size
                                gen.emit_call("__array_ones_dynamic");
                            }
                        } else {
                            std::cout << "[DEBUG] AST: No dtype for ones, calling __array_ones_dynamic" << std::endl;
                            std::cout.flush();
                            // No dtype, create dynamic array with ones
                            gen.emit_mov_reg_mem(7, -40); // RDI = size
                            gen.emit_call("__array_ones_dynamic");
                        }
                    } else {
                        gen.emit_mov_reg_imm(7, 0); // RDI = 0 (empty array)
                        gen.emit_call("__array_ones_dynamic");
                    }
                    result_type = DataType::ARRAY;
                    return;
                } else if (method_name == "arange") {
                    // Handle Array.arange(start, stop, step)
                    if (arguments.size() >= 2) {
                        arguments[0]->generate_code(gen, types);
                        gen.emit_mov_mem_reg(-8, 0); // Save start
                        arguments[1]->generate_code(gen, types);
                        gen.emit_mov_mem_reg(-16, 0); // Save stop
                        
                        gen.emit_mov_reg_mem(7, -8); // RDI = start
                        gen.emit_mov_reg_mem(6, -16); // RSI = stop
                        
                        if (arguments.size() >= 3) {
                            arguments[2]->generate_code(gen, types);
                            gen.emit_mov_reg_reg(2, 0); // RDX = step
                        } else {
                            gen.emit_mov_reg_imm(2, 1); // RDX = 1 (default step)
                        }
                        
                        // TODO: Implement Array.arange for new DynamicArray system
                        throw std::runtime_error("Array.arange() not yet implemented for new array system");
                        result_type = DataType::ARRAY;
                        return;
                    }
                } else if (method_name == "linspace") {
                    // Handle Array.linspace(start, stop, num)
                    if (arguments.size() >= 2) {
                        arguments[0]->generate_code(gen, types);
                        gen.emit_mov_mem_reg(-8, 0); // Save start
                        arguments[1]->generate_code(gen, types);
                        gen.emit_mov_mem_reg(-16, 0); // Save stop
                        
                        gen.emit_mov_reg_mem(7, -8); // RDI = start
                        gen.emit_mov_reg_mem(6, -16); // RSI = stop
                        
                        if (arguments.size() >= 3) {
                            arguments[2]->generate_code(gen, types);
                            gen.emit_mov_reg_reg(2, 0); // RDX = num
                        } else {
                            gen.emit_mov_reg_imm(2, 50); // RDX = 50 (default)
                        }
                        
                        // TODO: Implement Array.linspace for new DynamicArray system
                        throw std::runtime_error("Array.linspace() not yet implemented for new array system");
                        result_type = DataType::ARRAY;
                        return;
                    }
                }
            }
            
            std::string static_method_label = "__static_" + method_name;
            
            // Set up arguments for static method call (no 'this' parameter)
            for (size_t i = 0; i < arguments.size() && i < 6; i++) {
                arguments[i]->generate_code(gen, types);
                
                // Store argument in temporary stack location
                gen.emit_mov_mem_reg(-(int64_t)(i + 1) * 8, 0);
            }
            
            // Load arguments into registers
            for (size_t i = 0; i < arguments.size() && i < 6; i++) {
                switch (i) {
                    case 0: gen.emit_mov_reg_mem(7, -8); break;   // RDI
                    case 1: gen.emit_mov_reg_mem(6, -16); break;  // RSI
                    case 2: gen.emit_mov_reg_mem(2, -24); break;  // RDX
                    case 3: gen.emit_mov_reg_mem(1, -32); break;  // RCX
                    case 4: gen.emit_mov_reg_mem(8, -40); break;  // R8
                    case 5: gen.emit_mov_reg_mem(9, -48); break;  // R9
                }
            }
            
            // Call the static method
            gen.emit_call(static_method_label);
            
            result_type = DataType::ANY; // TODO: Get actual return type from method signature
            
            // std::cout << object_name << "." << method_name << " at label " << static_method_label << std::endl;
        } else {
            // Check if this is a class instance method call
            DataType object_type = types.get_variable_type(object_name);
            uint32_t class_type_id = types.get_variable_class_type_id(object_name);
            
            std::string class_name;
            if (class_type_id != 0) {
                auto* compiler = get_current_compiler();
                if (compiler) {
                    class_name = compiler->get_class_name_from_type_id(class_type_id);
                }
            }
            
            if (object_type == DataType::CLASS_INSTANCE && !class_name.empty()) {
                // Get object ID from variable
                int64_t object_offset = types.get_variable_offset(object_name);
                gen.emit_mov_reg_mem(0, object_offset); // RAX = object_address
                
                // Call method via object system
                // For now, just call __object_call_method with basic setup
                gen.emit_mov_reg_reg(7, 0); // RDI = object_address
                
                // Call the generated method function directly
                // PERFORMANCE OPTIMIZATION: Use parent method for single inheritance
                GoTSCompiler* compiler = get_current_compiler();
                std::string method_label;
                
                if (compiler) {
                    ClassInfo* class_info = compiler->get_class(class_name);
                    if (class_info) {
                        // Check if this class needs specialized methods for multiple inheritance
                        ClassDecl temp_class_decl(class_name);
                        temp_class_decl.parent_classes = class_info->parent_classes;
                        
                        if (compiler->needs_specialized_methods(temp_class_decl)) {
                            // Multiple inheritance - use specialized method
                            method_label = "__method_" + class_name + "_" + method_name;
                            std::cout << "[CALL] Using specialized method: " << method_label << std::endl;
                        } else {
                            // Single inheritance - check if method exists in parent and use parent method directly
                            bool found_in_parent = false;
                            for (const std::string& parent_name : class_info->parent_classes) {
                                ClassInfo* parent_info = compiler->get_class(parent_name);
                                if (parent_info && parent_info->methods.find(method_name) != parent_info->methods.end()) {
                                    method_label = "__method_" + method_name; // Use parent method directly
                                    found_in_parent = true;
                                    std::cout << "[CALL] Using parent method for single inheritance: " << method_label << std::endl;
                                    break;
                                }
                            }
                            if (!found_in_parent) {
                                // Method defined in this class - use class-specific method
                                method_label = "__method_" + method_name; // Use simple method name for own methods too
                                std::cout << "[CALL] Using own method for single inheritance: " << method_label << std::endl;
                            }
                        }
                    } else {
                        // Fallback - use class-specific method
                        method_label = "__method_" + class_name + "_" + method_name;
                    }
                } else {
                    // Fallback - use class-specific method  
                    method_label = "__method_" + class_name + "_" + method_name;
                }
                
                gen.emit_call(method_label);
                
                result_type = DataType::ANY; // TODO: Get actual return type from method signature
                
                // std::cout << class_name << "::" << object_name << "." << method_name << std::endl;
            } else {
                // Unknown object type
                gen.emit_mov_reg_imm(0, 0);
                // std::cout << object_name << "." << method_name << std::endl;
                result_type = DataType::ANY;
            }
        }
    }
    
    if (is_awaited) {
        gen.emit_promise_await(0);
    }
}

void FunctionExpression::generate_code(CodeGenerator& gen, TypeInference& types) {
    // NEW THREE-PHASE SYSTEM: Function should already be compiled in Phase 2
    // During Phase 3, we just generate the appropriate code to reference the function
    
    
    // The function should already be registered in the FunctionCompilationManager
    // We need to find its name and address
    std::string func_name = compilation_assigned_name_;
    if (func_name.empty()) {
        std::cerr << "ERROR: Function expression at " << this << " has no assigned name during Phase 3!" << std::endl;
        std::cerr << "ERROR: Current compilation_assigned_name_: '" << compilation_assigned_name_ << "'" << std::endl;
        throw std::runtime_error("Function not properly registered in compilation manager");
    }
    
    // ULTRA-FAST SYSTEM: Try direct address first, then relative offset, fallback to function ID
    void* func_address = FunctionCompilationManager::instance().get_function_address(func_name);
    
    if (func_address) {
        // OPTIMAL PATH: Direct address call - zero overhead
        
        if (is_goroutine) {
            // ULTRA-OPTIMIZED: Direct goroutine spawn with address
            gen.emit_goroutine_spawn_direct(func_address);
            result_type = DataType::PROMISE;
        } else {
            // ULTRA-OPTIMIZED: Direct function address return (no lookup needed)
            gen.emit_mov_reg_imm(0, reinterpret_cast<int64_t>(func_address)); // RAX = function address
            result_type = DataType::FUNCTION;
        }
    } else {
        // SECOND BEST PATH: Try relative offset calculation (address = base + offset)
        size_t func_offset = FunctionCompilationManager::instance().get_function_offset(func_name);
        
        if (FunctionCompilationManager::instance().is_function_compiled(func_name)) {
            // NEAR-OPTIMAL PATH: Calculate address at runtime (base + offset)
            
            if (is_goroutine) {
                // Calculate function address as exec_memory_base + offset
                // Get executable memory base and add offset
                gen.emit_call("__get_executable_memory_base");  // Result in RAX
                gen.emit_add_reg_imm(0, func_offset);  // Add offset to RAX
                gen.emit_mov_reg_reg(7, 0);  // Move address to RDI
                gen.emit_call("__goroutine_spawn_func_ptr");  // Spawn with function pointer
                result_type = DataType::PROMISE;
            } else {
                // Calculate function address as exec_memory_base + offset  
                gen.emit_call("__get_executable_memory_base");  // Result in RAX
                gen.emit_add_reg_imm(0, func_offset);  // Add offset to get function address
                result_type = DataType::FUNCTION;
            }
        } else {
            // FALLBACK PATH: Use function ID (should rarely happen with proper phase ordering)
            uint16_t func_id = FunctionCompilationManager::instance().get_function_id(func_name);
            if (func_id == 0) {
                std::cerr << "ERROR: Function " << func_name << " not found in either address or ID registry!" << std::endl;
                throw std::runtime_error("Function not found in fast function registry");
            }
        
        
        if (is_goroutine) {
            // Fallback: Use fast spawn with function ID
            gen.emit_goroutine_spawn_fast(func_id);
            result_type = DataType::PROMISE;
        } else {
            // Fallback: Use fast lookup with function ID
            
            // Load function ID into RDI and call fast lookup
            gen.emit_mov_reg_imm(7, static_cast<int64_t>(func_id)); // RDI = function ID
            gen.emit_call("__lookup_function_fast"); // Fast O(1) lookup
            result_type = DataType::FUNCTION;
        }
        }
    }
}


void FunctionExpression::compile_function_body(CodeGenerator& gen, TypeInference& types, const std::string& func_name) {
    // Safety check for corrupted function name
    if (func_name.empty() || func_name.size() > 1000) {
        std::cerr << "ERROR: Invalid function name detected, skipping compilation" << std::endl;
        return;
    }
    
    // Save current stack offset state
    TypeInference local_types;
    local_types.reset_for_function();
    
    // Emit function label
    gen.emit_label(func_name);
    
    // Calculate estimated stack size for the function
    int64_t estimated_stack_size = (parameters.size() * 8) + (body.size() * 16) + 64;
    if (estimated_stack_size < 80) estimated_stack_size = 80;
    if (estimated_stack_size % 16 != 0) {
        estimated_stack_size += 16 - (estimated_stack_size % 16);
    }
    
    // Set stack size for this function
    gen.set_function_stack_size(estimated_stack_size);
    
    gen.emit_prologue();
    
    // Set up parameter types and save parameters from registers to stack
    for (size_t i = 0; i < parameters.size() && i < 6; i++) {
        const auto& param = parameters[i];
        local_types.set_variable_type(param.name, param.type);
        
        int stack_offset = -(int)(i + 1) * 8;
        local_types.set_variable_offset(param.name, stack_offset);
        
        switch (i) {
            case 0: gen.emit_mov_mem_reg(stack_offset, 7); break;  // save RDI
            case 1: gen.emit_mov_mem_reg(stack_offset, 6); break;  // save RSI
            case 2: gen.emit_mov_mem_reg(stack_offset, 2); break;  // save RDX
            case 3: gen.emit_mov_mem_reg(stack_offset, 1); break;  // save RCX
            case 4: gen.emit_mov_mem_reg(stack_offset, 8); break;  // save R8
            case 5: gen.emit_mov_mem_reg(stack_offset, 9); break;  // save R9
        }
    }
    
    // Generate function body
    bool has_explicit_return = false;
    for (size_t i = 0; i < body.size(); i++) {
        const auto& stmt = body[i];
        
        // Safety check for null pointers
        if (!stmt) {
            std::cout << "ERROR: Statement " << i << " is null!" << std::endl;
            continue;
        }
        
        try {
            stmt->generate_code(gen, local_types);
        } catch (const std::exception& e) {
            std::cout << "ERROR: Statement " << i << " threw exception: " << e.what() << std::endl;
            throw;
        } catch (...) {
            std::cout << "ERROR: Statement " << i << " threw unknown exception" << std::endl;
            throw;
        }
        
        if (dynamic_cast<const ReturnStatement*>(stmt.get())) {
            has_explicit_return = true;
        }
    }
    
    // If no explicit return, add implicit return 0
    if (!has_explicit_return) {
        gen.emit_mov_reg_imm(0, 0);  // mov rax, 0
        gen.emit_function_return();
    }
    
    // Safety check before final debug output
    if (!func_name.empty() && func_name.size() <= 1000) {
    }
}

// Shared registry for function ID to name mapping - used by both registration and lookup
static std::unordered_map<int64_t, std::string>& get_function_id_registry() {
    static std::unordered_map<int64_t, std::string> shared_function_registry;
    return shared_function_registry;
}

// Global function to look up function names by ID for runtime callbacks
extern "C" const char* __lookup_function_name_by_id(int64_t function_id) {
    auto& registry = get_function_id_registry();
    auto it = registry.find(function_id);
    if (it != registry.end()) {
        return it->second.c_str();
    }
    return nullptr;
}

// Global function to look up function address by ID for JIT calls
extern "C" void* __lookup_function_by_id(int64_t function_id) {
    // Direct fast lookup using function ID (assumes function_id is valid uint16_t)
    if (function_id >= 0 && function_id <= 65535) {
        return __lookup_function_fast(static_cast<uint16_t>(function_id));
    }
    std::cout << "ERROR: Function ID " << function_id << " out of range!" << std::endl;
    return nullptr;
}

// Function to register a function ID with its name (called from generate_code)
void __register_function_id(int64_t function_id, const std::string& function_name) {
    auto& registry = get_function_id_registry();
    registry[function_id] = function_name;
}

// Register our function in the runtime on first use
static bool __lookup_function_by_id_registered = false;
void ensure_lookup_function_by_id_registered() {
    if (!__lookup_function_by_id_registered) {
        __register_function_fast(reinterpret_cast<void*>(__lookup_function_by_id), 1, 0);
        __lookup_function_by_id_registered = true;
    }
}

void ExpressionMethodCall::generate_code(CodeGenerator& gen, TypeInference& types) {
    
    // OPTIMIZATION: Check if this is runtime.x.y() pattern (like runtime.time.now())
    ExpressionPropertyAccess* expr_prop = dynamic_cast<ExpressionPropertyAccess*>(object.get());
    if (expr_prop) {
        
        // Check if the inner object is an Identifier with name "runtime"
        Identifier* runtime_ident = dynamic_cast<Identifier*>(expr_prop->object.get());
        if (runtime_ident && runtime_ident->name == "runtime") {
            // This is runtime.x.y() - generate direct function call
            std::string sub_object = expr_prop->property_name;  // e.g., "time"
            std::string function_name = "__runtime_" + sub_object + "_" + method_name;
            
            // Map common patterns to actual function names
            if (sub_object == "time" && method_name == "now") {
                function_name = "__runtime_time_now_millis";
            } else if (sub_object == "time" && method_name == "nowNanos") {
                function_name = "__runtime_time_now_nanos";
            } else if (sub_object == "process" && method_name == "pid") {
                function_name = "__runtime_process_pid";
            } else if (sub_object == "process" && method_name == "cwd") {
                function_name = "__runtime_process_cwd";
            } else if (sub_object == "timer" && method_name == "setTimeout") {
                function_name = "__gots_set_timeout";
            } else if (sub_object == "timer" && method_name == "clearTimeout") {
                function_name = "__gots_clear_timeout";
            } else if (sub_object == "timer" && method_name == "setInterval") {
                function_name = "__gots_set_interval";
            } else if (sub_object == "timer" && method_name == "clearInterval") {
                function_name = "__gots_clear_interval";
            } else if (sub_object == "referenceCounter" && method_name == "getRefCount") {
                function_name = "__runtime_get_ref_count";
            }
            // Add more mappings as needed
            
            // std::cout << " -> " << function_name << std::endl;
            
            // Generate argument code using proper x86-64 calling convention
            for (size_t i = 0; i < arguments.size() && i < 6; i++) {
                arguments[i]->generate_code(gen, types);
                // Move argument to appropriate register (x86-64 calling convention)
                switch (i) {
                    case 0: 
                        gen.emit_mov_reg_reg(7, 0); // RDI = RAX (1st arg)
                        break;
                    case 1: 
                        gen.emit_mov_reg_reg(6, 0); // RSI = RAX (2nd arg)
                        break;  
                    case 2: gen.emit_mov_reg_reg(2, 0); break;  // RDX = RAX (3rd arg)
                    case 3: gen.emit_mov_reg_reg(1, 0); break;  // RCX = RAX (4th arg)
                    case 4: gen.emit_mov_reg_reg(8, 0); break;  // R8 = RAX (5th arg)
                    case 5: gen.emit_mov_reg_reg(9, 0); break;  // R9 = RAX (6th arg)
                }
            }
            
            // Generate the optimized direct function call
            gen.emit_call(function_name);
            
            // Set appropriate result type based on the method
            if (sub_object == "time" && (method_name == "now" || method_name == "nowNanos")) {
                result_type = DataType::INT64;
            } else if (sub_object == "process" && method_name == "cwd") {
                result_type = DataType::STRING;
            } else if (sub_object == "timer" && (method_name == "setTimeout" || method_name == "setInterval" || method_name == "setImmediate")) {
                result_type = DataType::INT64; // Timer ID
            } else if (sub_object == "timer" && (method_name == "clearTimeout" || method_name == "clearInterval" || method_name == "clearImmediate")) {
                result_type = DataType::BOOLEAN; // Success/failure
            } else {
                result_type = DataType::ANY;
            }
            
            return; // Skip normal method call handling
        }
    }
    
    // First, generate code for the object expression and get its result
    object->generate_code(gen, types);
    DataType object_type = object->result_type;
    
    // Handle different types of objects for method calls
    if (object_type == DataType::STRING) {
        // Handle string methods like match, replace, search, split
        if (method_name == "match") {
            // String.match() method - returns array of matches
            // Save string pointer to stack location
            gen.emit_mov_mem_reg(-8, 0); // Save string at RBP-8
            
            if (arguments.size() > 0) {
                // Generate code for regex argument
                arguments[0]->generate_code(gen, types);
                
                // Set up call: string_match(string_ptr, regex_ptr)
                gen.emit_mov_reg_mem(7, -8);  // RDI = string pointer
                gen.emit_mov_reg_reg(6, 0);   // RSI = regex pointer
                gen.emit_call("__string_match");
                result_type = DataType::TENSOR; // Array of matches
            } else {
                throw std::runtime_error("String.match() requires a regex argument");
            }
        } else if (method_name == "replace") {
            // String.replace() method
            gen.emit_mov_mem_reg(-8, 0); // Save string at RBP-8
            
            if (arguments.size() >= 2) {
                // Generate code for pattern (regex or string)
                arguments[0]->generate_code(gen, types);
                gen.emit_mov_mem_reg(-16, 0); // Save pattern at RBP-16
                
                // Generate code for replacement string
                arguments[1]->generate_code(gen, types);
                
                // Set up call: string_replace(string_ptr, pattern_ptr, replacement_ptr)
                gen.emit_mov_reg_mem(7, -8);   // RDI = string pointer
                gen.emit_mov_reg_mem(6, -16);  // RSI = pattern pointer
                gen.emit_mov_reg_reg(2, 0);    // RDX = replacement pointer
                gen.emit_call("__string_replace");
                result_type = DataType::STRING;
            } else {
                throw std::runtime_error("String.replace() requires pattern and replacement arguments");
            }
        } else if (method_name == "search") {
            // String.search() method - returns index of first match
            gen.emit_mov_mem_reg(-8, 0); // Save string at RBP-8
            
            if (arguments.size() > 0) {
                arguments[0]->generate_code(gen, types);
                
                gen.emit_mov_reg_mem(7, -8);  // RDI = string pointer  
                gen.emit_mov_reg_reg(6, 0);   // RSI = regex pointer
                gen.emit_call("__string_search");
                result_type = DataType::FLOAT64; // Index or -1
            } else {
                throw std::runtime_error("String.search() requires a regex argument");
            }
        } else if (method_name == "split") {
            // String.split() method - returns array of strings
            gen.emit_mov_mem_reg(-8, 0); // Save string at RBP-8
            
            if (arguments.size() > 0) {
                arguments[0]->generate_code(gen, types);
                
                gen.emit_mov_reg_mem(7, -8);  // RDI = string pointer
                gen.emit_mov_reg_reg(6, 0);   // RSI = delimiter/regex pointer
                gen.emit_call("__string_split");
                result_type = DataType::TENSOR; // Array of strings
            } else {
                throw std::runtime_error("String.split() requires a delimiter argument");
            }
        } else {
            throw std::runtime_error("Unknown string method: " + method_name);
        }
    } else if (object_type == DataType::REGEX) {
        // Handle regex methods like test, exec
        if (method_name == "test") {
            gen.emit_mov_mem_reg(-8, 0); // Save regex at RBP-8
            
            if (arguments.size() > 0) {
                arguments[0]->generate_code(gen, types);
                
                gen.emit_mov_reg_mem(7, -8);  // RDI = regex pointer
                gen.emit_mov_reg_reg(6, 0);   // RSI = string pointer
                gen.emit_call("__regex_test");
                result_type = DataType::BOOLEAN;
            } else {
                throw std::runtime_error("RegExp.test() requires a string argument");
            }
        } else if (method_name == "exec") {
            gen.emit_mov_mem_reg(-8, 0); // Save regex at RBP-8
            
            if (arguments.size() > 0) {
                arguments[0]->generate_code(gen, types);
                
                gen.emit_mov_reg_mem(7, -8);  // RDI = regex pointer
                gen.emit_mov_reg_reg(6, 0);   // RSI = string pointer
                gen.emit_call("__regex_exec");
                result_type = DataType::TENSOR; // Match object/array
            } else {
                throw std::runtime_error("RegExp.exec() requires a string argument");
            }
        } else {
            throw std::runtime_error("Unknown regex method: " + method_name);
        }
    } else if (object_type == DataType::TENSOR) {
        // Handle array/tensor methods
        if (method_name == "push") {
            gen.emit_mov_mem_reg(-8, 0); // Save array pointer
            
            for (size_t i = 0; i < arguments.size(); i++) {
                arguments[i]->generate_code(gen, types);
                
                gen.emit_mov_reg_mem(7, -8);  // RDI = array pointer
                gen.emit_mov_reg_reg(6, 0);   // RSI = value to push
                gen.emit_call("__array_push");
            }
            result_type = DataType::VOID;
        } else if (method_name == "pop") {
            gen.emit_mov_reg_reg(7, 0);   // RDI = array pointer
            gen.emit_call("__array_pop");
            result_type = DataType::FLOAT64; // Popped value
        } else {
            throw std::runtime_error("Unknown array method: " + method_name);
        }
    } else {
        // For other types, try a generic method call
        // This is a fallback for custom objects or future types
        gen.emit_mov_mem_reg(-8, 0); // Save object pointer
        
        // For now, we'll emit a placeholder call
        // In a full implementation, this would do dynamic method lookup
        std::string method_label = "__dynamic_method_" + method_name;
        gen.emit_mov_reg_mem(7, -8);  // RDI = object pointer
        
        // Set up arguments
        for (size_t i = 0; i < arguments.size() && i < 5; i++) {
            arguments[i]->generate_code(gen, types);
            gen.emit_mov_mem_reg(-(int64_t)(i + 2) * 8, 0); // Save to stack
        }
        
        // Load arguments into registers (starting from RSI since RDI has object)
        for (size_t i = 0; i < arguments.size() && i < 5; i++) {
            switch (i) {
                case 0: gen.emit_mov_reg_mem(6, -16); break;  // RSI
                case 1: gen.emit_mov_reg_mem(2, -24); break;  // RDX
                case 2: gen.emit_mov_reg_mem(1, -32); break;  // RCX
                case 3: gen.emit_mov_reg_mem(8, -40); break;  // R8
                case 4: gen.emit_mov_reg_mem(9, -48); break;  // R9
            }
        }
        
        gen.emit_call(method_label);
        result_type = DataType::ANY; // Unknown return type for dynamic calls
    }
    
    if (is_awaited) {
        gen.emit_promise_await(0);
    }
}

void ArrayLiteral::generate_code(CodeGenerator& gen, TypeInference& types) {
    std::cout << "[DEBUG] ArrayLiteral::generate_code - Creating array with " << elements.size() << " elements" << std::endl;
    std::cout.flush();
    
    // Check if this array literal is being assigned to a typed array variable
    DataType target_type = types.get_current_assignment_target_type();
    DataType element_type = types.get_current_assignment_array_element_type();
    
    // Determine if we should use typed arrays:
    // - Must have a specific element type (not ANY)
    // - Element type must NOT be STRING (they use dynamic arrays)
    bool is_typed_array = (target_type == DataType::ARRAY && 
                          element_type != DataType::ANY && 
                          element_type != DataType::STRING);
    
    if (elements.size() == 0) {
        // Empty array case
        gen.emit_mov_reg_imm(7, 0);  // RDI = 0 (empty array)
        
        if (is_typed_array) {
            // Create typed array based on element type
            switch (element_type) {
                case DataType::INT64:
                    gen.emit_call("__array_create_int64");
                    std::cout << "[DEBUG] ArrayLiteral: Created int64 typed array" << std::endl;
                    break;
                case DataType::FLOAT64:
                    gen.emit_call("__array_create_float64");
                    std::cout << "[DEBUG] ArrayLiteral: Created float64 typed array" << std::endl;
                    break;
                case DataType::INT32:
                    gen.emit_call("__array_create_int32");
                    std::cout << "[DEBUG] ArrayLiteral: Created int32 typed array" << std::endl;
                    break;
                case DataType::FLOAT32:
                    gen.emit_call("__array_create_float32");
                    std::cout << "[DEBUG] ArrayLiteral: Created float32 typed array" << std::endl;
                    break;
                case DataType::INT8:
                case DataType::UINT8:
                    // Use int32 creation function for 8-bit types (will be converted)
                    gen.emit_call("__array_create_int32");
                    std::cout << "[DEBUG] ArrayLiteral: Created int8/uint8 typed array (using int32)" << std::endl;
                    break;
                case DataType::INT16:
                case DataType::UINT16:
                    // Use int32 creation function for 16-bit types (will be converted)
                    gen.emit_call("__array_create_int32");
                    std::cout << "[DEBUG] ArrayLiteral: Created int16/uint16 typed array (using int32)" << std::endl;
                    break;
                case DataType::UINT32:
                    gen.emit_call("__array_create_int32");
                    std::cout << "[DEBUG] ArrayLiteral: Created uint32 typed array (using int32)" << std::endl;
                    break;
                case DataType::UINT64:
                    gen.emit_call("__array_create_int64");
                    std::cout << "[DEBUG] ArrayLiteral: Created uint64 typed array (using int64)" << std::endl;
                    break;
                case DataType::STRING:
                    // Strings always use dynamic arrays for flexibility
                    gen.emit_call("__array_create_dynamic");
                    std::cout << "[DEBUG] ArrayLiteral: Created dynamic array for string type" << std::endl;
                    break;
                default:
                    // Fallback to dynamic array
                    gen.emit_call("__array_create_dynamic");
                    std::cout << "[DEBUG] ArrayLiteral: Created dynamic array (fallback)" << std::endl;
                    break;
            }
        } else {
            // Create dynamic array (mixed types)
            gen.emit_call("__array_create_dynamic");
            std::cout << "[DEBUG] ArrayLiteral: Created dynamic array" << std::endl;
        }
    } else {
        // Non-empty array
        gen.emit_mov_reg_imm(7, 0);  // RDI = 0 (empty array)
        
        if (is_typed_array) {
            // Create typed array based on element type
            switch (element_type) {
                case DataType::INT64:
                    gen.emit_call("__array_create_int64");
                    break;
                case DataType::FLOAT64:
                    gen.emit_call("__array_create_float64");
                    break;
                case DataType::INT32:
                    gen.emit_call("__array_create_int32");
                    break;
                case DataType::FLOAT32:
                    gen.emit_call("__array_create_float32");
                    break;
                case DataType::INT8:
                case DataType::UINT8:
                case DataType::INT16:
                case DataType::UINT16:
                case DataType::UINT32:
                    gen.emit_call("__array_create_int32");
                    break;
                case DataType::UINT64:
                    gen.emit_call("__array_create_int64");
                    break;
                case DataType::STRING:
                    // Strings always use dynamic arrays for flexibility
                    gen.emit_call("__array_create_dynamic");
                    break;
                default:
                    // Fallback to dynamic array
                    gen.emit_call("__array_create_dynamic");
                    break;
            }
        } else {
            // Create dynamic array (mixed types)
            gen.emit_call("__array_create_dynamic");
        }
        
        // Store array pointer in a safe stack location
        gen.emit_mov_mem_reg(-64, 0); // Save array pointer to stack[rbp-64]
        
        // Push each element into the array
        for (size_t i = 0; i < elements.size(); i++) {
            std::cout << "[DEBUG] ArrayLiteral: Processing element " << i << std::endl;
            std::cout.flush();
            
            // First, restore array pointer to a register
            gen.emit_mov_reg_mem(3, -64); // RBX = array pointer from stack[rbp-64]
            
            // Set element type context for typed arrays
            if (is_typed_array) {
                types.set_current_element_type_context(element_type);
            } else {
                types.clear_element_type_context();
            }
            
            // Generate the element value
            elements[i]->generate_code(gen, types);
            // RAX now contains the element value
            
            // Clear element type context after generation
            types.clear_element_type_context();
            
            // Set up parameters for appropriate push function
            gen.emit_mov_reg_reg(7, 3); // RDI = array pointer (from RBX)
            gen.emit_mov_reg_reg(6, 0); // RSI = value (from RAX)
            
            if (is_typed_array) {
                // Use typed push function
                switch (element_type) {
                    case DataType::INT64:
                        gen.emit_call("__array_push_int64_typed");
                        break;
                    case DataType::FLOAT64:
                        gen.emit_call("__array_push_float64_typed");
                        break;
                    case DataType::INT32:
                        gen.emit_call("__array_push_int32_typed");
                        break;
                    case DataType::FLOAT32:
                        gen.emit_call("__array_push_float32_typed");
                        break;
                    case DataType::INT8:
                    case DataType::UINT8:
                    case DataType::INT16:
                    case DataType::UINT16:
                    case DataType::UINT32:
                        gen.emit_call("__array_push_int32_typed");
                        break;
                    case DataType::UINT64:
                        gen.emit_call("__array_push_int64_typed");
                        break;
                    case DataType::STRING:
                        // Strings always use dynamic push
                        gen.emit_call("__array_push_dynamic");
                        break;
                    default:
                        // Fallback to dynamic push
                        gen.emit_call("__array_push_dynamic");
                        break;
                }
                std::cout << "[DEBUG] ArrayLiteral: Called typed push for element " << i << std::endl;
            } else {
                // Use dynamic push function
                gen.emit_call("__array_push_dynamic");
                std::cout << "[DEBUG] ArrayLiteral: Called dynamic push for element " << i << std::endl;
            }
        }
        
        // Return the array pointer in RAX
        gen.emit_mov_reg_mem(0, -64); // RAX = array pointer from stack[rbp-64]
    }
    
    std::cout << "[DEBUG] ArrayLiteral::generate_code complete" << std::endl;
    std::cout.flush();
    
    result_type = DataType::ARRAY;
}

void ObjectLiteral::generate_code(CodeGenerator& gen, TypeInference& types) {
    // Create an object using the existing runtime object system
    // Use a special class name for object literals
    
    // Create string literal for the object literal class name
    static const char* object_literal_class = "ObjectLiteral";
    
    // Call __object_create with class name and property count
    gen.emit_mov_reg_imm(7, reinterpret_cast<int64_t>(object_literal_class)); // RDI = class_name
    gen.emit_mov_reg_imm(6, properties.size()); // RSI = property count
    gen.emit_call("__object_create");
    
    // RAX now contains the object_address
    // Store it temporarily while we add properties
    int64_t object_offset = types.allocate_variable("__temp_object_" + std::to_string(rand()), DataType::CLASS_INSTANCE);
    gen.emit_mov_mem_reg(object_offset, 0); // Save object_address
    
    // Add each property to the object using property indices
    for (size_t i = 0; i < properties.size(); i++) {
        const auto& prop = properties[i];
        
        // First set the property name
        // Store property name in static storage for the runtime call
        static std::unordered_map<std::string, const char*> property_name_storage;
        auto it = property_name_storage.find(prop.first);
        const char* name_ptr;
        if (it != property_name_storage.end()) {
            name_ptr = it->second;
        } else {
            // Allocate permanent storage for this property name
            char* permanent_name = new char[prop.first.length() + 1];
            strcpy(permanent_name, prop.first.c_str());
            property_name_storage[prop.first] = permanent_name;
            name_ptr = permanent_name;
        }
        
        // Call __object_set_property_name(object_address, property_index, property_name)
        gen.emit_mov_reg_mem(7, object_offset); // RDI = object_address
        gen.emit_mov_reg_imm(6, i); // RSI = property_index
        gen.emit_mov_reg_imm(2, reinterpret_cast<int64_t>(name_ptr)); // RDX = property_name
        gen.emit_call("__object_set_property_name");
        
        // Generate code for the property value
        prop.second->generate_code(gen, types);
        
        // Set up call to __object_set_property(object_address, property_index, value)
        gen.emit_mov_reg_reg(2, 0); // RDX = value (save from RAX)
        gen.emit_mov_reg_mem(7, object_offset); // RDI = object_address
        gen.emit_mov_reg_imm(6, i); // RSI = property_index
        gen.emit_call("__object_set_property");
    }
    
    // Return the object_address in RAX
    gen.emit_mov_reg_mem(0, object_offset);
    result_type = DataType::CLASS_INSTANCE; // Objects are class instances
}

void TypedArrayLiteral::generate_code(CodeGenerator& gen, TypeInference& types) {
    // Create typed array with initial capacity - maximum performance
    gen.emit_mov_reg_imm(7, elements.size() > 0 ? elements.size() : 8); // RDI = initial capacity
    
    // Call appropriate typed array creation function based on type
    switch (array_type) {
        case DataType::INT32:
            gen.emit_call("__typed_array_create_int32");
            break;
        case DataType::INT64:
            gen.emit_call("__typed_array_create_int64");
            break;
        case DataType::FLOAT32:
            gen.emit_call("__typed_array_create_float32");
            break;
        case DataType::FLOAT64:
        // FLOAT64 is the standard numeric type (equivalent to JavaScript's number)
            gen.emit_call("__typed_array_create_float64");
            break;
        case DataType::UINT8:
            gen.emit_call("__typed_array_create_uint8");
            break;
        case DataType::UINT16:
            gen.emit_call("__typed_array_create_uint16");
            break;
        case DataType::UINT32:
            gen.emit_call("__typed_array_create_uint32");
            break;
        case DataType::UINT64:
            gen.emit_call("__typed_array_create_uint64");
            break;
        default:
            throw std::runtime_error("Unsupported typed array type");
    }
    
    gen.emit_mov_mem_reg(-16, 0); // Save array pointer on stack
    
    // Push each element into the typed array using appropriate typed push function
    for (const auto& element : elements) {
        element->generate_code(gen, types);
        gen.emit_mov_reg_mem(7, -16); // RDI = array pointer from stack
        gen.emit_mov_reg_reg(6, 0); // RSI = value to push
        
        // Call appropriate push function based on type for maximum performance
        switch (array_type) {
            case DataType::INT32:
                gen.emit_call("__typed_array_push_int32");
                break;
            case DataType::INT64:
                gen.emit_call("__typed_array_push_int64");
                break;
            case DataType::FLOAT32:
                gen.emit_call("__typed_array_push_float32");
                break;
            case DataType::FLOAT64:
            // FLOAT64 is the standard numeric type (equivalent to JavaScript's number)
                gen.emit_call("__typed_array_push_float64");
                break;
            case DataType::UINT8:
                gen.emit_call("__typed_array_push_uint8");
                break;
            case DataType::UINT16:
                gen.emit_call("__typed_array_push_uint16");
                break;
            case DataType::UINT32:
                gen.emit_call("__typed_array_push_uint32");
                break;
            case DataType::UINT64:
                gen.emit_call("__typed_array_push_uint64");
                break;
            default:
                throw std::runtime_error("Unsupported typed array type");
        }
    }
    
    // Return the array pointer in RAX
    gen.emit_mov_reg_mem(0, -16); // Load array pointer from stack
    result_type = array_type; // Set to the specific typed array type
}

void ArrayAccess::generate_code(CodeGenerator& gen, TypeInference& types) {
    
    // First check if the object is a class instance with operator[] overload
    bool use_operator_overload = false;
    std::string class_name;
    uint32_t class_type_id = 0;  // Store type ID for efficiency
    
    // Try to determine if object is a class instance
    if (auto* var_expr = dynamic_cast<Identifier*>(object.get())) {
        DataType var_type = types.get_variable_type(var_expr->name);
        if (var_type == DataType::CLASS_INSTANCE) {
            class_type_id = types.get_variable_class_type_id(var_expr->name);
            if (class_type_id != 0) {
                auto* compiler = get_current_compiler();
                if (compiler) {
                    // Use type ID directly for operator overload checks (more efficient)
                    bool has_bracket_overload = compiler->has_operator_overload(class_type_id, TokenType::LBRACKET);
                    bool has_slice_overload = compiler->has_operator_overload(class_type_id, TokenType::SLICE_BRACKET);
                    
                    // Only convert to class name if we actually need operator overloading
                    if (has_bracket_overload || has_slice_overload) {
                        class_name = compiler->get_class_name_from_type_id(class_type_id);
                    }
                    
                    // Prefer slice operator for slice expressions, bracket operator otherwise
                    if (is_slice_expression && has_slice_overload) {
                        use_operator_overload = true;
                    } else if (!is_slice_expression && has_bracket_overload) {
                        use_operator_overload = true;
                    } else if (has_bracket_overload) {
                        // Fallback to bracket operator if available
                        use_operator_overload = true;
                    }
                }
            }
        } else if (var_type == DataType::ARRAY) {
            // Handle simplified Array access directly
            
            // Get the array variable offset
            int64_t array_offset = types.get_variable_offset(var_expr->name);
            gen.emit_mov_reg_mem(7, array_offset); // RDI = array pointer
            
            // Generate code for the index
            if (index) {
                index->generate_code(gen, types);
                gen.emit_mov_reg_reg(6, 0); // RSI = index
            } else if (!slices.empty()) {
                // For slice syntax, use slice function instead
                slices[0]->generate_code(gen, types);
                // TODO: Handle slice properly
                gen.emit_mov_reg_reg(6, 0); // RSI = slice object
            } else {
                gen.emit_mov_reg_imm(6, 0); // RSI = 0 (default index)
            }
            
            // Check if this is a typed array and use the appropriate access function
            DataType element_type = types.get_variable_array_element_type(var_expr->name);
            if (element_type != DataType::ANY) {
                // This is a typed array, use the appropriate typed access function
                switch (element_type) {
                    case DataType::INT64:
                        gen.emit_call("__array_access_int64");
                        result_type = DataType::INT64;
                        break;
                    case DataType::FLOAT64:
                        gen.emit_call("__array_access_float64");
                        result_type = DataType::FLOAT64;
                        break;
                    case DataType::INT32:
                        gen.emit_call("__array_access_int32");
                        result_type = DataType::INT32;
                        break;
                    case DataType::FLOAT32:
                        gen.emit_call("__array_access_float32");
                        result_type = DataType::FLOAT32;
                        break;
                    default:
                        // Error for unsupported types - should not happen with proper type checking
                        throw std::runtime_error("Unsupported array element type for typed array access: " + 
                                                std::to_string(static_cast<int>(element_type)));
                        break;
                }
            } else {
                // Dynamic array, use generic access
                gen.emit_call("__array_access");
                result_type = DataType::FLOAT64;
            }
            
            return;
        }
    }
    
    if (use_operator_overload) {
        // Determine the index expression as a string for type inference
        std::string index_expr_str = "";
        if (is_slice_expression) {
            index_expr_str = slice_expression;
        } else if (index) {
            // Extract the expression string using the helper method
            index_expr_str = types.extract_expression_string(index.get());
            if (index_expr_str.empty()) {
                index_expr_str = "complex_expression";
            }
        } else {
            // Handle case where we have slices but no index (new slice syntax)
            index_expr_str = "slice_expression";
        }
        
        // Use enhanced type inference to determine the best operator overload
        // Use type ID version for efficiency if available
        DataType index_type = (class_type_id != 0) 
            ? types.infer_operator_index_type(class_type_id, index_expr_str)
            : types.infer_operator_index_type(class_name, index_expr_str);
        
        // Generate argument 0 (object)
        object->generate_code(gen, types);
        gen.emit_mov_reg_reg(7, 0);  // Move object to RDI (first parameter)
        
        // Generate argument 1 (index/string) and place in RSI
        if (is_slice_expression) {
            // For slice expressions, create a string literal directly
            auto string_literal = std::make_unique<StringLiteral>(slice_expression);
            string_literal->generate_code(gen, types);
        } else if (index) {
            // For normal expressions, evaluate them
            index->generate_code(gen, types);
        } else if (!slices.empty()) {
            // For new slice syntax, generate slice object code
            slices[0]->generate_code(gen, types);
        } else {
            // Fallback - generate a zero index
            gen.emit_mov_reg_imm(0, 0);
        }
        gen.emit_mov_reg_reg(6, 0);  // Move string/index to RSI (second parameter)
        
        // Find the best operator overload based on the inferred index type
        auto* compiler = get_current_compiler();
        if (compiler) {
            std::vector<DataType> operand_types = {index_type};
            // Choose the appropriate operator token based on whether it's a slice expression
            // Use type ID directly for efficiency (avoid string conversion)
            uint32_t current_class_type_id = 0;
            if (auto* var_expr = dynamic_cast<Identifier*>(object.get())) {
                current_class_type_id = types.get_variable_class_type_id(var_expr->name);
            }
            TokenType operator_token = (is_slice_expression && current_class_type_id != 0 && compiler->has_operator_overload(current_class_type_id, TokenType::SLICE_BRACKET)) 
                                     ? TokenType::SLICE_BRACKET 
                                     : TokenType::LBRACKET;
            const auto* best_overload = compiler->find_best_operator_overload(class_name, operator_token, operand_types);
            
            if (best_overload) {
                // Call the specific operator overload function
                std::string op_name = best_overload->function_name;
                gen.emit_call(op_name);
                result_type = best_overload->return_type;
            } else {
                // No typed overload found, try to fall back to ANY overload
                
                // Try ANY type overload as fallback
                std::vector<DataType> any_operand_types = {DataType::ANY};
                const auto* any_overload = compiler->find_best_operator_overload(class_name, operator_token, any_operand_types);
                
                if (any_overload) {
                    gen.emit_call(any_overload->function_name);
                    result_type = any_overload->return_type;
                } else {
                    // Last resort: try direct function name construction for compatibility
                    std::string param_signature;
                    if (is_slice_expression || index_type == DataType::STRING) {
                        param_signature = std::to_string(static_cast<int>(DataType::STRING)); // string parameter
                    } else {
                        param_signature = "any"; // ANY type parameter
                    }
                    
                    std::string op_function_name = class_name + "::__op_" + std::to_string(static_cast<int>(operator_token)) + "_any_" + param_signature + "__";
                    gen.emit_call(op_function_name);
                    result_type = DataType::CLASS_INSTANCE; // Assume operator overloads return class instances
                }
            }
        } else {
            result_type = DataType::ANY;
        }
    } else {
        // Check if this is a class instance with property access optimization
        bool optimized_property_access = false;
        if (auto* var_expr = dynamic_cast<Identifier*>(object.get())) {
            DataType var_type = types.get_variable_type(var_expr->name);
            if (var_type == DataType::CLASS_INSTANCE) {
                uint32_t class_type_id = types.get_variable_class_type_id(var_expr->name);
                std::string class_name;
                if (class_type_id != 0) {
                    auto* compiler = get_current_compiler();
                    if (compiler) {
                        class_name = compiler->get_class_name_from_type_id(class_type_id);
                    }
                }
                
                // Check if index is a string literal - can optimize to direct property access
                auto* string_literal = dynamic_cast<StringLiteral*>(index.get());
                if (!class_name.empty() && string_literal) {
                    std::string property_name = string_literal->value;
                    
                    // Remove quotes if present
                    if (property_name.size() >= 2 && property_name.front() == '"' && property_name.back() == '"') {
                        property_name = property_name.substr(1, property_name.size() - 2);
                    }
                    
                    auto* compiler = get_current_compiler();
                    if (compiler) {
                        ClassInfo* class_info = compiler->get_class(class_name);
                        if (class_info) {
                            // Find the property in the class fields
                            int64_t property_offset = -1;
                            DataType property_type = DataType::ANY;
                            for (size_t i = 0; i < class_info->fields.size(); ++i) {
                                if (class_info->fields[i].name == property_name) {
                                    // Object layout: [class_name_ptr][property_count][ref_count][dynamic_map_ptr][property0][property1]...
                                    // Properties start at offset 32 (4 * 8 bytes for metadata)
                                    property_offset = OBJECT_PROPERTIES_START_OFFSET + (i * 8); // Each property is 8 bytes
                                    property_type = class_info->fields[i].type;
                                    break;
                                }
                            }
                            
                            if (property_offset != -1) {
                                std::cout << "[DEBUG] ArrayAccess: Using safe property access for d[\"" << property_name << "\"]" << std::endl;
                                
                                // Use safe runtime property lookup instead of direct access to handle nullptr properly
                                // This ensures JavaScript-compatible undefined behavior for uninitialized properties
                                
                                // Generate code for the object expression
                                object->generate_code(gen, types);
                                gen.emit_mov_reg_reg(7, 0); // RDI = object pointer
                                
                                // Create string for property name
                                static std::unordered_map<std::string, const char*> property_name_storage;
                                auto it = property_name_storage.find(property_name);
                                const char* name_ptr;
                                if (it != property_name_storage.end()) {
                                    name_ptr = it->second;
                                } else {
                                    // Allocate permanent storage for this property name
                                    char* permanent_name = new char[property_name.length() + 1];
                                    strcpy(permanent_name, property_name.c_str());
                                    property_name_storage[property_name] = permanent_name;
                                    name_ptr = permanent_name;
                                }
                                
                                gen.emit_mov_reg_imm(6, reinterpret_cast<int64_t>(name_ptr)); // RSI = property_name
                                gen.emit_mov_reg_imm(2, reinterpret_cast<int64_t>(class_info)); // RDX = class_info pointer
                                
                                // Call safe runtime property lookup that handles nullptr properly
                                gen.emit_call("__class_property_lookup");
                                
                                result_type = DataType::ANY; // Runtime lookup returns ANY
                                std::cout << "[DEBUG] ArrayAccess: Set result_type to ANY for runtime property lookup" << std::endl;
                                optimized_property_access = true;
                            } else {
                                // Property not found in static fields, try dynamic property lookup
                                std::cout << "[DEBUG] ArrayAccess: Property '" << property_name << "' not found in static fields, using dynamic property lookup" << std::endl;
                                
                                // Generate code for the object expression  
                                object->generate_code(gen, types);
                                gen.emit_mov_reg_reg(7, 0); // RDI = object pointer
                                
                                // Use the same approach as ExpressionPropertyAccess for consistent behavior
                                static std::unordered_map<std::string, const char*> property_name_storage;
                                auto it = property_name_storage.find(property_name);
                                const char* name_ptr;
                                if (it != property_name_storage.end()) {
                                    name_ptr = it->second;
                                } else {
                                    // Allocate permanent storage for this property name
                                    char* permanent_name = new char[property_name.length() + 1];
                                    strcpy(permanent_name, property_name.c_str());
                                    property_name_storage[property_name] = permanent_name;
                                    name_ptr = permanent_name;
                                }
                                
                                gen.emit_mov_reg_imm(6, reinterpret_cast<int64_t>(name_ptr)); // RSI = property_name
                                gen.emit_call("__dynamic_property_get");
                                
                                result_type = DataType::ANY; // Dynamic properties return ANY
                                optimized_property_access = true;
                            }
                        }
                    }
                }
                // Check if index is a variable containing a string - emit runtime property lookup
                else if (auto* var_index = dynamic_cast<Identifier*>(index.get())) {
                    DataType index_type = types.get_variable_type(var_index->name);
                    if (index_type == DataType::STRING || index_type == DataType::ANY) {
                        std::cout << "[DEBUG] ArrayAccess: Optimizing d[prop] with runtime property lookup" << std::endl;
                        
                        auto* compiler = get_current_compiler();
                        if (compiler) {
                            ClassInfo* class_info = compiler->get_class(class_name);
                            if (class_info) {
                                // Generate code for the object expression
                                object->generate_code(gen, types);
                                gen.emit_mov_reg_reg(7, 0); // RDI = object pointer
                                
                                // Generate code for the property name string
                                index->generate_code(gen, types);
                                gen.emit_mov_reg_reg(6, 0); // RSI = property name string
                                
                                // Pass class info as third parameter
                                gen.emit_mov_reg_imm(2, reinterpret_cast<int64_t>(class_info)); // RDX = class_info pointer
                                
                                // Call optimized runtime property lookup
                                gen.emit_call("__class_property_lookup");
                                
                                result_type = DataType::ANY; // Runtime lookup returns ANY
                                std::cout << "[DEBUG] ArrayAccess: Set result_type to ANY for variable index runtime lookup" << std::endl;
                                optimized_property_access = true;
                            }
                        }
                    }
                }
            }
        }
        
        if (!optimized_property_access) {
            // Standard array access
            // Generate code for the object expression
            object->generate_code(gen, types);
            
            // Save object on stack
            gen.emit_sub_reg_imm(4, 8);   // sub rsp, 8 (allocate stack space)
            gen.emit_mov_mem_rsp_reg(0, 0);   // mov [rsp], rax (save object on stack)
            
            // Generate code for the index expression
            if (index) {
                index->generate_code(gen, types);
            } else if (!slices.empty()) {
                // For new slice syntax, generate slice object code
                slices[0]->generate_code(gen, types);
            } else {
                // Fallback - generate a zero index
                gen.emit_mov_reg_imm(0, 0);
            }
            gen.emit_mov_reg_reg(6, 0); // Move index to RSI
            
            // Pop object into RDI
            gen.emit_mov_reg_mem(7, 0);   // mov rdi, [rsp] (load object from stack)
            gen.emit_add_reg_imm(4, 8);   // add rsp, 8 (restore stack)
            
            // Call array access function
            gen.emit_call("__array_access");
            
            // Result is in RAX
            result_type = DataType::ANY; // Array access returns unknown type for JavaScript compatibility
        }
    }
    
}

void Assignment::generate_code(CodeGenerator& gen, TypeInference& types) {
    std::cout << "[DEBUG] Assignment::generate_code called for variable: " << variable_name 
              << ", declared_type=" << static_cast<int>(declared_type) << std::endl;
    if (value) {
        // For reassignments (declared_type == ANY), look up the existing variable type
        DataType actual_declared_type = declared_type;
        if (declared_type == DataType::ANY) {
            // This might be a reassignment to an existing typed variable
            DataType existing_type = types.get_variable_type(variable_name);
            std::cout << "[DEBUG] Assignment: Checking existing variable type for '" << variable_name 
                      << "', existing_type=" << static_cast<int>(existing_type) << std::endl;
            if (existing_type != DataType::ANY) {
                actual_declared_type = existing_type;
                std::cout << "[DEBUG] Assignment: Reassignment detected, using existing variable type: " 
                          << static_cast<int>(actual_declared_type) << std::endl;
            }
        }
        
        // Set the assignment context for type-aware array creation
        if (actual_declared_type != DataType::ANY) {
            types.set_current_assignment_target_type(actual_declared_type);
            if (actual_declared_type == DataType::ARRAY && declared_element_type != DataType::ANY) {
                // This is a typed array like [int64], store the element type
                types.set_current_assignment_array_element_type(declared_element_type);
            }
        } else {
            types.clear_assignment_context();
        }
        
        value->generate_code(gen, types);
        
        // Clear assignment context after generation
        types.clear_assignment_context();
        
        DataType variable_type;
        if (actual_declared_type != DataType::ANY) {
            // Explicitly typed variable - use the declared type for performance
            variable_type = actual_declared_type;
        } else {
            // Untyped variable - infer type from value for arrays and other structured types
            // For property access results, preserve the specific type for better performance
            if (value->result_type == DataType::TENSOR || value->result_type == DataType::STRING || 
                value->result_type == DataType::REGEX || value->result_type == DataType::FUNCTION ||
                value->result_type == DataType::ARRAY || value->result_type == DataType::CLASS_INSTANCE) {
                // Arrays, tensors, strings, regex, functions, and class instances should preserve their type for proper method dispatch
                variable_type = value->result_type;
            } else if (auto* prop_access = dynamic_cast<ExpressionPropertyAccess*>(value.get())) {
                // Property access results: preserve the property's original type for performance
                variable_type = prop_access->result_type;
            } else if (auto* prop_access = dynamic_cast<PropertyAccess*>(value.get())) {
                // Property access results: preserve the property's original type for performance
                variable_type = prop_access->result_type;
            } else {
                // Other types keep as ANY for JavaScript compatibility
                // This allows dynamic type changes but sacrifices some performance
                variable_type = DataType::ANY;
            }
        }
        
        // Handle class instance assignments specially - robust object instance detection
        // std::cout << ", value->result_type: " << static_cast<int>(value->result_type) << std::endl;
        if (actual_declared_type == DataType::CLASS_INSTANCE || 
            (actual_declared_type == DataType::ANY && value->result_type == DataType::CLASS_INSTANCE)) {
            auto new_expr = dynamic_cast<NewExpression*>(value.get());
            if (new_expr) {
                // Set the class type information for this variable using type ID AND class name
                auto* compiler = get_current_compiler();
                if (compiler) {
                    uint32_t class_type_id = compiler->get_class_type_id(new_expr->class_name);
                    types.set_variable_class_type(variable_name, class_type_id);
                    types.set_variable_class_name(variable_name, new_expr->class_name); // Store class name for direct destructor calls
                    std::cout << "[DEBUG] Assignment: Set class type '" << new_expr->class_name 
                              << "' (id=" << class_type_id << ") for variable '" << variable_name << "'" << std::endl;
                }
            } else {
                // For copy assignments (obj3 = obj1), copy the class type ID from source variable
                auto* var_expr = dynamic_cast<Identifier*>(value.get());
                if (var_expr) {
                    uint32_t source_class_type_id = types.get_variable_class_type_id(var_expr->name);
                    std::string source_class_name = types.get_variable_class_name(var_expr->name);
                    if (source_class_type_id != 0) {
                        types.set_variable_class_type(variable_name, source_class_type_id);
                        types.set_variable_class_name(variable_name, source_class_name); // Copy class name too
                        std::cout << "[DEBUG] Assignment: Copied class type '" << source_class_name 
                                  << "' (id=" << source_class_type_id << ") from '" << var_expr->name 
                                  << "' to '" << variable_name << "'" << std::endl;
                    }
                }
            }
            // ALWAYS set the variable type to CLASS_INSTANCE for object instances
            // This includes both NewExpression and ObjectLiteral
            variable_type = DataType::CLASS_INSTANCE;
        }
        
        // ========== COMPREHENSIVE REFERENCE COUNTING: Handle ALL assignment transitions ==========
        // Check for reassignment BEFORE allocating variable to avoid false positives
        bool is_reassignment = types.variable_exists(variable_name);
        
        // Allocate or get the proper stack offset for this variable
        int64_t offset = types.allocate_variable(variable_name, variable_type);
        
        // Store array element type for typed arrays
        if (actual_declared_type == DataType::ARRAY && declared_element_type != DataType::ANY) {
            types.set_variable_array_element_type(variable_name, declared_element_type);
        }
        
        // Step 1: ONLY decrement reference count of OLD value if this is a reassignment
        if (is_reassignment) {
            DataType existing_var_type = types.get_variable_type(variable_name);
            if (existing_var_type == DataType::CLASS_INSTANCE) {
                // OLD value is a raw class instance - decrement directly
                gen.emit_mov_reg_mem(1, offset); // RCX = old class instance pointer
                gen.emit_mov_reg_imm(2, 0); // RDX = 0
                gen.emit_compare(1, 2); // Compare RCX with 0
                std::string skip_old_dec = "skip_old_dec_" + std::to_string(rand());
                gen.emit_jump_if_zero(skip_old_dec); // Skip if null
                gen.emit_ref_count_decrement(1, 2); // Decrement ref count, may call destructor
                gen.emit_label(skip_old_dec);
            } else if (existing_var_type == DataType::ANY) {
                // OLD value is DynamicValue - might contain class instance
                gen.emit_mov_reg_mem(1, offset); // RCX = old DynamicValue*
                gen.emit_mov_reg_imm(2, 0); // RDX = 0
                gen.emit_compare(1, 2); // Compare RCX with 0
                std::string skip_old_release = "skip_old_release_" + std::to_string(rand());
                gen.emit_jump_if_zero(skip_old_release); // Skip if null
                gen.emit_mov_reg_reg(7, 1); // RDI = DynamicValue*
                gen.emit_call("__dynamic_value_release_if_object"); // Runtime function to handle release
                gen.emit_label(skip_old_release);
            }
        }
        
        // Step 2: Handle NEW value assignment - ALL possible transitions
        if (variable_type == DataType::ANY) {
            // Target is ANY variable (DynamicValue container)
            // Source can be: raw class, raw primitive, DynamicValue with class, DynamicValue with primitive
            
            if (value->result_type == DataType::CLASS_INSTANCE) {
                // NEW: raw class -> ANY (wrap in DynamicValue and increment)
                gen.emit_mov_reg_imm(1, 0); // RCX = 0
                gen.emit_compare(0, 1); // Compare RAX with 0
                std::string skip_inc_class_to_any = "skip_inc_class_to_any_" + std::to_string(rand());
                gen.emit_jump_if_zero(skip_inc_class_to_any); // Skip if null
                gen.emit_ref_count_increment(0); // Increment ref count for RAX (class instance)
                gen.emit_label(skip_inc_class_to_any);
                
                gen.emit_mov_reg_reg(7, 0); // RDI = RAX (class instance pointer)
                gen.emit_call("__dynamic_value_create_from_object");
            } else if (value->result_type == DataType::ANY) {
                // NEW: DynamicValue -> ANY (copy DynamicValue, handling ref counting internally)
                gen.emit_mov_reg_reg(7, 0); // RDI = RAX (DynamicValue*)
                gen.emit_call("__dynamic_value_copy_with_refcount"); // Runtime handles ref counting
            } else {
                // NEW: primitive -> ANY (wrap in DynamicValue, no ref counting)
                switch (value->result_type) {
                    case DataType::FLOAT64:
                        gen.emit_mov_reg_reg(7, 0); // RDI = RAX
                        gen.emit_call("__dynamic_value_create_from_double");
                        break;
                    case DataType::INT64:
                        gen.emit_mov_reg_reg(7, 0); // RDI = RAX
                        gen.emit_call("__dynamic_value_create_from_int64");
                        break;
                    case DataType::BOOLEAN:
                        gen.emit_mov_reg_reg(7, 0); // RDI = RAX
                        gen.emit_call("__dynamic_value_create_from_bool");
                        break;
                    case DataType::STRING:
                        gen.emit_mov_reg_reg(7, 0); // RDI = RAX
                        gen.emit_call("__dynamic_value_create_from_string");
                        break;
                    case DataType::ARRAY:
                        gen.emit_mov_reg_reg(7, 0); // RDI = RAX
                        gen.emit_call("__dynamic_value_create_from_array");
                        break;
                    default:
                        gen.emit_mov_reg_reg(7, 0); // RDI = RAX
                        gen.emit_call("__dynamic_value_create_from_double");
                        break;
                }
            }
            // RAX now contains the DynamicValue*
            gen.emit_mov_mem_reg(offset, 0);
            
            // DEBUG: Call runtime debug function to track what's being stored
            gen.emit_mov_reg_reg(7, 5);  // RDI = RBP (frame pointer)
            gen.emit_mov_reg_imm(6, offset);  // RSI = offset
            gen.emit_mov_reg_reg(2, 0);  // RDX = value being stored (RAX)
            gen.emit_call("__debug_stack_store");
            
            std::cout << "[DEBUG] Assignment: Stored DynamicValue pointer at offset " << offset << std::endl;
            
        } else if (variable_type == DataType::CLASS_INSTANCE) {
            // Target is statically typed class variable
            // Source can be: raw class, DynamicValue with class, null
            
            if (value->result_type == DataType::CLASS_INSTANCE) {
                // Check if this is a NewExpression - if so, don't increment (transfer ownership)
                auto* new_expr = dynamic_cast<NewExpression*>(value.get());
                if (new_expr) {
                    // TRANSFER SEMANTICS: new object already has ref_count = 1, just transfer ownership
                    // No reference count increment needed - this is a move operation
                    std::cout << "[DEBUG] Assignment: Transfer semantics for NewExpression - no ref increment" << std::endl;
                } else {
                    // COPY SEMANTICS: existing object assignment - increment ref count
                    gen.emit_mov_reg_imm(1, 0); // RCX = 0
                    gen.emit_compare(0, 1); // Compare RAX with 0
                    std::string skip_inc_class_to_class = "skip_inc_class_to_class_" + std::to_string(rand());
                    gen.emit_jump_if_zero(skip_inc_class_to_class); // Skip if null
                    gen.emit_ref_count_increment(0); // Increment ref count for RAX
                    gen.emit_label(skip_inc_class_to_class);
                    std::cout << "[DEBUG] Assignment: Copy semantics for existing object - ref increment applied" << std::endl;
                }
                
            } else if (value->result_type == DataType::ANY) {
                // NEW: DynamicValue -> class (extract class and increment)
                gen.emit_mov_reg_reg(7, 0); // RDI = DynamicValue*
                gen.emit_call("__dynamic_value_extract_object_with_refcount"); // Extract and increment
                // RAX now contains the extracted class instance (ref count already incremented)
            }
            // else: null assignment - no ref counting needed
            
            gen.emit_mov_mem_reg(offset, 0); // Store the class instance pointer
            
            // DEBUG: Call runtime debug function to track what's being stored
            gen.emit_mov_reg_reg(7, 5);  // RDI = RBP (frame pointer)  
            gen.emit_mov_reg_imm(6, offset);  // RSI = offset
            gen.emit_mov_reg_reg(2, 0);  // RDX = value being stored (RAX)
            gen.emit_call("__debug_stack_store");
            
            std::cout << "[DEBUG] Assignment: Stored object pointer at offset " << offset << std::endl;
            
        } else {
            // Target is primitive type (string, int64, float64, etc.)
            // Handle DynamicValue extraction if needed
            
            if (value->result_type == DataType::ANY && variable_type != DataType::ANY) {
                // NEW: DynamicValue -> primitive (extract primitive value)
                gen.emit_mov_reg_reg(7, 0); // RDI = DynamicValue*
                
                switch (variable_type) {
                    case DataType::STRING:
                        gen.emit_call("__dynamic_value_extract_string");
                        break;
                    case DataType::INT64:
                        gen.emit_call("__dynamic_value_extract_int64");
                        break;
                    case DataType::FLOAT64:
                        gen.emit_call("__dynamic_value_extract_float64");
                        break;
                    default:
                        // For other types, keep as DynamicValue
                        break;
                }
            }
            // else: primitive -> primitive (no conversion needed)
            
            gen.emit_mov_mem_reg(offset, 0); // Store the primitive value
        }
        
        // Store the variable type in the type system for future lookups
        // Only store if we have meaningful type information or if the variable doesn't exist yet
        DataType existing_stored_type = types.get_variable_type(variable_name);
        if (variable_type != DataType::ANY || existing_stored_type == DataType::ANY) {
            types.set_variable_type(variable_name, variable_type);
            std::cout << "[DEBUG] Assignment: Stored type " << static_cast<int>(variable_type) 
                      << " for variable '" << variable_name << "'" << std::endl;
        } else {
            std::cout << "[DEBUG] Assignment: Preserving existing type " << static_cast<int>(existing_stored_type) 
                      << " for variable '" << variable_name << "' (not overwriting with ANY)" << std::endl;
        }
        
        result_type = variable_type;
    } else {
        // Variable declaration without value (e.g., "var str: string;")
        // Store the declared type for future lookups
        if (declared_type != DataType::ANY) {
            types.set_variable_type(variable_name, declared_type);
            std::cout << "[DEBUG] Assignment: Stored declared type " << static_cast<int>(declared_type) 
                      << " for variable '" << variable_name << "' (no value)" << std::endl;
            result_type = declared_type;
        } else {
            result_type = DataType::ANY;
        }
    }
}

void PostfixIncrement::generate_code(CodeGenerator& gen, TypeInference& types) {
    // Load the current value
    DataType var_type = types.get_variable_type(variable_name);
    int64_t offset = types.get_variable_offset(variable_name);
    gen.emit_mov_reg_mem(0, offset); // Load current value into register 0
    
    // Increment the value
    gen.emit_add_reg_imm(0, 1);
    
    // Store back to memory
    gen.emit_mov_mem_reg(offset, 0);
    
    result_type = var_type;
}

void PostfixDecrement::generate_code(CodeGenerator& gen, TypeInference& types) {
    // Load the current value
    DataType var_type = types.get_variable_type(variable_name);
    int64_t offset = types.get_variable_offset(variable_name);
    gen.emit_mov_reg_mem(0, offset); // Load current value into register 0
    
    // Decrement the value
    gen.emit_sub_reg_imm(0, 1);
    
    // Store back to memory
    gen.emit_mov_mem_reg(offset, 0);
    
    result_type = var_type;
}

void FunctionDecl::generate_code(CodeGenerator& gen, TypeInference& types) {
    // Reset type inference for new function to avoid offset conflicts
    types.reset_for_function();
    
    gen.emit_label(name);
    
    // Calculate estimated stack size (parameters + locals + temporaries)
    int64_t estimated_stack_size = (parameters.size() * 8) + (body.size() * 16) + 64;
    // Ensure minimum stack size and 16-byte alignment
    if (estimated_stack_size < 80) estimated_stack_size = 80;
    if (estimated_stack_size % 16 != 0) {
        estimated_stack_size += 16 - (estimated_stack_size % 16);
    }
    
    // Set stack size for this function
    gen.set_function_stack_size(estimated_stack_size);
    
    gen.emit_prologue();
    
    // Set up parameter types and save parameters from registers to stack
    for (size_t i = 0; i < parameters.size() && i < 6; i++) {
        const auto& param = parameters[i];
        types.set_variable_type(param.name, param.type);
        
        // Use fixed offsets for parameters to avoid conflicts with local variables  
        int stack_offset = -(int)(i + 1) * 8;  // Start at -8, -16, -24 etc
        types.set_variable_offset(param.name, stack_offset);
        
        switch (i) {
            case 0: gen.emit_mov_mem_reg(stack_offset, 7); break;  // save RDI
            case 1: gen.emit_mov_mem_reg(stack_offset, 6); break;  // save RSI
            case 2: gen.emit_mov_mem_reg(stack_offset, 2); break;  // save RDX
            case 3: gen.emit_mov_mem_reg(stack_offset, 1); break;  // save RCX
            case 4: gen.emit_mov_mem_reg(stack_offset, 8); break;  // save R8
            case 5: gen.emit_mov_mem_reg(stack_offset, 9); break;  // save R9
        }
    }
    
    // Handle stack parameters (beyond first 6)
    for (size_t i = 6; i < parameters.size(); i++) {
        const auto& param = parameters[i];
        types.set_variable_type(param.name, param.type);
        // Stack parameters are at positive offsets from RBP
        int stack_offset = (int)(i - 6 + 2) * 8;  // +16 for return addr and old RBP, then +8 for each param
        types.set_variable_offset(param.name, stack_offset);
    }
    
    // Generate function body
    bool has_explicit_return = false;
    for (const auto& stmt : body) {
        stmt->generate_code(gen, types);
        // Check if this statement is a return statement
        if (dynamic_cast<const ReturnStatement*>(stmt.get())) {
            has_explicit_return = true;
        }
    }
    
    // If no explicit return, add implicit return 0
    if (!has_explicit_return) {
        // CRITICAL: Add automatic cleanup for local variables before function return
        auto* compiler = get_current_compiler();
        if (compiler) {
            compiler->generate_scope_cleanup_code(gen, types);
        }
        
        gen.emit_mov_reg_imm(0, 0);  // mov rax, 0 (default return value)
        gen.emit_function_return();
    }
    
    // Register function with compiler for return type lookup
    auto* compiler = get_current_compiler();
    if (compiler) {
        Function func;
        func.name = name;
        func.return_type = (return_type == DataType::ANY) ? DataType::FLOAT64 : return_type;
        func.parameters = parameters;
        func.stack_size = 0; // Will be filled during execution
        compiler->register_function(name, func);
        
        //           << static_cast<int>(func.return_type) << std::endl;
    }
}

void IfStatement::generate_code(CodeGenerator& gen, TypeInference& types) {
    static int if_counter = 0;
    std::string else_label = "else_" + std::to_string(if_counter);
    std::string end_label = "end_if_" + std::to_string(if_counter);
    if_counter++;
    
    // Generate condition code - this puts the result in RAX
    condition->generate_code(gen, types);
    
    // Compare RAX with 0 (false)
    gen.emit_mov_reg_imm(1, 0);      // RCX = 0
    gen.emit_compare(0, 1);          // Compare RAX with RCX (0)
    gen.emit_jump_if_zero(else_label); // Jump to else if RAX == 0 (false)
    
    // Generate then body
    for (const auto& stmt : then_body) {
        stmt->generate_code(gen, types);
    }
    
    // Skip else body
    gen.emit_jump(end_label);
    
    // Generate else body
    gen.emit_label(else_label);
    for (const auto& stmt : else_body) {
        stmt->generate_code(gen, types);
    }
    
    gen.emit_label(end_label);
}

void ForLoop::generate_code(CodeGenerator& gen, TypeInference& types) {
    static int loop_counter = 0;
    std::string loop_start = "loop_start_" + std::to_string(loop_counter);
    std::string loop_end = "loop_end_" + std::to_string(loop_counter);
    loop_counter++;
    
    if (init) {
        init->generate_code(gen, types);
    }
    
    gen.emit_label(loop_start);
    
    if (condition) {
        condition->generate_code(gen, types);
        // Check if RAX (result of condition) is zero
        gen.emit_mov_reg_imm(1, 0); // RCX = 0
        gen.emit_compare(0, 1); // Compare RAX with 0
        gen.emit_jump_if_zero(loop_end);
    }
    
    for (const auto& stmt : body) {
        stmt->generate_code(gen, types);
    }
    
    if (update) {
        update->generate_code(gen, types);
    }
    
    gen.emit_jump(loop_start);
    gen.emit_label(loop_end);
}

void ForEachLoop::generate_code(CodeGenerator& gen, TypeInference& types) {
    static int loop_counter = 0;
    std::string loop_start = "foreach_start_" + std::to_string(loop_counter);
    std::string loop_end = "foreach_end_" + std::to_string(loop_counter);
    std::string loop_check = "foreach_check_" + std::to_string(loop_counter);
    
    // Create scoped variable names to avoid conflicts (let semantics)
    std::string scoped_index_name = "__foreach_" + std::to_string(loop_counter) + "_" + index_var_name;
    std::string scoped_value_name = "__foreach_" + std::to_string(loop_counter) + "_" + value_var_name;
    loop_counter++;
    
    // Generate code for the iterable expression
    iterable->generate_code(gen, types);
    
    // Store the iterable in a temporary location
    int64_t iterable_offset = types.allocate_variable("__temp_iterable_" + std::to_string(loop_counter - 1), iterable->result_type);
    gen.emit_mov_mem_reg(iterable_offset, 0); // Store iterable pointer
    
    // Initialize loop index to 0 (use let semantics - create scoped variable)
    int64_t index_offset = types.allocate_variable(scoped_index_name, DataType::INT64);
    gen.emit_mov_reg_imm(0, 0); // RAX = 0
    gen.emit_mov_mem_reg(index_offset, 0); // Store index = 0
    
    // But also create user-visible variables for the loop body  
    // Arrays use INT64 indices, objects use STRING keys
    DataType index_type = (iterable->result_type == DataType::TENSOR) ? DataType::INT64 : DataType::STRING;
    int64_t user_index_offset = types.allocate_variable(index_var_name, index_type);
    int64_t user_value_offset = types.allocate_variable(value_var_name, DataType::ANY);
    
    gen.emit_label(loop_check);
    
    // Check if we've reached the end of the iterable
    if (iterable->result_type == DataType::TENSOR) {
        // HIGHLY OPTIMIZED PATHWAY FOR TYPED ARRAYS
        // For arrays: check if index < array.length
        gen.emit_mov_reg_mem(7, iterable_offset); // RDI = array pointer
        gen.emit_call("__array_size"); // RAX = array size
        gen.emit_mov_reg_reg(3, 0); // RBX = array size
        gen.emit_mov_reg_mem(0, index_offset); // RAX = current index
        gen.emit_compare(0, 3); // Compare index with size
        
        // Use setge to check if index >= size, then jump if result is non-zero
        gen.emit_setge(1); // RCX = 1 if index >= size, 0 otherwise
        gen.emit_mov_reg_imm(2, 0); // RDX = 0
        gen.emit_compare(1, 2); // Compare RCX with 0
        gen.emit_jump_if_not_zero(loop_end); // Jump if RCX != 0 (i.e., if index >= size)
        
        // OPTIMIZED: Copy index to user variable
        gen.emit_mov_reg_mem(0, index_offset); // RAX = current index
        gen.emit_mov_mem_reg(user_index_offset, 0); // Store in user index variable
        
        // OPTIMIZED: Get the value at current index using fastest possible method
        gen.emit_mov_reg_mem(7, iterable_offset); // RDI = array pointer
        gen.emit_mov_reg_mem(6, index_offset); // RSI = index
        
        // ULTRA-FAST OPTIMIZATION: Check if we know the array element type
        // For explicitly typed arrays, we can use direct memory access
        if (auto typed_array = dynamic_cast<TypedArrayLiteral*>(iterable.get())) {
            // MAXIMUM PERFORMANCE: Direct typed array access
            switch (typed_array->array_type) {
                case DataType::INT32:
                    gen.emit_call("__typed_array_get_int32_fast");
                    break;
                case DataType::INT64:
                    gen.emit_call("__typed_array_get_int64_fast");
                    break;
                case DataType::FLOAT32:
                    gen.emit_call("__typed_array_get_float32_fast");
                    break;
                case DataType::FLOAT64:
                // FLOAT64 is the standard numeric type (equivalent to JavaScript's number)
                    gen.emit_call("__typed_array_get_float64_fast");
                    break;
                default:
                    gen.emit_call("__array_get"); // Fallback to general case
                    break;
            }
        } else {
            // General case for dynamic arrays
            gen.emit_call("__array_get"); // RAX = array[index]
        }
        gen.emit_mov_mem_reg(user_value_offset, 0); // Store value in user variable
        
    } else {
        // For objects: SIMPLIFIED IMPLEMENTATION
        // Since object iteration is complex and __object_iterate doesn't exist,
        // implement a basic version that works for object literals
        
        // Check if we've exceeded the reasonable property limit (simpler logic)
        gen.emit_mov_reg_mem(0, index_offset); // RAX = current index
        gen.emit_mov_reg_imm(1, 3); // RCX = max properties for basic object literal
        gen.emit_compare(0, 1); // Compare index with max properties
        
        // Direct jump if index >= max_properties (much simpler)
        gen.emit_setge(0); // AL = 1 if index >= max_properties, 0 otherwise
        gen.emit_and_reg_imm(0, 0xFF); // Zero out upper bits, keep AL
        gen.emit_mov_reg_imm(1, 0); // RCX = 0
        gen.emit_compare(0, 1); // Compare AL with 0
        gen.emit_jump_if_not_zero(loop_end); // Jump if AL != 0 (i.e., if index >= max_properties)
        
        // Get the property name for the current index
        gen.emit_mov_reg_mem(7, iterable_offset); // RDI = object_address
        gen.emit_mov_reg_mem(6, index_offset); // RSI = property_index
        gen.emit_call("__object_get_property_name"); // RAX = property name (const char*)
        
        // Create a UltraScript string from the property name
        gen.emit_mov_reg_reg(7, 0); // RDI = property name
        gen.emit_call("__string_intern"); // RAX = UltraScript string
        gen.emit_mov_mem_reg(user_index_offset, 0); // Store property name string in key variable
        
        // Get the value at current property index
        gen.emit_mov_reg_mem(7, iterable_offset); // RDI = object_address
        gen.emit_mov_reg_mem(6, index_offset); // RSI = property_index
        gen.emit_call("__object_get_property"); // RAX = property value (which should be a string pointer)
        gen.emit_mov_mem_reg(user_value_offset, 0); // Store value in user variable
    }
    
    gen.emit_label(loop_start);
    
    // Generate loop body - user variables are now populated
    for (const auto& stmt : body) {
        stmt->generate_code(gen, types);
    }
    
    // Increment internal index counter
    gen.emit_mov_reg_mem(0, index_offset); // RAX = current internal index
    gen.emit_add_reg_imm(0, 1); // RAX++
    gen.emit_mov_mem_reg(index_offset, 0); // Store incremented internal index
    
    // Jump back to condition check
    gen.emit_jump(loop_check);
    
    gen.emit_label(loop_end);
}

void ForInStatement::generate_code(CodeGenerator& gen, TypeInference& types) {
    static int loop_counter = 0;
    std::string loop_start = "forin_loop_" + std::to_string(loop_counter);
    std::string loop_end = "forin_end_" + std::to_string(loop_counter);
    loop_counter++;
    
    // Generate code for the object expression
    object->generate_code(gen, types);
    
    // Store the object in a temporary location
    int64_t object_offset = types.allocate_variable("__temp_object_" + std::to_string(loop_counter - 1), object->result_type);
    gen.emit_mov_mem_reg(object_offset, 0); // Store object pointer
    
    // Create user-visible variable for the key
    int64_t user_key_offset = types.allocate_variable(key_var_name, DataType::STRING);
    
    // Initialize property index to 0
    int64_t index_offset = types.allocate_variable("__index_" + std::to_string(loop_counter - 1), DataType::INT64);
    gen.emit_mov_reg_imm(0, 0); // RAX = 0
    gen.emit_mov_mem_reg(index_offset, 0); // Store index = 0
    
    // Loop start
    gen.emit_label(loop_start);
    
    // Get field name for current index
    gen.emit_mov_reg_mem(7, object_offset); // RDI = object pointer
    gen.emit_mov_reg_mem(6, index_offset); // RSI = current index
    gen.emit_call("__get_class_property_name"); // Returns const char* or nullptr
    
    // Check if we got a valid field name (if null, we're done)
    gen.emit_mov_reg_imm(1, 0); // RCX = 0 (null)
    gen.emit_compare(0, 1); // Compare field name with null
    gen.emit_jump_if_zero(loop_end); // Jump to end if no more fields
    
    // Convert C string to GoTSString and store in key variable
    gen.emit_mov_reg_reg(7, 0); // RDI = field name (const char*)
    gen.emit_call("__string_intern"); // Convert to GoTSString
    gen.emit_mov_mem_reg(user_key_offset, 0); // Store in key variable
    
    // Generate loop body
    for (const auto& stmt : body) {
        stmt->generate_code(gen, types);
    }
    
    // Increment index
    gen.emit_mov_reg_mem(0, index_offset); // RAX = current index
    gen.emit_add_reg_imm(0, 1); // RAX++
    gen.emit_mov_mem_reg(index_offset, 0); // Store incremented index
    
    // Jump back to loop start
    gen.emit_jump(loop_start);
    
    gen.emit_label(loop_end);
}

void ReturnStatement::generate_code(CodeGenerator& gen, TypeInference& types) {
    if (value) {
        value->generate_code(gen, types);
    }
    
    // CRITICAL: Add automatic cleanup for local variables before function return
    auto* compiler = get_current_compiler();
    if (compiler) {
        compiler->generate_scope_cleanup_code(gen, types);
    }
    
    // Use function return to properly restore stack frame and return
    gen.emit_function_return();
}

// Global variable to track current break target
static std::string current_break_target = "";

void BreakStatement::generate_code(CodeGenerator& gen, TypeInference& types) {
    (void)types; // Suppress unused parameter warning
    
    if (!current_break_target.empty()) {
        gen.emit_jump(current_break_target);
    } else {
        // No active switch/loop context
        // For now, just emit a nop or comment
        gen.emit_label("__break_without_context");
    }
}

void FreeStatement::generate_code(CodeGenerator& gen, TypeInference& types) {
    // Generate code to evaluate the target expression (puts result in RAX)
    target->generate_code(gen, types);
    
    // Get the type of the target expression for optimized code generation
    DataType target_type = target->result_type;
    
    // For ultra-fast performance, we generate different optimized assembly 
    // for each type rather than using runtime type dispatch
    
    if (is_shallow) {
        // SHALLOW FREE - Ultra-optimized type-specific assembly
        switch (target_type) {
            case DataType::STRING: {
                // Direct inline string freeing - faster than function call
                // Test if pointer is null: test rax, rax
                gen.emit_mov_reg_imm(1, 0);   // mov rcx, 0
                gen.emit_compare(0, 1);       // Compare RAX with 0
                std::string skip_free = "skip_string_free_" + std::to_string(rand());
                gen.emit_jump_if_zero(skip_free);
                
                // Free string buffer directly with optimized inline assembly
                // mov rdi, rax      ; string pointer to first argument
                gen.emit_mov_reg_reg(7, 0);  // RDI = RAX (first arg for function call)
                gen.emit_call("__free_string");
                
                gen.emit_label(skip_free);
                break;
            }
            
            case DataType::ARRAY: {
                // Ultra-fast array container freeing (not elements)
                gen.emit_mov_reg_imm(1, 0);   // mov rcx, 0
                gen.emit_compare(0, 1);       // Compare RAX with 0
                std::string skip_free = "skip_array_free_" + std::to_string(rand());
                gen.emit_jump_if_zero(skip_free);
                
                // mov rdi, rax      ; array pointer to first argument
                gen.emit_mov_reg_reg(7, 0);
                gen.emit_call("__free_array_shallow");
                
                gen.emit_label(skip_free);
                break;
            }
            
            case DataType::CLASS_INSTANCE: {
                // Class instance shallow free - only the object structure
                gen.emit_mov_reg_imm(1, 0);   // mov rcx, 0
                gen.emit_compare(0, 1);       // Compare RAX with 0
                std::string skip_free = "skip_class_free_" + std::to_string(rand());
                gen.emit_jump_if_zero(skip_free);
                
                // mov rdi, rax      ; object pointer to first argument
                gen.emit_mov_reg_reg(7, 0);
                gen.emit_call("__free_class_instance_shallow");
                
                gen.emit_label(skip_free);
                break;
            }
            
            case DataType::ANY: {
                // Dynamic type - need runtime type checking but still optimized
                gen.emit_mov_reg_imm(1, 0);   // mov rcx, 0
                gen.emit_compare(0, 1);       // Compare RAX with 0
                std::string skip_free = "skip_dynamic_free_" + std::to_string(rand());
                gen.emit_jump_if_zero(skip_free);
                
                // mov rdi, rax      ; pointer to first argument
                // mov rsi, 1        ; shallow flag (1 = shallow, 0 = deep)
                gen.emit_mov_reg_reg(7, 0);    // RDI = pointer
                gen.emit_mov_reg_imm(6, 1);    // RSI = 1 (shallow)
                gen.emit_call("__free_dynamic_value");
                
                gen.emit_label(skip_free);
                break;
            }
            
            default: {
                // For primitive types (int, float, boolean), freeing is a no-op
                // Generate debug log call in debug mode
                gen.emit_call("__debug_log_primitive_free_ignored");
                break;
            }
        }
    } else {
        // DEEP FREE - Not implemented yet, throw runtime error
        // This should never be reached due to parser validation, but just in case:
        gen.emit_call("__throw_deep_free_not_implemented");
    }
}

void SwitchStatement::generate_code(CodeGenerator& gen, TypeInference& types) {
    static int switch_counter = 0;
    std::string switch_end = "switch_end_" + std::to_string(switch_counter);
    switch_counter++;
    
    // Save previous break target and set new one
    std::string previous_break_target = current_break_target;
    current_break_target = switch_end;
    
    // Generate discriminant code - this puts the result in RAX
    discriminant->generate_code(gen, types);
    DataType discriminant_type = discriminant->result_type;
    
    // Store discriminant value in a temporary location using dynamic stack allocation
    int64_t discriminant_offset = types.allocate_variable("__temp_discriminant_" + std::to_string(switch_counter - 1), discriminant_type);
    int64_t discriminant_type_offset = types.allocate_variable("__temp_discriminant_type_" + std::to_string(switch_counter - 1), DataType::INT64);
    
    gen.emit_mov_mem_reg(discriminant_offset, 0); // Store RAX to discriminant offset
    // Store discriminant type
    gen.emit_mov_reg_imm(0, static_cast<int64_t>(discriminant_type));
    gen.emit_mov_mem_reg(discriminant_type_offset, 0); // Store discriminant type to type offset
    
    // Generate code for each case
    std::vector<std::string> case_labels;
    std::string default_label;
    bool has_default = false;
    
    // First pass: create labels and generate comparison jumps
    for (size_t i = 0; i < cases.size(); i++) {
        const auto& case_clause = cases[i];
        
        if (case_clause->is_default) {
            default_label = "case_default_" + std::to_string(switch_counter - 1);
            has_default = true;
        } else {
            std::string case_label = "case_" + std::to_string(switch_counter - 1) + "_" + std::to_string(i);
            case_labels.push_back(case_label);
            
            // Generate case value and compare with discriminant
            case_clause->value->generate_code(gen, types);
            DataType case_type = case_clause->value->result_type;
            
            // ULTRA HIGH PERFORMANCE: Fast path for typed comparisons, slow path for ANY
            if (discriminant_type != DataType::ANY && discriminant_type != DataType::ANY &&
                case_type != DataType::ANY && case_type != DataType::ANY &&
                discriminant_type == case_type) {
                
                // FAST PATH: Both operands are the same known type - direct comparison
                gen.emit_mov_reg_mem(3, discriminant_offset); // RBX = discriminant value from stack
                gen.emit_compare(3, 0); // Compare discriminant (RBX) with case value (RAX)
                gen.emit_sete(1); // Set RCX = 1 if equal, 0 if not equal
                gen.emit_mov_reg_imm(2, 0); // RDX = 0
                gen.emit_compare(1, 2); // Compare RCX with 0
                gen.emit_jump_if_not_zero(case_label); // Jump if RCX != 0 (i.e., if equal)
                
            } else if (discriminant_type != DataType::ANY && discriminant_type != DataType::ANY &&
                       case_type != DataType::ANY && case_type != DataType::ANY &&
                       discriminant_type != case_type) {
                
                // FAST PATH: Both operands are known types but different - never equal
                // Skip this case entirely (no jump, fall through to next case)
                
            } else {
                
                // SLOW PATH: At least one operand is ANY - use type-aware comparison
                // Prepare arguments for __runtime_js_equal(left_value, left_type, right_value, right_type)
                gen.emit_mov_reg_mem(7, discriminant_offset); // RDI = discriminant value from stack
                gen.emit_mov_reg_mem(6, discriminant_type_offset); // RSI = discriminant type from stack
                gen.emit_mov_reg_reg(2, 0);   // RDX = case value (currently in RAX)
                gen.emit_mov_reg_imm(1, static_cast<int64_t>(case_type)); // RCX = case type
                
                // Call __runtime_js_equal
                gen.emit_sub_reg_imm(4, 8);  // Align stack to 16-byte boundary
                gen.emit_call("__runtime_js_equal");
                gen.emit_add_reg_imm(4, 8);  // Restore stack
                
                // RAX now contains 1 if equal, 0 if not equal
                gen.emit_mov_reg_imm(3, 0); // RBX = 0
                gen.emit_compare(0, 3); // Compare RAX with 0
                gen.emit_jump_if_not_zero(case_label); // Jump if RAX != 0 (i.e., if equal)
            }
        }
    }
    
    // If no case matched, jump to default or end
    if (has_default) {
        gen.emit_jump(default_label);
    } else {
        gen.emit_jump(switch_end);
    }
    
    // Second pass: generate case bodies
    size_t case_index = 0;
    for (size_t i = 0; i < cases.size(); i++) {
        const auto& case_clause = cases[i];
        
        if (case_clause->is_default) {
            gen.emit_label(default_label);
        } else {
            gen.emit_label(case_labels[case_index++]);
        }
        
        // Generate case body
        for (const auto& stmt : case_clause->body) {
            stmt->generate_code(gen, types);
        }
        
        // Fall through to next case (JavaScript/C-style behavior)
        // Break statements will jump to switch_end
    }
    
    gen.emit_label(switch_end);
    
    // Restore previous break target
    current_break_target = previous_break_target;
}

void CaseClause::generate_code(CodeGenerator& gen, TypeInference& types) {
    // CaseClause code generation is handled by SwitchStatement
    // This method should not be called directly
    (void)gen;
    (void)types;
}

// Property access code generation - high-performance direct offset access for declared properties
void PropertyAccess::generate_code(CodeGenerator& gen, TypeInference& types) {
    std::cout << "[DEBUG] PropertyAccess::generate_code - object=" << object_name << ", property=" << property_name << std::endl;
    
    DataType object_type;
    std::string class_name;
    
    // Special handling for 'this' keyword
    if (object_name == "this") {
        // Get the current class context from the type system
        class_name = types.get_current_class_context();
        if (class_name.empty()) {
            throw std::runtime_error("'this' used outside of class method");
        }
        object_type = DataType::CLASS_INSTANCE;
        
        std::cout << "[DEBUG] PropertyAccess: 'this' resolved to class " << class_name << std::endl;
    } else {
        // Get the object's variable type and class name for regular variables
        object_type = types.get_variable_type(object_name);
        uint32_t class_type_id = types.get_variable_class_type_id(object_name);
        
        if (class_type_id != 0) {
            auto* compiler = get_current_compiler();
            if (compiler) {
                class_name = compiler->get_class_name_from_type_id(class_type_id);
            }
        }
        
        if (object_type != DataType::CLASS_INSTANCE || class_name.empty()) {
            throw std::runtime_error("Property access on non-object or unknown class: " + object_name);
        }
    }
    
    // Get the class info to find property offset
    auto* compiler = get_current_compiler();
    if (!compiler) {
        throw std::runtime_error("No compiler context available for property access");
    }
    
    ClassInfo* class_info = compiler->get_class(class_name);
    if (!class_info) {
        throw std::runtime_error("Unknown class: " + class_name);
    }
    
    // Find the property in the class fields
    int64_t property_offset = -1;
    DataType property_type = DataType::ANY;
    for (size_t i = 0; i < class_info->fields.size(); ++i) {
        if (class_info->fields[i].name == property_name) {
            // Object layout: [class_name_ptr][property_count][property0][property1]...
            // Properties start at offset 32 (4 * 8 bytes for metadata)
            property_offset = OBJECT_PROPERTIES_START_OFFSET + (i * 8); // Each property is 8 bytes (pointer or int64)
            property_type = class_info->fields[i].type;
            break;
        }
    }
    
    if (property_offset == -1) {
        // Check for special built-in properties first
        if (property_name == "memoryAddress") {
            std::cout << "[DEBUG] PropertyAccess: Accessing special .memoryAddress property" << std::endl;
            
            // Load the object pointer and return it as the memory address
            if (object_name == "this") {
                // For 'this', the object_address is stored at stack offset -8 by the method prologue
                gen.emit_mov_reg_mem(0, -8); // RAX = object_address (this)
            } else {
                // For regular variables, load from their stack offset
                int64_t object_stack_offset = types.get_variable_offset(object_name);
                gen.emit_mov_reg_mem(0, object_stack_offset); // RAX = object pointer
            }
            
            // RAX now contains the object address, which IS the memory address
            // No additional processing needed since object_address == memory address
            return;
        }
        
        // Property not found in static class fields - check dynamic properties
        std::cout << "[DEBUG] PropertyAccess: Property '" << property_name << "' not found in static fields, using dynamic property lookup" << std::endl;
        
        // Load the object pointer - different handling for 'this' vs regular variables
        if (object_name == "this") {
            // For 'this', the object_address is stored at stack offset -8 by the method prologue
            gen.emit_mov_reg_mem(0, -8); // RAX = object_address (this)
            std::cout << "[DEBUG] PropertyAccess: Loading 'this' from stack offset -8" << std::endl;
        } else {
            // For regular variables, load from their stack offset
            int64_t object_stack_offset = types.get_variable_offset(object_name);
            gen.emit_mov_reg_mem(0, object_stack_offset); // RAX = object pointer
            std::cout << "[DEBUG] PropertyAccess: Loading " << object_name << " from stack offset " << object_stack_offset << std::endl;
        }
        
        // Call dynamic property getter: __dynamic_property_get(object_ptr, property_name)
        gen.emit_mov_reg_reg(7, 0); // RDI = object pointer
        
        // Create property name string storage
        static std::unordered_map<std::string, const char*> property_name_storage;
        auto it = property_name_storage.find(property_name);
        const char* name_ptr;
        if (it != property_name_storage.end()) {
            name_ptr = it->second;
        } else {
            // Allocate permanent storage for this property name
            char* permanent_name = new char[property_name.length() + 1];
            strcpy(permanent_name, property_name.c_str());
            property_name_storage[property_name] = permanent_name;
            name_ptr = permanent_name;
        }
        
        gen.emit_mov_reg_imm(6, reinterpret_cast<int64_t>(name_ptr)); // RSI = property_name
        gen.emit_call("__dynamic_property_get");
        // RAX now contains DynamicValue* or nullptr
        
        result_type = DataType::ANY; // Dynamic properties return ANY type
        std::cout << "[DEBUG] PropertyAccess: Generated dynamic property access for " << class_name << "." << property_name << std::endl;
        return;
    }
    
    std::cout << "[DEBUG] PropertyAccess: Found property at offset " << property_offset << " with type " << static_cast<int>(property_type) << std::endl;
    
    // Load the object pointer - different handling for 'this' vs regular variables
    if (object_name == "this") {
        // For 'this', the object_address is stored at stack offset -8 by the method prologue
        gen.emit_mov_reg_mem(0, -8); // RAX = object_address (this)
        std::cout << "[DEBUG] PropertyAccess: Loading 'this' from stack offset -8" << std::endl;
    } else {
        // For regular variables, load from their stack offset
        int64_t object_stack_offset = types.get_variable_offset(object_name);
        gen.emit_mov_reg_mem(0, object_stack_offset); // RAX = object pointer
        std::cout << "[DEBUG] PropertyAccess: Loading " << object_name << " from stack offset " << object_stack_offset << std::endl;
    }
    
    // LIGHTNING FAST: Direct offset access - zero performance penalty like C++
    gen.emit_mov_reg_reg_offset(0, 0, property_offset); // RAX = [RAX + property_offset]
    
    // Set the result type for further operations
    result_type = property_type;
    
    std::cout << "[DEBUG] PropertyAccess: Generated direct offset access for " << class_name << "." << property_name << std::endl;
}

// Expression property access code generation - supports built-in types and class instances
void ExpressionPropertyAccess::generate_code(CodeGenerator& gen, TypeInference& types) {
    std::cout << "[DEBUG] ExpressionPropertyAccess::generate_code - property=" << property_name << std::endl;
    
    // Generate code for the object expression first
    object->generate_code(gen, types);
    DataType object_type = object->result_type;
    
    // Handle different types of objects for property access
    if (object_type == DataType::CLASS_INSTANCE) {
        // Handle class instance property access with direct offset access
        std::string class_name;
        
        // Check if this is a 'this' expression
        auto* this_expr = dynamic_cast<ThisExpression*>(object.get());
        if (this_expr) {
            // For 'this', get the class name from the current class context
            class_name = types.get_current_class_context();
            if (class_name.empty()) {
                throw std::runtime_error("'this' used outside of class method");
            }
            std::cout << "[DEBUG] ExpressionPropertyAccess: 'this' resolved to class " << class_name << std::endl;
        } else {
            // For other expressions, try to get the class name from variable references
            auto* var_expr = dynamic_cast<Identifier*>(object.get());
            if (!var_expr) {
                throw std::runtime_error("Class property access currently only supports direct variable references and 'this'");
            }
            
            std::string object_name = var_expr->name;
            uint32_t class_type_id = types.get_variable_class_type_id(object_name);
            
            if (class_type_id == 0) {
                throw std::runtime_error("Property access on object with unknown class: " + object_name);
            }
            
            // Get class name from type ID for backward compatibility with existing logic
            auto* compiler = get_current_compiler();
            if (!compiler) {
                throw std::runtime_error("No compiler context available for property access");
            }
            class_name = compiler->get_class_name_from_type_id(class_type_id);
        }
        
        // Get the class info to find property offset
        auto* compiler = get_current_compiler();
        if (!compiler) {
            throw std::runtime_error("No compiler context available for property access");
        }
        
        ClassInfo* class_info = compiler->get_class(class_name);
        if (!class_info) {
            throw std::runtime_error("Unknown class: " + class_name);
        }
        
        // Find the property in the class fields
        int64_t property_offset = -1;
        DataType property_type = DataType::ANY;
        for (size_t i = 0; i < class_info->fields.size(); ++i) {
            if (class_info->fields[i].name == property_name) {
                // Object layout: [class_name_ptr][property_count][property0][property1]...
                // Properties start at offset 32 (4 * 8 bytes for metadata)
                property_offset = OBJECT_PROPERTIES_START_OFFSET + (i * 8); // Each property is 8 bytes (pointer or int64)
                property_type = class_info->fields[i].type;
                break;
            }
        }
        
        if (property_offset == -1) {
            // Check for special built-in properties first
            if (property_name == "memoryAddress") {
                std::cout << "[DEBUG] ExpressionPropertyAccess: Accessing special .memoryAddress property" << std::endl;
                
                // Object pointer is already in RAX from object->generate_code()
                // Just return the object pointer as the memory address
                result_type = DataType::INT64; // Return as integer for proper address handling
                std::cout << "[DEBUG] ExpressionPropertyAccess: Generated memoryAddress access" << std::endl;
                return;
            }
            
            // Property not found in static class fields - check dynamic properties
            std::cout << "[DEBUG] ExpressionPropertyAccess: Property '" << property_name << "' not found in static fields, using dynamic property lookup" << std::endl;
            
            // Object pointer is already in RAX from object->generate_code()
            // Call dynamic property getter: __dynamic_property_get(object_ptr, property_name)
            gen.emit_mov_reg_reg(7, 0); // RDI = object pointer
            
            // Create property name string storage
            static std::unordered_map<std::string, const char*> property_name_storage;
            auto it = property_name_storage.find(property_name);
            const char* name_ptr;
            if (it != property_name_storage.end()) {
                name_ptr = it->second;
            } else {
                // Allocate permanent storage for this property name
                char* permanent_name = new char[property_name.length() + 1];
                strcpy(permanent_name, property_name.c_str());
                property_name_storage[property_name] = permanent_name;
                name_ptr = permanent_name;
            }
            
            gen.emit_mov_reg_imm(6, reinterpret_cast<int64_t>(name_ptr)); // RSI = property_name
            gen.emit_call("__dynamic_property_get");
            // RAX now contains DynamicValue* or nullptr
            
            result_type = DataType::ANY; // Dynamic properties return ANY type
            std::cout << "[DEBUG] ExpressionPropertyAccess: Generated dynamic property access for " << class_name << "." << property_name << std::endl;
            return;
        }
        
        std::cout << "[DEBUG] ExpressionPropertyAccess: Found property at offset " << property_offset << " with type " << static_cast<int>(property_type) << std::endl;
        
        // LIGHTNING FAST: Direct offset access - zero performance penalty like C++
        // Object pointer is already in RAX from object->generate_code()
        gen.emit_mov_reg_reg_offset(0, 0, property_offset); // RAX = [RAX + property_offset]
        
        // Set the result type for further operations
        result_type = property_type;
        
        std::cout << "[DEBUG] ExpressionPropertyAccess: Generated direct offset access for " << class_name << "." << property_name << std::endl;
        
    } else if (object_type == DataType::STRING) {
        // Handle string properties like length
        if (property_name == "length") {
            // String object is now in RAX, get its length
            gen.emit_mov_reg_reg(7, 0);  // RDI = string pointer
            gen.emit_call("__string_length");
            result_type = DataType::FLOAT64;
        } else {
            throw std::runtime_error("Unknown string property: " + property_name);
        }
    } else if (object_type == DataType::TENSOR) {
        // Handle array properties like length, and special match result properties
        if (property_name == "length") {
            gen.emit_mov_reg_reg(7, 0);  // RDI = array pointer
            gen.emit_call("__array_size");
            result_type = DataType::FLOAT64;
        } else if (property_name == "index") {
            // JavaScript match result property - lazily computed
            gen.emit_mov_reg_reg(7, 0);  // RDI = match array pointer
            gen.emit_call("__match_result_get_index");
            result_type = DataType::FLOAT64;
        } else if (property_name == "input") {
            // JavaScript match result property - lazily computed
            gen.emit_mov_reg_reg(7, 0);  // RDI = match array pointer
            gen.emit_call("__match_result_get_input");
            result_type = DataType::STRING;
        } else if (property_name == "groups") {
            // JavaScript match result property - always undefined for basic matches
            gen.emit_mov_reg_reg(7, 0);  // RDI = match array pointer
            gen.emit_call("__match_result_get_groups");
            result_type = DataType::ANY; // undefined
        } else {
            throw std::runtime_error("Unknown array property: " + property_name);
        }
    } else {
        // Property access not supported for this object type
        throw std::runtime_error("Property access only supported for class instances, arrays and strings");
    }
}

void ThisExpression::generate_code(CodeGenerator& gen, TypeInference& types) {
    // Load the object_address from the stack where it was saved in the method prologue
    // Instance methods save the object_address (this) at stack offset -8
    gen.emit_mov_reg_mem(0, -8); // RAX = object_address (this)
}


void NewExpression::generate_code(CodeGenerator& gen, TypeInference& types) {
    // Extract class name from the NewExpression
    std::string class_name = this->class_name;
    
    // MODERN SINGLE SYSTEM: Use the correct object creation function
    auto* compiler = get_current_compiler();
    if (compiler) {
        ClassInfo* class_info = compiler->get_class(class_name);
        if (class_info && class_info->instance_size > 0) {
            // HIGH PERFORMANCE PATH: Use known instance size for direct allocation
            uint32_t instance_size = class_info->instance_size;
            
            // Create a GoTSString for the class name and pass that pointer
            // First, create the class name string using string interning
            static std::unordered_map<std::string, const char*> class_name_storage;
            auto it = class_name_storage.find(class_name);
            const char* str_ptr;
            if (it != class_name_storage.end()) {
                str_ptr = it->second;
            } else {
                // Allocate permanent storage for this class name
                char* permanent_str = new char[class_name.length() + 1];
                strcpy(permanent_str, class_name.c_str());
                class_name_storage[class_name] = permanent_str;
                str_ptr = permanent_str;
            }
            
            uint64_t str_literal_addr = reinterpret_cast<uint64_t>(str_ptr);
            gen.emit_mov_reg_imm(7, static_cast<int64_t>(str_literal_addr)); // RDI = class_name string
            gen.emit_call("__string_intern"); // Create GoTSString
            gen.emit_mov_reg_reg(7, 0); // RDI = GoTSString pointer from RAX
            gen.emit_mov_reg_imm(6, instance_size);   // RSI = instance_size
            gen.emit_call("__jit_object_create_sized");
            
            std::cout << "[JIT] Optimized object creation for " << class_name 
                     << " (size=" << instance_size << ")" << std::endl;
        } else {
            // Fallback for classes without known size - use basic creation
            // Create a GoTSString for the class name
            static std::unordered_map<std::string, const char*> class_name_storage;
            auto it = class_name_storage.find(class_name);
            const char* str_ptr;
            if (it != class_name_storage.end()) {
                str_ptr = it->second;
            } else {
                // Allocate permanent storage for this class name
                char* permanent_str = new char[class_name.length() + 1];
                strcpy(permanent_str, class_name.c_str());
                class_name_storage[class_name] = permanent_str;
                str_ptr = permanent_str;
            }
            
            uint64_t str_literal_addr = reinterpret_cast<uint64_t>(str_ptr);
            gen.emit_mov_reg_imm(7, static_cast<int64_t>(str_literal_addr)); // RDI = class_name string
            gen.emit_call("__string_intern"); // Create GoTSString
            gen.emit_mov_reg_reg(7, 0); // RDI = GoTSString pointer from RAX
            gen.emit_call("__jit_object_create");
            
            std::cout << "[JIT] Basic object creation for " << class_name << std::endl;
        }
    } else {
        // No compiler context - use basic object creation
        // Create a GoTSString for the class name
        static std::unordered_map<std::string, const char*> class_name_storage;
        auto it = class_name_storage.find(class_name);
        const char* str_ptr;
        if (it != class_name_storage.end()) {
            str_ptr = it->second;
        } else {
            // Allocate permanent storage for this class name
            char* permanent_str = new char[class_name.length() + 1];
            strcpy(permanent_str, class_name.c_str());
            class_name_storage[class_name] = permanent_str;
            str_ptr = permanent_str;
        }
        
        uint64_t str_literal_addr = reinterpret_cast<uint64_t>(str_ptr);
        gen.emit_mov_reg_imm(7, static_cast<int64_t>(str_literal_addr)); // RDI = class_name string
        gen.emit_call("__string_intern"); // Create GoTSString
        gen.emit_mov_reg_reg(7, 0); // RDI = GoTSString pointer from RAX
        gen.emit_call("__jit_object_create");
    }
    
    // Object pointer is now in RAX - for now, skip all constructor logic to test basic functionality
    // TODO: Re-enable constructor calls once basic object creation/storage is working
    
    result_type = DataType::CLASS_INSTANCE;
}

void ConstructorDecl::generate_code(CodeGenerator& gen, TypeInference& types) {
    // Reset type inference for new constructor to avoid offset conflicts
    types.reset_for_function();
    
    // Set the current class context for 'this' handling in constructors
    types.set_current_class_context(class_name);
    
    // Generate constructor as a function with 'this' (object_address) as first parameter, then constructor parameters
    std::string constructor_label = "__constructor_" + class_name;
    
    gen.emit_label(constructor_label);
    
    // Calculate estimated stack size for constructor
    int64_t estimated_stack_size = ((parameters.size() + 1) * 8) + (body.size() * 16) + 64; // +1 for 'this' parameter
    if (estimated_stack_size < 80) estimated_stack_size = 80;
    if (estimated_stack_size % 16 != 0) {
        estimated_stack_size += 16 - (estimated_stack_size % 16);
    }
    
    gen.set_function_stack_size(estimated_stack_size);
    
    gen.emit_prologue();
    
    // Set up 'this' parameter (object_address) in first stack slot
    types.set_variable_type("this", DataType::CLASS_INSTANCE);
    types.set_variable_offset("this", -8);
    gen.emit_mov_mem_reg(-8, 7); // save RDI (object_address) to 'this'
    
    // Set up constructor parameters from registers to stack (starting from second parameter)
    for (size_t i = 0; i < parameters.size() && i < 5; i++) { // Max 5 params (RDI is used for 'this')
        const auto& param = parameters[i];
        types.set_variable_type(param.name, param.type);
        
        int stack_offset = -(int)(i + 2) * 8;  // Start at -16, -24, -32 etc ('this' is at -8)
        types.set_variable_offset(param.name, stack_offset);
        
        switch (i) {
            case 0: gen.emit_mov_mem_reg(stack_offset, 6); break;  // save RSI
            case 1: gen.emit_mov_mem_reg(stack_offset, 2); break;  // save RDX
            case 2: gen.emit_mov_mem_reg(stack_offset, 1); break;  // save RCX
            case 3: gen.emit_mov_mem_reg(stack_offset, 8); break;  // save R8
            case 4: gen.emit_mov_mem_reg(stack_offset, 9); break;  // save R9
        }
    }
    
    // Initialize fields with default values
    if (current_compiler_context) {
        ClassInfo* class_info = current_compiler_context->get_class(class_name);
        if (class_info) {
            for (size_t i = 0; i < class_info->fields.size(); i++) {
                const auto& field = class_info->fields[i];
                if (field.default_value) {
                    // Set the property assignment context for proper type conversion
                    types.set_current_property_assignment_type(field.type);
                    
                    // Generate code for default value expression
                    field.default_value->generate_code(gen, types);
                    
                    // Clear the property assignment context
                    types.clear_property_assignment_context();
                    
                    // Set the property on 'this' object using direct offset access
                    // RAX contains the result of the default value expression
                    // Object layout: [class_name_ptr][property_count][property0][property1]...
                    // Properties start at offset 32 (4 * 8 bytes for metadata)
                    int64_t property_offset = OBJECT_PROPERTIES_START_OFFSET + (i * 8); // Each property is 8 bytes
                    
                    // Direct offset assignment - same as ExpressionPropertyAssignment
                    gen.emit_mov_reg_mem(2, -8); // RDX = object_address (from 'this' at [rbp-8])
                    gen.emit_mov_reg_offset_reg(2, property_offset, 0); // [RDX + property_offset] = RAX
                }
            }
        }
    }
    
    // Clear default_value shared_ptrs after constructor generation to prevent double-free during cleanup
    if (current_compiler_context) {
        ClassInfo* class_info = current_compiler_context->get_class(class_name);
        if (class_info) {
            for (auto& field : class_info->fields) {
                field.default_value.reset(); // Clear the shared_ptr to prevent double-free
            }
        }
    }
    
    // Generate constructor body
    for (const auto& stmt : body) {
        stmt->generate_code(gen, types);
    }
    
    gen.emit_epilogue();
    
}

void MethodDecl::generate_code(CodeGenerator& gen, TypeInference& types) {
    // Reset type inference for new method to avoid offset conflicts
    types.reset_for_function();
    
    // Set the current class context for 'this' handling
    types.set_current_class_context(class_name);
    
    // Generate different labels and parameter handling for static vs instance methods
    std::string method_label = is_static ? "__static_" + name : "__method_" + name + "_" + class_name;
    
    gen.emit_label(method_label);
    
    // Calculate estimated stack size for method
    int64_t estimated_stack_size = (parameters.size() * 8) + (body.size() * 16) + 64;
    if (estimated_stack_size < 80) estimated_stack_size = 80;
    if (estimated_stack_size % 16 != 0) {
        estimated_stack_size += 16 - (estimated_stack_size % 16);
    }
    
    gen.set_function_stack_size(estimated_stack_size);
    
    gen.emit_prologue();
    
    if (!is_static) {
        // Instance method: first parameter (RDI) is the object_address (this)
        types.set_variable_offset("__this_object_address", -8);
        gen.emit_mov_mem_reg(-8, 7); // Save object_address from RDI
        
        // Set up other parameters starting from -16
        for (size_t i = 0; i < parameters.size() && i < 5; i++) { // 5 because RDI is used for this
            const auto& param = parameters[i];
            types.set_variable_type(param.name, param.type);
            int stack_offset = -(int)(i + 2) * 8;  // Start at -16 (after this)
            types.set_variable_offset(param.name, stack_offset);
            
            switch (i) {
                case 0: gen.emit_mov_mem_reg(stack_offset, 6); break;  // save RSI  
                case 1: gen.emit_mov_mem_reg(stack_offset, 2); break;  // save RDX
                case 2: gen.emit_mov_mem_reg(stack_offset, 1); break;  // save RCX
                case 3: gen.emit_mov_mem_reg(stack_offset, 8); break;  // save R8
                case 4: gen.emit_mov_mem_reg(stack_offset, 9); break;  // save R9
            }
        }
    } else {
        // Static method: no 'this' parameter, parameters start from -8
        for (size_t i = 0; i < parameters.size() && i < 6; i++) { // 6 registers available for static methods
            const auto& param = parameters[i];
            types.set_variable_type(param.name, param.type);
            int stack_offset = -(int)(i + 1) * 8;  // Start at -8
            types.set_variable_offset(param.name, stack_offset);
            
            switch (i) {
                case 0: gen.emit_mov_mem_reg(stack_offset, 7); break;  // save RDI
                case 1: gen.emit_mov_mem_reg(stack_offset, 6); break;  // save RSI
                case 2: gen.emit_mov_mem_reg(stack_offset, 2); break;  // save RDX
                case 3: gen.emit_mov_mem_reg(stack_offset, 1); break;  // save RCX
                case 4: gen.emit_mov_mem_reg(stack_offset, 8); break;  // save R8
                case 5: gen.emit_mov_mem_reg(stack_offset, 9); break;  // save R9
            }
        }
    }
    
    // Generate method body
    bool has_explicit_return = false;
    for (const auto& stmt : body) {
        stmt->generate_code(gen, types);
        if (dynamic_cast<const ReturnStatement*>(stmt.get())) {
            has_explicit_return = true;
        }
    }
    
    // If no explicit return, return 0 for non-void methods
    if (!has_explicit_return && return_type != DataType::VOID) {
        gen.emit_mov_reg_imm(0, 0);
    }
    
    gen.emit_function_return();
}

// Property assignment code generation - supports both static and dynamic properties
void PropertyAssignment::generate_code(CodeGenerator& gen, TypeInference& types) {
    std::cout << "[DEBUG] PropertyAssignment::generate_code - object=" << object_name << ", property=" << property_name << std::endl;
    
    DataType object_type;
    std::string class_name;
    
    // Special handling for 'this' keyword
    if (object_name == "this") {
        // Get the current class context from the type system
        class_name = types.get_current_class_context();
        if (class_name.empty()) {
            throw std::runtime_error("'this' used outside of class method");
        }
        object_type = DataType::CLASS_INSTANCE;
        
        std::cout << "[DEBUG] PropertyAssignment: 'this' resolved to class " << class_name << std::endl;
    } else {
        // Get the object's variable type and class name for regular variables
        object_type = types.get_variable_type(object_name);
        uint32_t class_type_id = types.get_variable_class_type_id(object_name);
        
        if (class_type_id != 0) {
            auto* compiler = get_current_compiler();
            if (compiler) {
                class_name = compiler->get_class_name_from_type_id(class_type_id);
            }
        }
        
        if (object_type != DataType::CLASS_INSTANCE || class_name.empty()) {
            throw std::runtime_error("Property assignment on non-object or unknown class: " + object_name);
        }
    }
    
    // Get the class info to find property offset
    auto* compiler = get_current_compiler();
    if (!compiler) {
        throw std::runtime_error("No compiler context available for property assignment");
    }
    
    ClassInfo* class_info = compiler->get_class(class_name);
    if (!class_info) {
        throw std::runtime_error("Unknown class: " + class_name);
    }
    
    // Find the property in the class fields
    int64_t property_offset = -1;
    DataType property_type = DataType::ANY;
    for (size_t i = 0; i < class_info->fields.size(); ++i) {
        if (class_info->fields[i].name == property_name) {
            // Object layout: [class_name_ptr][property_count][dynamic_map_ptr][property0][property1]...
            // Properties start at offset 32 (4 * 8 bytes for metadata)
            property_offset = OBJECT_PROPERTIES_START_OFFSET + (i * 8); // Each property is 8 bytes (pointer or int64)
            property_type = class_info->fields[i].type;
            break;
        }
    }
    
    if (property_offset == -1) {
        // Property not found in static class fields - create dynamic property
        std::cout << "[DEBUG] PropertyAssignment: Property '" << property_name << "' not found in static fields, using dynamic property assignment" << std::endl;
        
        // Load the object pointer
        if (object_name == "this") {
            // For 'this', the object_address is stored at stack offset -8 by the method prologue
            gen.emit_mov_reg_mem(0, -8); // RAX = object_address (this)
        } else {
            // For regular variables, load from their stack offset
            int64_t object_stack_offset = types.get_variable_offset(object_name);
            gen.emit_mov_reg_mem(0, object_stack_offset); // RAX = object pointer
        }
        
        // Save the object pointer
        gen.emit_mov_mem_reg(-56, 0); // Save object pointer to [rbp-56]
        
        // Generate code for the value expression
        value->generate_code(gen, types);
        // Value is now in RAX
        
        // Convert the value to a DynamicValue
        DataType value_type = value->result_type;
        gen.emit_mov_reg_reg(7, 0); // RDI = value
        gen.emit_mov_reg_imm(6, static_cast<int>(value_type)); // RSI = type_id
        gen.emit_call("__dynamic_value_create_any");
        // RAX now contains DynamicValue*
        
        // Save DynamicValue pointer
        gen.emit_mov_mem_reg(-64, 0); // Save DynamicValue* to [rbp-64]
        
        // Load object pointer and call dynamic property setter
        gen.emit_mov_reg_mem(7, -56); // RDI = object pointer
        
        // Create property name string storage
        static std::unordered_map<std::string, const char*> property_name_storage;
        auto it = property_name_storage.find(property_name);
        const char* name_ptr;
        if (it != property_name_storage.end()) {
            name_ptr = it->second;
        } else {
            // Allocate permanent storage for this property name
            char* permanent_name = new char[property_name.length() + 1];
            strcpy(permanent_name, property_name.c_str());
            property_name_storage[property_name] = permanent_name;
            name_ptr = permanent_name;
        }
        
        gen.emit_mov_reg_imm(6, reinterpret_cast<int64_t>(name_ptr)); // RSI = property_name
        gen.emit_mov_reg_mem(2, -64); // RDX = DynamicValue*
        gen.emit_call("__dynamic_property_set");
        
        std::cout << "[DEBUG] PropertyAssignment: Generated dynamic property assignment for " << class_name << "." << property_name << std::endl;
        return;
    }
    
    std::cout << "[DEBUG] PropertyAssignment: Found property at offset " << property_offset << " with type " << static_cast<int>(property_type) << std::endl;
    
    // Set the property assignment context for proper type conversion
    types.set_current_property_assignment_type(property_type);
    
    // Generate code for the value expression
    value->generate_code(gen, types);
    // Value is now in RAX
    
    // Clear the property assignment context
    types.clear_property_assignment_context();
    
    // Handle reference counting for CLASS_INSTANCE property assignments
    if (property_type == DataType::CLASS_INSTANCE && value->result_type == DataType::CLASS_INSTANCE) {
        // Load the object pointer to check for existing property value
        if (object_name == "this") {
            gen.emit_mov_reg_mem(2, -8); // RDX = object_address (this)
        } else {
            int64_t object_stack_offset = types.get_variable_offset(object_name);
            gen.emit_mov_reg_mem(2, object_stack_offset); // RDX = object pointer
        }
        
        // Load old property value and decrement its reference count if not null
        gen.emit_mov_reg_reg_offset(1, 2, property_offset); // RBX = old property value at [RDX + property_offset]
        gen.emit_mov_reg_imm(3, 0); // R8 = 0
        gen.emit_compare(1, 3); // Compare RBX with 0
        std::string skip_old_prop_release = "skip_old_prop_release_" + std::to_string(rand());
        gen.emit_jump_if_zero(skip_old_prop_release); // Skip if null
        
        // Decrement reference count of old property value (RBX = old object)
        gen.emit_ref_count_decrement(1, 3); // RBX = old object, RDX = result (unused)
        
        gen.emit_label(skip_old_prop_release);
        
        // Increment reference count for new property value if not null
        gen.emit_mov_reg_imm(1, 0); // RCX = 0
        gen.emit_compare(0, 1); // Compare RAX with 0
        std::string skip_new_prop_inc = "skip_new_prop_inc_" + std::to_string(rand());
        gen.emit_jump_if_zero(skip_new_prop_inc); // Skip if null
        
        // Increment reference count for the new object (RAX = object)
        gen.emit_ref_count_increment(0); // RAX = new object pointer
        
        gen.emit_label(skip_new_prop_inc);
    }
    
    // Load the object pointer and store the value
    if (object_name == "this") {
        // For 'this', the object_address is stored at stack offset -8 by the method prologue
        gen.emit_mov_reg_mem(2, -8); // RDX = object_address (this)
    } else {
        // For regular variables, load from their stack offset
        int64_t object_stack_offset = types.get_variable_offset(object_name);
        gen.emit_mov_reg_mem(2, object_stack_offset); // RDX = object pointer
    }
    
    // LIGHTNING FAST: Direct offset assignment - zero performance penalty like C++
    gen.emit_mov_reg_offset_reg(2, property_offset, 0); // [RDX + property_offset] = RAX
    
    std::cout << "[DEBUG] PropertyAssignment: Generated direct offset assignment for " << class_name << "." << property_name << std::endl;
}

// Expression property assignment code generation - high-performance direct offset access for declared properties
void ExpressionPropertyAssignment::generate_code(CodeGenerator& gen, TypeInference& types) {
    std::cout << "[DEBUG] ExpressionPropertyAssignment::generate_code - property=" << property_name << std::endl;
    
    // Generate code for the object expression first
    object->generate_code(gen, types);
    DataType object_type = object->result_type;
    
    // For now, only support direct variable references as objects
    // TODO: Later extend to support full expression chains
    auto* var_expr = dynamic_cast<Identifier*>(object.get());
    if (!var_expr) {
        throw std::runtime_error("Property assignment currently only supports direct variable references");
    }
    
    std::string object_name = var_expr->name;
    uint32_t class_type_id = types.get_variable_class_type_id(object_name);
    
    std::string class_name;
    if (class_type_id != 0) {
        auto* compiler = get_current_compiler();
        if (compiler) {
            class_name = compiler->get_class_name_from_type_id(class_type_id);
        }
    }
    
    if (object_type != DataType::CLASS_INSTANCE || class_name.empty()) {
        throw std::runtime_error("Property assignment on non-object or unknown class: " + object_name);
    }
    
    // Get the class info to find property offset
    auto* compiler = get_current_compiler();
    if (!compiler) {
        throw std::runtime_error("No compiler context available for property assignment");
    }
    
    ClassInfo* class_info = compiler->get_class(class_name);
    if (!class_info) {
        throw std::runtime_error("Unknown class: " + class_name);
    }
    
    // Find the property in the class fields
    int64_t property_offset = -1;
    DataType property_type = DataType::ANY;
    for (size_t i = 0; i < class_info->fields.size(); ++i) {
        if (class_info->fields[i].name == property_name) {
            // Object layout: [class_name_ptr][property_count][property0][property1]...
            // Properties start at offset 32 (4 * 8 bytes for metadata)
            property_offset = OBJECT_PROPERTIES_START_OFFSET + (i * 8); // Each property is 8 bytes (pointer or int64)
            property_type = class_info->fields[i].type;
            break;
        }
    }
    
    if (property_offset == -1) {
        // Property not found in static class fields - create dynamic property
        std::cout << "[DEBUG] ExpressionPropertyAssignment: Property '" << property_name << "' not found in static fields, using dynamic property assignment" << std::endl;
        
        // Save the object pointer (currently in RAX from object->generate_code)
        gen.emit_mov_mem_reg(-56, 0); // Save object pointer to [rbp-56]
        
        // Generate code for the value expression
        value->generate_code(gen, types);
        // Value is now in RAX
        
        // Convert the value to a DynamicValue
        DataType value_type = value->result_type;
        gen.emit_mov_reg_reg(7, 0); // RDI = value
        gen.emit_mov_reg_imm(6, static_cast<int>(value_type)); // RSI = type_id
        gen.emit_call("__dynamic_value_create_any");
        // RAX now contains DynamicValue*
        
        // Save DynamicValue pointer
        gen.emit_mov_mem_reg(-64, 0); // Save DynamicValue* to [rbp-64]
        
        // Load object pointer and call dynamic property setter
        gen.emit_mov_reg_mem(7, -56); // RDI = object pointer
        
        // Create property name string storage
        static std::unordered_map<std::string, const char*> property_name_storage;
        auto it = property_name_storage.find(property_name);
        const char* name_ptr;
        if (it != property_name_storage.end()) {
            name_ptr = it->second;
        } else {
            // Allocate permanent storage for this property name
            char* permanent_name = new char[property_name.length() + 1];
            strcpy(permanent_name, property_name.c_str());
            property_name_storage[property_name] = permanent_name;
            name_ptr = permanent_name;
        }
        
        gen.emit_mov_reg_imm(6, reinterpret_cast<int64_t>(name_ptr)); // RSI = property_name
        gen.emit_mov_reg_mem(2, -64); // RDX = DynamicValue*
        gen.emit_call("__dynamic_property_set");
        
        std::cout << "[DEBUG] ExpressionPropertyAssignment: Generated dynamic property assignment for " << class_name << "." << property_name << std::endl;
        return;
    }
    
    std::cout << "[DEBUG] ExpressionPropertyAssignment: Found property at offset " << property_offset << " with type " << static_cast<int>(property_type) << std::endl;
    
    // Save the object pointer (currently in RAX from object->generate_code)
    std::cout << "[DEBUG] ExpressionPropertyAssignment: Saving object pointer from RAX to stack" << std::endl;
    std::cout.flush();
    // Save object pointer to stack instead of RDX (which might be clobbered by function calls)
    gen.emit_mov_mem_reg(-56, 0); // Save object pointer to [rbp-56]
    
    // Set the property assignment context for proper type conversion
    types.set_current_property_assignment_type(property_type);
    
    // Generate code for the value expression
    value->generate_code(gen, types);
    // Value is now in RAX
    
    // Clear the property assignment context
    types.clear_property_assignment_context();
    
    // Handle reference counting for CLASS_INSTANCE property assignments
    if (property_type == DataType::CLASS_INSTANCE && value->result_type == DataType::CLASS_INSTANCE) {
        // Load object pointer from stack
        gen.emit_mov_reg_mem(2, -56); // RDX = object pointer from [rbp-56]
        
        // Load old property value and decrement its reference count if not null
        gen.emit_mov_reg_reg_offset(1, 2, property_offset); // RBX = old property value at [RDX + property_offset]
        gen.emit_mov_reg_imm(3, 0); // R8 = 0
        gen.emit_compare(1, 3); // Compare RBX with 0
        std::string skip_old_expr_prop_release = "skip_old_expr_prop_release_" + std::to_string(rand());
        gen.emit_jump_if_zero(skip_old_expr_prop_release); // Skip if null
        
        // Decrement reference count of old property value (RBX = old object)
        gen.emit_ref_count_decrement(1, 3); // RBX = old object, RDX = result (unused)
        
        gen.emit_label(skip_old_expr_prop_release);
        
        // Increment reference count for new property value if not null
        gen.emit_mov_reg_imm(1, 0); // RCX = 0
        gen.emit_compare(0, 1); // Compare RAX with 0
        std::string skip_new_expr_prop_inc = "skip_new_expr_prop_inc_" + std::to_string(rand());
        gen.emit_jump_if_zero(skip_new_expr_prop_inc); // Skip if null
        
        // Increment reference count for the new object (RAX = object)
        gen.emit_ref_count_increment(0); // RAX = new object pointer
        
        gen.emit_label(skip_new_expr_prop_inc);
    }
    
    // LIGHTNING FAST: Direct offset assignment - zero performance penalty like C++
    std::cout << "[DEBUG] ExpressionPropertyAssignment: Loading object pointer from stack and storing value" << std::endl;
    std::cout.flush();
    // Load object pointer from stack to RDX, then store value
    gen.emit_mov_reg_mem(2, -56); // RDX = object pointer from [rbp-56]
    gen.emit_mov_reg_offset_reg(2, property_offset, 0); // [RDX + property_offset] = RAX
    std::cout << "[DEBUG] ExpressionPropertyAssignment: Direct offset assignment completed" << std::endl;
    std::cout.flush();
    
    // Result of assignment is the assigned value (already in RAX)
    result_type = value->result_type;
    
    std::cout << "[DEBUG] ExpressionPropertyAssignment: Generated direct offset assignment for " << class_name << "." << property_name << std::endl;
}

void ClassDecl::generate_code(CodeGenerator& gen, TypeInference& types) {
    // Class declarations don't generate code during main execution
    // Constructor and methods are generated separately in the function generation phase
    
    // No code generation needed here - everything is handled in the function phase
}

void SuperCall::generate_code(CodeGenerator& gen, TypeInference& types) {
    // Super constructor call: calls the parent class constructor
    // TODO: Need to determine the parent class name from context
    // For now, generate a call that will need to be resolved at runtime
    
    // Get the object_address from 'this' parameter (should be available in constructor context)
    gen.emit_mov_reg_mem(7, -8); // RDI = object_address (this)
    
    // Set up constructor arguments
    for (size_t i = 0; i < arguments.size() && i < 5; i++) {
        arguments[i]->generate_code(gen, types);
        
        // Store argument value in temporary stack location
        gen.emit_mov_mem_reg(-(int64_t)(i + 2) * 8, 0); // Store at -16, -24, etc.
    }
    
    // Load arguments into appropriate registers
    for (size_t i = 0; i < arguments.size() && i < 5; i++) {
        switch (i) {
            case 0: gen.emit_mov_reg_mem(6, -16); break; // RSI
            case 1: gen.emit_mov_reg_mem(2, -24); break; // RDX  
            case 2: gen.emit_mov_reg_mem(1, -32); break; // RCX
            case 3: gen.emit_mov_reg_mem(8, -40); break; // R8
            case 4: gen.emit_mov_reg_mem(9, -48); break; // R9
        }
    }
    
    // Call a runtime function to resolve and call the parent constructor
    // This will need to be implemented to look up the parent class constructor
    gen.emit_call("__super_constructor_call");
    
    
    result_type = DataType::VOID;
}

void SuperMethodCall::generate_code(CodeGenerator& gen, TypeInference& types) {
    // Super method call: calls the parent class method
    // TODO: This needs to be enhanced to dynamically resolve the parent class method
    // For now, generate a call that assumes parent method naming convention
    
    // Get the object_address from 'this' parameter (should be available in method context)
    gen.emit_mov_reg_mem(7, -8); // RDI = object_address (this)
    
    // Set up method arguments
    for (size_t i = 0; i < arguments.size() && i < 5; i++) {
        arguments[i]->generate_code(gen, types);
        
        // Store argument value in temporary stack location
        gen.emit_mov_mem_reg(-(int64_t)(i + 2) * 8, 0); // Store at -16, -24, etc.
    }
    
    // Load arguments into appropriate registers
    for (size_t i = 0; i < arguments.size() && i < 5; i++) {
        switch (i) {
            case 0: gen.emit_mov_reg_mem(6, -16); break; // RSI
            case 1: gen.emit_mov_reg_mem(2, -24); break; // RDX  
            case 2: gen.emit_mov_reg_mem(1, -32); break; // RCX
            case 3: gen.emit_mov_reg_mem(8, -40); break; // R8
            case 4: gen.emit_mov_reg_mem(9, -48); break; // R9
        }
    }
    
    // Call the parent method - for now use a simple naming convention
    // TODO: This should be enhanced to dynamically resolve parent class methods
    std::string parent_method_label = "__parent_method_" + method_name;
    gen.emit_call(parent_method_label);
    
    result_type = DataType::ANY; // TODO: Get actual return type from method signature
}

void ImportStatement::generate_code(CodeGenerator& gen, TypeInference& types) {
    // At code generation time, we need to load the module and make its exports available
    
    // Get the compiler context to access the module system
    GoTSCompiler* compiler = ConstructorDecl::current_compiler_context;
    if (!compiler) {
        throw std::runtime_error("No compiler context available for module loading");
    }
    
    try {
        // Load the module using lazy loading with circular import support
        Module* module = compiler->load_module_lazy(module_path);
        if (!module) {
            throw std::runtime_error("Failed to load module: " + module_path);
        }
        
        // Check if module has circular import issues
        if (module->exports_partial) {
            std::cerr << "Warning: Module " << module_path << " has partial exports due to circular imports" << std::endl;
            std::cerr << compiler->get_import_stack_trace() << std::endl;
        }
        
        // Execute the module to populate its exports
        // For now, we'll simulate this by parsing the exports and binding known values
        if (is_namespace_import) {
            // Create a namespace object containing all exports from the module
            // This would require runtime module loading and export collection
            types.set_variable_type(namespace_name, DataType::ANY);
        } else {
            for (const auto& spec : specifiers) {
                
                // Look for the exported value in the module's AST
                for (const auto& stmt : module->ast) {
                    if (auto export_stmt = dynamic_cast<ExportStatement*>(stmt.get())) {
                        
                        // Check if this export statement has a declaration (like "export const bobby = 'hello'")
                        if (export_stmt->declaration) {
                            
                            // Check if this is an Assignment with a number literal value
                            if (auto assignment = dynamic_cast<Assignment*>(export_stmt->declaration.get())) {
                                
                                if (assignment->variable_name == spec.local_name) {
                                    
                                    // Check if the value is a number literal
                                    if (auto number_literal = dynamic_cast<NumberLiteral*>(assignment->value.get())) {
                                        // Store the constant value globally instead of using stack
                                        global_imported_constants[spec.local_name] = number_literal->value;
                                        types.set_variable_type(spec.local_name, DataType::FLOAT64);
                                        break;
                                    } else {
                                    }
                                }
                            } else {
                                
                                // Try to cast to other possible types
                                if (auto func_decl = dynamic_cast<FunctionDecl*>(export_stmt->declaration.get())) {
                                } else {
                                }
                            }
                            
                            // For non-constant exports, use the original stack-based approach
                            // Allocate stack space for the imported variable
                            int64_t offset = types.allocate_variable(spec.local_name, DataType::STRING);
                            
                            // Generate code for the export declaration
                            export_stmt->declaration->generate_code(gen, types);
                            
                            // Store the result in the imported variable's location
                            gen.emit_mov_mem_reg(offset, 0);
                            
                            break;
                        }
                    }
                }
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Error loading module " << module_path << ": " << e.what() << std::endl;
        // Fall back to registering as unknown type
        for (const auto& spec : specifiers) {
            types.set_variable_type(spec.local_name, DataType::ANY);
        }
    }
}

void ExportStatement::generate_code(CodeGenerator& gen, TypeInference& types) {
    
    if (is_default) {
        if (declaration) {
            // Generate code for the default export declaration
            declaration->generate_code(gen, types);
            // The result should be stored as the default export value
        }
    } else if (!specifiers.empty()) {
        for (const auto& spec : specifiers) {
            std::cout << "  " << spec.local_name << " as " << spec.exported_name << std::endl;
        }
        // Named exports just mark existing variables/functions as exported
        // The actual export registration happens in the module system
    } else if (declaration) {
        // Generate code for the exported declaration
        declaration->generate_code(gen, types);
        // Mark the declared item as exported
    }
}

void OperatorOverloadDecl::generate_code(CodeGenerator& gen, TypeInference& types) {
    // Generate operator overload function with unique name based on parameter types
    std::string param_signature = "";
    for (size_t i = 0; i < parameters.size(); ++i) {
        if (i > 0) param_signature += "_";
        if (parameters[i].type == DataType::ANY) {
            param_signature += "any";
        } else {
            param_signature += std::to_string(static_cast<int>(parameters[i].type));
        }
    }
    
    std::string op_function_name = class_name + "::__op_" + std::to_string(static_cast<int>(operator_type)) + "_" + param_signature + "__";
    
    // Generate function label
    gen.emit_label(op_function_name);
    gen.emit_prologue();
    
    // Reset type inference for new function
    types.reset_for_function();
    
    // Set up parameter types and save parameters from registers to stack
    for (size_t i = 0; i < parameters.size() && i < 6; i++) {
        const auto& param = parameters[i];
        types.set_variable_type(param.name, param.type);
        
        int stack_offset = -(int)(i + 1) * 8;
        types.set_variable_offset(param.name, stack_offset);
        
        // Save parameters from calling convention registers to stack
        switch (i) {
            case 0: gen.emit_mov_mem_reg(stack_offset, 7); break;  // save RDI
            case 1: gen.emit_mov_mem_reg(stack_offset, 6); break;  // save RSI
            case 2: gen.emit_mov_mem_reg(stack_offset, 2); break;  // save RDX
            case 3: gen.emit_mov_mem_reg(stack_offset, 1); break;  // save RCX
            case 4: gen.emit_mov_mem_reg(stack_offset, 8); break;  // save R8
            case 5: gen.emit_mov_mem_reg(stack_offset, 9); break;  // save R9
        }
        
        // Special handling for class instances - set class type ID
        if (param.type == DataType::CLASS_INSTANCE && !param.class_name.empty()) {
            auto* compiler = get_current_compiler();
            if (compiler) {
                uint32_t class_type_id = compiler->get_class_type_id(param.class_name);
                types.set_variable_class_type(param.name, class_type_id);
            }
        }
    }
    
    // Generate function body
    for (const auto& stmt : body) {
        stmt->generate_code(gen, types);
    }
    
    // Ensure there's a return value in RAX
    gen.emit_mov_reg_imm(0, 0); // Default return 0
    
    gen.emit_epilogue();
    
    // Register the operator overload with the compiler
    auto* compiler = get_current_compiler();
    if (compiler) {
        OperatorOverload overload(operator_type, parameters, return_type);
        overload.function_name = op_function_name;
        compiler->register_operator_overload(class_name, overload);
        
        // Verify registration
        bool has_overload = compiler->has_operator_overload(class_name, operator_type);
        (void)has_overload; // Suppress unused variable warning
    }
}

void SliceExpression::generate_code(CodeGenerator& gen, TypeInference& types) {
    // Create a runtime slice object that can be used by array operations
    // The slice object should contain start, end, step, and specification flags
    
    // Load slice parameters into registers
    gen.emit_mov_reg_imm(7, start_specified ? start : 0);                    // RDI = start
    gen.emit_mov_reg_imm(6, end_specified ? end : -1);                       // RSI = end  
    gen.emit_mov_reg_imm(2, step_specified ? step : 1);                      // RDX = step
    gen.emit_mov_reg_imm(1, (start_specified ? 1 : 0) | 
                            (end_specified ? 2 : 0) | 
                            (step_specified ? 4 : 0));                       // RCX = flags
    
    // Call runtime function to create slice object
    gen.emit_call("__slice_create");
    
    // Result (slice pointer) is now in RAX
    result_type = DataType::SLICE;
}