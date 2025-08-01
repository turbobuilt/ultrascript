# Ultra-Performance Array Compiler Integration

## Overview

This design eliminates runtime type checking by using compile-time type inference and generating completely different code paths for typed vs untyped arrays.

## Compiler Integration Strategy

### 1. Parse-Time Type Inference

The parser determines array type at parse time and generates different AST nodes:

```cpp
// In parser.cpp - Array literal parsing
if (is_explicit_typed_declaration(current_variable)) {
    // var x: [int64] = [1,2,3]
    DataType element_type = get_declared_element_type(current_variable);
    return std::make_unique<TypedArrayLiteral>(element_type, elements);
} else if (has_dtype_parameter(factory_call)) {
    // Array.zeros([10], { dtype: "int64" })
    DataType element_type = parse_dtype_parameter(factory_call);
    return std::make_unique<TypedArrayFactoryCall>(element_type, factory_call);
} else {
    // var x = [1,2,3] or var x = []
    return std::make_unique<DynamicArrayLiteral>(elements);
}
```

### 2. Code Generation - Separate Paths

The JIT generates completely different code for each array type:

```cpp
// In ast_codegen.cpp

void TypedArrayLiteral::generate_code(CodeGenerator& gen, TypeInference& types) {
    switch (element_type_) {
        case DataType::INT64:
            gen.emit_call("__create_int64_array");
            for (auto& element : elements_) {
                element->generate_code(gen, types);
                gen.emit_call("__int64_array_push_direct");  // No type checking!
            }
            break;
        case DataType::FLOAT64:
            gen.emit_call("__create_float64_array");
            for (auto& element : elements_) {
                element->generate_code(gen, types);
                gen.emit_call("__float64_array_push_direct");  // No type checking!
            }
            break;
        // ... etc for each type
    }
    result_type = DataType::TYPED_ARRAY | element_type_;
}

void DynamicArrayLiteral::generate_code(CodeGenerator& gen, TypeInference& types) {
    gen.emit_call("__create_dynamic_array");
    for (auto& element : elements_) {
        element->generate_code(gen, types);
        gen.emit_call("__dynamic_array_push");  // With type checking
    }
    result_type = DataType::DYNAMIC_ARRAY;
}
```

### 3. Runtime Functions - Type-Specific

Generate ultra-fast runtime functions for each type:

```cpp
// In runtime.cpp - Ultra-performance typed array functions

// INT64 Arrays - Zero overhead, direct memory access
extern "C" void* __create_int64_array(int64_t capacity) {
    return new Int64Array();
}

extern "C" void __int64_array_push_direct(void* array_ptr, int64_t value) {
    static_cast<Int64Array*>(array_ptr)->push(value);  // Direct call, no type checking
}

extern "C" int64_t __int64_array_get_direct(void* array_ptr, int64_t index) {
    return (*static_cast<Int64Array*>(array_ptr))[index];  // Direct access, no bounds checking in release
}

extern "C" double __int64_array_sum_simd(void* array_ptr) {
    return static_cast<Int64Array*>(array_ptr)->sum();  // SIMD optimized
}

// FLOAT64 Arrays - SIMD optimized
extern "C" void* __create_float64_array(int64_t capacity) {
    return new Float64Array();
}

extern "C" void __float64_array_push_direct(void* array_ptr, double value) {
    static_cast<Float64Array*>(array_ptr)->push(value);
}

// Dynamic Arrays - Flexible but still optimized
extern "C" void* __create_dynamic_array() {
    return new DynamicArray();
}

extern "C" void __dynamic_array_push(void* array_ptr, void* value_ptr) {
    // Type checking and conversion here
    auto* arr = static_cast<DynamicArray*>(array_ptr);
    auto* val = static_cast<DynamicValue*>(value_ptr);
    arr->push(*val);
}
```

### 4. Type Inference Rules

