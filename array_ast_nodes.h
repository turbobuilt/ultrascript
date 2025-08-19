#pragma once

#include "ultra_performance_array.h"
#include <string>
#include <vector>
#include <memory>
#include <map>

// Forward declarations
struct ExpressionNode;
class CodeGenerator;
class TypeInference;

// ============================================================================
// AST Nodes for Ultra-Performance Array System
// ============================================================================

// Base class for all array-related AST nodes
struct ArrayExpressionNode : ExpressionNode {
    virtual ~ArrayExpressionNode() = default;
};

// ============================================================================
// TYPED ARRAY AST NODES - Compile-time type known, zero overhead
// ============================================================================

struct TypedArrayLiteral : ArrayExpressionNode {
    DataType element_type_;
    std::vector<std::unique_ptr<ExpressionNode>> elements_;
    
    TypedArrayLiteral(DataType element_type) : element_type_(element_type) {}
    
    void generate_code(CodeGenerator& gen, TypeInference& types) override {
        // Generate ultra-performance type-specific code
        switch (element_type_) {
            case DataType::INT8:
                gen.emit_call("__create_int8_array");
                result_type = DataType::TYPED_ARRAY_INT8;
                break;
            case DataType::INT16:
                gen.emit_call("__create_int16_array");
                result_type = DataType::TYPED_ARRAY_INT16;
                break;
            case DataType::INT32:
                gen.emit_call("__create_int32_array");
                result_type = DataType::TYPED_ARRAY_INT32;
                break;
            case DataType::INT64:
                gen.emit_call("__create_int64_array");
                result_type = DataType::TYPED_ARRAY_INT64;
                break;
            case DataType::UINT8:
                gen.emit_call("__create_uint8_array");
                result_type = DataType::TYPED_ARRAY_UINT8;
                break;
            case DataType::UINT16:
                gen.emit_call("__create_uint16_array");
                result_type = DataType::TYPED_ARRAY_UINT16;
                break;
            case DataType::UINT32:
                gen.emit_call("__create_uint32_array");
                result_type = DataType::TYPED_ARRAY_UINT32;
                break;
            case DataType::UINT64:
                gen.emit_call("__create_uint64_array");
                result_type = DataType::TYPED_ARRAY_UINT64;
                break;
            case DataType::FLOAT32:
                gen.emit_call("__create_float32_array");
                result_type = DataType::TYPED_ARRAY_FLOAT32;
                break;
            case DataType::FLOAT64:
                gen.emit_call("__create_float64_array");
                result_type = DataType::TYPED_ARRAY_FLOAT64;
                break;
            default:
                throw std::runtime_error("Unsupported typed array element type");
        }
        
        // Store array pointer on stack
        gen.emit_mov_mem_reg(-16, 0);
        
        // Push each element with direct, type-specific calls
        for (const auto& element : elements_) {
            element->generate_code(gen, types);
            gen.emit_mov_reg_mem(7, -16);  // RDI = array pointer
            gen.emit_mov_reg_reg(6, 0);    // RSI = element value
            
            // Ultra-fast type-specific push - no runtime type checking!
            switch (element_type_) {
                case DataType::INT8:
                    gen.emit_call("__int8_array_push_direct");
                    break;
                case DataType::INT16:
                    gen.emit_call("__int16_array_push_direct");
                    break;
                case DataType::INT32:
                    gen.emit_call("__int32_array_push_direct");
                    break;
                case DataType::INT64:
                    gen.emit_call("__int64_array_push_direct");
                    break;
                case DataType::UINT8:
                    gen.emit_call("__uint8_array_push_direct");
                    break;
                case DataType::UINT16:
                    gen.emit_call("__uint16_array_push_direct");
                    break;
                case DataType::UINT32:
                    gen.emit_call("__uint32_array_push_direct");
                    break;
                case DataType::UINT64:
                    gen.emit_call("__uint64_array_push_direct");
                    break;
                case DataType::FLOAT32:
                    gen.emit_call("__float32_array_push_direct");
                    break;
                case DataType::FLOAT64:
                    gen.emit_call("__float64_array_push_direct");
                    break;
            }
        }
        
        // Return array pointer
        gen.emit_mov_reg_mem(0, -16);
    }
};

struct TypedArrayFactoryCall : ArrayExpressionNode {
    std::string factory_method_;  // "zeros", "ones", "full", "arange", "linspace"
    DataType element_type_;
    std::vector<std::unique_ptr<ExpressionNode>> shape_args_;
    std::vector<std::unique_ptr<ExpressionNode>> method_args_;
    
    TypedArrayFactoryCall(const std::string& method, DataType element_type) 
        : factory_method_(method), element_type_(element_type) {}
    
    void generate_code(CodeGenerator& gen, TypeInference& types) override {
        // Generate shape array
        gen.emit_mov_reg_imm(7, shape_args_.size());  // Number of dimensions
        gen.emit_call("__create_shape_array");
        
        for (size_t i = 0; i < shape_args_.size(); ++i) {
            shape_args_[i]->generate_code(gen, types);
            gen.emit_mov_reg_imm(7, i);  // Dimension index
            gen.emit_mov_reg_reg(6, 0);  // Dimension size
            gen.emit_call("__shape_array_set");
        }
        
        // Generate method-specific arguments
        for (const auto& arg : method_args_) {
            arg->generate_code(gen, types);
            gen.emit_push_reg(0);
        }
        
        // Call ultra-fast type-specific factory method
        std::string factory_function = "__" + get_type_prefix(element_type_) + "_array_" + factory_method_;
        gen.emit_call(factory_function);
        
        // Set result type for further optimizations
        result_type = get_typed_array_datatype(element_type_);
    }
    
private:
    std::string get_type_prefix(DataType type) {
        switch (type) {
            case DataType::INT8: return "int8";
            case DataType::INT16: return "int16";
            case DataType::INT32: return "int32";
            case DataType::INT64: return "int64";
            case DataType::UINT8: return "uint8";
            case DataType::UINT16: return "uint16";
            case DataType::UINT32: return "uint32";
            case DataType::UINT64: return "uint64";
            case DataType::FLOAT32: return "float32";
            case DataType::FLOAT64: return "float64";
            default: return "unknown";
        }
    }
    
    DataType get_typed_array_datatype(DataType element_type) {
        // Map element type to corresponding typed array type
        switch (element_type) {
            case DataType::INT8: return static_cast<DataType>(100);    // TYPED_ARRAY_INT8
            case DataType::INT16: return static_cast<DataType>(101);   // TYPED_ARRAY_INT16
            case DataType::INT32: return static_cast<DataType>(102);   // TYPED_ARRAY_INT32
            case DataType::INT64: return static_cast<DataType>(103);   // TYPED_ARRAY_INT64
            case DataType::UINT8: return static_cast<DataType>(104);   // TYPED_ARRAY_UINT8
            case DataType::UINT16: return static_cast<DataType>(105);  // TYPED_ARRAY_UINT16
            case DataType::UINT32: return static_cast<DataType>(106);  // TYPED_ARRAY_UINT32
            case DataType::UINT64: return static_cast<DataType>(107);  // TYPED_ARRAY_UINT64
            case DataType::FLOAT32: return static_cast<DataType>(108); // TYPED_ARRAY_FLOAT32
            case DataType::FLOAT64: return static_cast<DataType>(109); // TYPED_ARRAY_FLOAT64
            default: return static_cast<DataType>(110);                // TYPED_ARRAY_UNKNOWN
        }
    }
};

struct TypedArrayMethodCall : ArrayExpressionNode {
    std::unique_ptr<ExpressionNode> array_expr_;
    std::string method_name_;
    std::vector<std::unique_ptr<ExpressionNode>> arguments_;
    DataType element_type_;  // Known at compile time
    