```cpp
// In type_inference.cpp

DataType infer_array_type(const ArrayExpression* expr) {
    // Check for explicit type declaration
    if (auto typed_decl = expr->get_type_declaration()) {
        return typed_decl->element_type;
    }
    
    // Check for factory method with dtype
    if (auto factory = expr->as_factory_call()) {
        if (factory->has_dtype_parameter()) {
            return parse_dtype_string(factory->get_dtype());
        }
    }
    
    // Check for variable type annotation
    if (current_variable_has_array_type()) {
        return get_variable_element_type();
    }
    
    // Default to dynamic array
    return DataType::DYNAMIC_ARRAY;
}
```

### 5. Variable Type Tracking

```cpp
// In lexical_scope.cpp

void declare_typed_array_variable(const std::string& name, DataType element_type) {
    // Store complete type information
    VariableInfo info;
    info.base_type = DataType::TYPED_ARRAY;
    info.element_type = element_type;
    info.is_mutable = true;
    
    current_scope_->variables[name] = info;
}

void declare_dynamic_array_variable(const std::string& name) {
    VariableInfo info;
    info.base_type = DataType::DYNAMIC_ARRAY;
    info.element_type = DataType::ANY;
    info.is_mutable = true;
    
    current_scope_->variables[name] = info;
}
```

### 6. Method Call Dispatch

Generate type-specific method calls:

```cpp
void ArrayMethodCall::generate_code(CodeGenerator& gen, TypeInference& types) {
    auto array_type = types.get_expression_type(array_expr_);
    
    if (is_typed_array(array_type)) {
        auto element_type = get_element_type(array_type);
        
        if (method_name_ == "push") {
            switch (element_type) {
                case DataType::INT64:
                    argument_->generate_code(gen, types);
                    gen.emit_call("__int64_array_push_direct");
                    break;
                case DataType::FLOAT64:
                    argument_->generate_code(gen, types);
                    gen.emit_call("__float64_array_push_direct");
                    break;
                // ... etc
            }
        } else if (method_name_ == "sum") {
            switch (element_type) {
                case DataType::INT64:
                    gen.emit_call("__int64_array_sum_simd");
                    break;
                case DataType::FLOAT64:
                    gen.emit_call("__float64_array_sum_simd");
                    break;
                // ... etc
            }
        }
    } else {
        // Dynamic array path
        if (method_name_ == "push") {
            argument_->generate_code(gen, types);
            gen.emit_call("__dynamic_array_push");
        } else if (method_name_ == "sum") {
            gen.emit_call("__dynamic_array_sum");
        }
    }
}
```

## Performance Benefits

### 1. Zero Runtime Overhead for Typed Arrays
- No `is_typed_` checks
- No type dispatch switches
- Direct function calls
- Optimal memory layout

### 2. Compile-Time Optimizations
- SIMD vectorization
- Loop unrolling
- Bounds check elimination (release mode)
- Cache-friendly access patterns

### 3. Type-Specific Code Generation
```javascript
// This generates completely different assembly:

// TYPED PATH - Ultra performance
var x: [int64] = [1,2,3];
x.push(42);        // Direct memory write, no checks
var sum = x.sum(); // SIMD vectorized loop

// DYNAMIC PATH - Flexible
var y = [1, "hello", 3.14];
y.push(42);        // Type checking and variant storage
var sum = y.sum(); // Type-dispatched accumulation
```

### 4. Example Generated Assembly Difference

**Typed Array Push (zero overhead):**
```asm
; Direct memory access - 3 instructions
mov rax, [array_ptr]     ; Load array pointer
mov [rax + size*8], rdx  ; Store value directly  
inc qword [rax + 8]      ; Increment size
```

**Dynamic Array Push (flexible):**
```asm
; Type checking and dispatch - ~20 instructions
mov rax, [array_ptr]
call __dynamic_value_create
call __type_check_and_convert  
call __variant_store
call __vector_push_back
```

## Implementation Notes

1. **Parser Changes**: Add type inference logic to array creation
2. **AST Nodes**: Separate `TypedArrayLiteral` vs `DynamicArrayLiteral`
3. **Code Generation**: Type-specific runtime function calls
4. **Runtime**: Template-specialized array implementations
5. **Type System**: Track element types in variable declarations

This approach gives you the best of both worlds: **C-level performance for typed arrays** and **JavaScript flexibility for dynamic arrays** - all determined at compile time with zero runtime overhead.