    TypedArrayMethodCall(std::unique_ptr<ExpressionNode> array_expr, const std::string& method, DataType element_type)
        : array_expr_(std::move(array_expr)), method_name_(method), element_type_(element_type) {}
    
    void generate_code(CodeGenerator& gen, TypeInference& types) override {
        // Generate array expression
        array_expr_->generate_code(gen, types);
        gen.emit_mov_mem_reg(-16, 0);  // Store array pointer
        
        // Generate arguments
        for (const auto& arg : arguments_) {
            arg->generate_code(gen, types);
            gen.emit_push_reg(0);
        }
        
        // Load array pointer
        gen.emit_mov_reg_mem(7, -16);
        
        // Generate ultra-fast type-specific method call
        std::string method_function = "__" + get_type_prefix(element_type_) + "_array_" + method_name_;
        
        if (method_name_ == "push") {
            // Ultra-fast push with type conversion
            gen.emit_pop_reg(6);  // Pop argument
            gen.emit_call(method_function + "_direct");
            result_type = DataType::VOID;
        } else if (method_name_ == "pop") {
            gen.emit_call(method_function + "_direct");
            result_type = element_type_;
        } else if (method_name_ == "sum" || method_name_ == "mean" || method_name_ == "max" || method_name_ == "min") {
            // SIMD-optimized statistical operations
            gen.emit_call(method_function + "_simd");
            result_type = DataType::FLOAT64;  // Statistical results are always float64
        } else if (method_name_ == "at") {
            // Multi-dimensional access
            gen.emit_call(method_function + "_at_direct");
            result_type = element_type_;
        } else {
            throw std::runtime_error("Unsupported typed array method: " + method_name_);
        }
    }
    
private:
    std::string get_type_prefix(DataType type) {
        switch (type) {
            case DataType::INT8: return "int8";
            case DataType::INT16: return "int16";
            case DataType::INT32: return "int32";
            case DataType::INT64: return "int64";
            case DataType::UINT8: return "uint8";
            case DataType::UINT16: return "uint16";
            case DataType::UINT32: return "uint32";
            case DataType::UINT64: return "uint64";
            case DataType::FLOAT32: return "float32";
            case DataType::FLOAT64: return "float64";
            default: return "unknown";
        }
    }
};

// ============================================================================
// DYNAMIC ARRAY AST NODES - Runtime flexibility
// ============================================================================

struct DynamicArrayLiteral : ArrayExpressionNode {
    std::vector<std::unique_ptr<ExpressionNode>> elements_;
    
    void generate_code(CodeGenerator& gen, TypeInference& types) override {
        // Create dynamic array
        gen.emit_call("__create_dynamic_array");
        gen.emit_mov_mem_reg(-16, 0);  // Store array pointer
        
        // Add each element with type checking
        for (const auto& element : elements_) {
            element->generate_code(gen, types);
            gen.emit_mov_reg_mem(7, -16);  // RDI = array pointer
            gen.emit_mov_reg_reg(6, 0);    // RSI = element value
            gen.emit_call("__dynamic_array_push");  // With type checking and conversion
        }
        
        // Return array pointer
        gen.emit_mov_reg_mem(0, -16);
        result_type = DataType::DYNAMIC_ARRAY;
    }
};

struct DynamicArrayMethodCall : ArrayExpressionNode {
    std::unique_ptr<ExpressionNode> array_expr_;
    std::string method_name_;
    std::vector<std::unique_ptr<ExpressionNode>> arguments_;
    
    DynamicArrayMethodCall(std::unique_ptr<ExpressionNode> array_expr, const std::string& method)
        : array_expr_(std::move(array_expr)), method_name_(method) {}
    
    void generate_code(CodeGenerator& gen, TypeInference& types) override {
        // Generate array expression
        array_expr_->generate_code(gen, types);
        gen.emit_mov_mem_reg(-16, 0);  // Store array pointer
        
        // Generate arguments
        for (const auto& arg : arguments_) {
            arg->generate_code(gen, types);
            gen.emit_push_reg(0);
        }
        
        // Load array pointer
        gen.emit_mov_reg_mem(7, -16);
        
        // Call dynamic array method with type checking
        if (method_name_ == "push") {
            gen.emit_pop_reg(6);  // Pop argument
            gen.emit_call("__dynamic_array_push");
            result_type = DataType::VOID;
        } else if (method_name_ == "pop") {
            gen.emit_call("__dynamic_array_pop");
            result_type = DataType::DYNAMIC_VALUE;
        } else if (method_name_ == "sum" || method_name_ == "mean" || method_name_ == "max" || method_name_ == "min") {
            gen.emit_call("__dynamic_array_" + method_name_);
            result_type = DataType::FLOAT64;
        } else if (method_name_ == "at") {
            gen.emit_call("__dynamic_array_at");
            result_type = DataType::DYNAMIC_VALUE;
        } else {
            throw std::runtime_error("Unsupported dynamic array method: " + method_name_);
        }
    }
};

// ============================================================================
// ARRAY ACCESS AST NODES
// ============================================================================

struct TypedArrayAccess : ArrayExpressionNode {
    std::unique_ptr<ExpressionNode> array_expr_;
    std::unique_ptr<ExpressionNode> index_expr_;
    DataType element_type_;
    
    TypedArrayAccess(std::unique_ptr<ExpressionNode> array_expr, std::unique_ptr<ExpressionNode> index_expr, DataType element_type)
        : array_expr_(std::move(array_expr)), index_expr_(std::move(index_expr)), element_type_(element_type) {}
    
    void generate_code(CodeGenerator& gen, TypeInference& types) override {
        // Generate array and index
        array_expr_->generate_code(gen, types);
        gen.emit_mov_mem_reg(-16, 0);  // Store array pointer
        
        index_expr_->generate_code(gen, types);
        gen.emit_mov_reg_mem(7, -16);  // RDI = array pointer
        gen.emit_mov_reg_reg(6, 0);    // RSI = index
        
        // Ultra-fast direct access - no bounds checking in release mode
        std::string access_function = "__" + get_type_prefix(element_type_) + "_array_get_direct";
        gen.emit_call(access_function);
        
        result_type = element_type_;
    }
    
private:
    std::string get_type_prefix(DataType type) {
        switch (type) {
            case DataType::INT8: return "int8";
            case DataType::INT16: return "int16";
            case DataType::INT32: return "int32";
            case DataType::INT64: return "int64";
            case DataType::UINT8: return "uint8";
            case DataType::UINT16: return "uint16";
            case DataType::UINT32: return "uint32";
            case DataType::UINT64: return "uint64";
            case DataType::FLOAT32: return "float32";
            case DataType::FLOAT64: return "float64";
            default: return "unknown";
        }
    }
};

struct DynamicArrayAccess : ArrayExpressionNode {
    std::unique_ptr<ExpressionNode> array_expr_;
    std::unique_ptr<ExpressionNode> index_expr_;
    
    DynamicArrayAccess(std::unique_ptr<ExpressionNode> array_expr, std::unique_ptr<ExpressionNode> index_expr)
        : array_expr_(std::move(array_expr)), index_expr_(std::move(index_expr)) {}
    
    void generate_code(CodeGenerator& gen, TypeInference& types) override {
        // Generate array and index
        array_expr_->generate_code(gen, types);
        gen.emit_mov_mem_reg(-16, 0);  // Store array pointer
        
        index_expr_->generate_code(gen, types);
        gen.emit_mov_reg_mem(7, -16);  // RDI = array pointer
        gen.emit_mov_reg_reg(6, 0);    // RSI = index
        
        // Dynamic access with bounds checking
        gen.emit_call("__dynamic_array_get");
        
        result_type = DataType::DYNAMIC_VALUE;
    }
};