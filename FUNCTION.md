# UltraScript High-Performance Closure System

## Overview

This document describes UltraScript's ultra-high-performance closure and function system that achieves native C-level performance through compile-time analysis, direct address calls, and optimal register allocation for lexical scopes.

## Core Concepts

### Function Definition vs Function Instance

**Function Definition**: The raw machine code for a function - this is shared and never changes.

**Function Instance**: A runtime object that contains:
- Pointer to the function definition code
- Captured lexical scope addresses needed by this closure
- Size metadata for variable-length scope data

All functions in UltraScript are treated as function instances, even simple functions without closures (they just have empty scope lists).

### Lexical Scope Management

**Scope Allocation**: All lexical scopes are allocated on the heap for simplicity (stack allocation for non-escaping scopes will be added later as an optimization).

**Scope Registers**: 
- `R15`: Always points to current function's local scope (heap-allocated object)
- `R12`: Most frequently accessed ancestor scope  
- `R13`: 2nd most frequently accessed ancestor scope
- `R14`: 3rd most frequently accessed ancestor scope
- **Stack**: Additional ancestor scopes (for functions needing >3 ancestor scopes)

**Scope Ranking**: The AST analyzer determines which ancestor scopes each function accesses and ranks them by frequency. Only accessed scopes are captured - unused ancestor scopes are omitted entirely.

## Function Instance Structure

### Basic Layout
```cpp
struct FunctionInstance {
    uint64_t size;              // Total size of this instance (since it's variable-length)
    void* function_code_addr;   // Address of the actual function machine code
    void* lex_addr1;           // Most frequent scope (-> R12)
    void* lex_addr2;           // 2nd most frequent scope (-> R13) 
    void* lex_addr3;           // 3rd most frequent scope (-> R14)
    // ... additional scope addresses for stack if needed
};
```

### Size Computation

At parse time, the LexicalScopeNode analysis determines:
1. Which ancestor scopes this function accesses (stored in `self_dependencies`)
2. Which lexical scope addresses descendant functions need (stored in `descendant_dependencies`)
3. How many scopes need to be captured in total
4. The exact size of the function instance: `16 + (scope_count * 8)` bytes

The scope access frequency ranking is computed and stored in `priority_sorted_parent_scopes`.

Example:
- Function accesses global + parent scope directly, plus descendant needs grandparent scope = `16 + (3 * 8) = 40 bytes`
- Function accesses parent scope directly, descendant needs global scope = `16 + (2 * 8) = 32 bytes`  
- Function accesses no scopes directly, but descendant needs ancestor scope = `16 + (1 * 8) = 24 bytes`
- Function has no closures and no descendants with closures = `16 + (0 * 8) = 16 bytes`

## Lexical Scope Integration

### Variable Classification and Packing Strategy

**DynamicValue Classification**: Any variable that EVER receives a function assignment becomes a `DataType::DYNAMIC_VALUE` permanently, regardless of other assignments.

```javascript
var f;        // Initially unknown type
f = 42;       // Regular variable
f = function() { return 5; };  // Now f becomes DYNAMIC_VALUE forever
f = "hello";  // Still DYNAMIC_VALUE, can hold any type
```

**Packing Algorithm**:
1. **Regular Variables**: Standard-typed variables (never assigned functions) packed first
2. **DynamicValue Variables**: All variables that ever hold functions, grouped at the end
3. **Function Declarations**: Dedicated function declarations (`function test() {}`) packed with DynamicValues
4. **Optimal Alignment**: All DynamicValues and functions use 8-byte alignment

### Memory Layout Strategy
The key thing to note is that functions can take up different sizes based on how many lexical scope addresses they need

**Offset Access**: All variables accessible via `LexicalScopeNode.variable_offsets[variable_name]` regardless of type or position.

### Static Analysis Integration

**Conservative Maximum Size**: For variables that receive function assignments:
- `variable_function_sizes_`: Tracks all function instance sizes assigned to each variable
- `get_max_function_size(var_name)`: Returns maximum size for DynamicValue allocation
- **Allocation**: DynamicValue gets sized to hold largest possible function instance

## Function Variable Storage & Calling Strategies

UltraScript uses three distinct strategies for function variables based on static analysis, optimizing the most common scenarios for maximum performance.

### Strategy 1: Static Single Function Assignment (FASTEST)

**Scenario**: Function defined once, variable never reassigned.

```javascript
var myFunc = function() { return 42; };
// myFunc is never reassigned - static analysis confirms single function
// or
function test(){} // never reassigned, hoist
```

**Storage Strategy**:
- **Compile-time type**: `DataType::LocalFunctionInstance`
- **Storage location**: Direct allocation in lexical scope using computed function instance size
- **No runtime type checks**: Compiler statically knows it's always a function
- **Call mechanism**: Direct function call with zero indirection

**Generated Code Example**:
```asm
; Variable allocation (in lexical scope initialization)
; myFunc allocated at [r15 + offset] with exact computed size (e.g., 32 bytes)

; Function call: myFunc()
mov rdi, r15
add rdi, myFunc_offset     ; RDI = pointer to function instance
call [rdi + 8]             ; Direct call to function code (zero additional indirection)
```

**Performance**: Maximum optimization - most common scenario gets fastest execution.

### Strategy 2: Function-Typed Variables (FAST)

**Scenario**: Variables explicitly typed as Function, using Conservative Maximum Size approach.

```javascript
var x: Function = function() { return 1; };
x = function() { return 2; };  // Conservative max size handles both

function processCallback(callback: Function) {
    callback();  // Parameter typed as Function
}
```

**Storage Strategy**:
- **Compile-time type**: `DataType::LocalFunctionInstance` (for storage), `DataType::PointerFunctionInstance` (for parameters)
- **Storage allocation**: Conservative Maximum Size - analyze all possible function assignments, allocate space for largest
- **No runtime type checks**: Type system guarantees these are functions
- **Call mechanism**: Single pointer indirection for parameters, direct for local variables

**Conservative Maximum Size Analysis**:
```cpp
// During static analysis
for (auto assignment : variable_function_assignments["x"]) {
    max_size = std::max(max_size, assignment.function_instance_size);
}
// Allocate x with max_size in lexical scope
```

**Generated Code Example**:
```asm
; Variable allocation - using conservative maximum size
; x allocated at [r15 + offset] with maximum required size (e.g., 48 bytes)

; Local function call: x()  
mov rdi, r15
add rdi, x_offset          ; RDI = pointer to function instance  
call [rdi + 8]             ; Direct call to function code

; Parameter passing: processCallback(x)
mov rdi, r15
add rdi, x_offset          ; Create PointerFunctionInstance (just the address)
call processCallback       ; Pass pointer to the function instance

; Inside processCallback (callback: Function parameter)
; RDI already contains pointer to function instance
call [rdi + 8]             ; Single indirection call
```

### Strategy 3: Any-Typed Variables with Mixed Assignment (DYNAMIC)

**Scenario**: Variables that can hold functions OR other types, requiring runtime type safety.

```javascript
var x = function() { return 1; };
x = 5;  // Now x can be function OR number - needs dynamic typing

function processAny(param) {  // Untyped parameter
    param();  // Must check if param is actually callable
}
```

**Storage Strategy**:
- **Runtime type**: `DynamicValue subtype LocalFunctionInstance` (for storage), `DynamicValue subtype PointerFunctionInstance` (for passing)
- **Storage allocation**: DynamicValue wrapper + Conservative Maximum Size for function data
- **Runtime type checks**: Required on every function call
- **Call mechanism**: Type check + branch + indirection

**Storage Layout**:
```cpp
// In lexical scope
struct DynamicValue {
    DataType type;             // Runtime type tag
    union {
        double number_value;
        void* pointer_value;
        LocalFunctionInstance function_data;  // Using conservative max size
    };
};
```

**Generated Code Example**:
```asm
; Function call: x() where x is DynamicValue
mov rax, [r15 + x_offset]     ; Load type tag  
cmp rax, FUNCTION_TYPE        ; Check if it's a function
jne .not_a_function           ; Branch if not function
.is_function:
    mov rdi, r15
    add rdi, x_offset + 8     ; RDI = pointer to function data within DynamicValue
    call [rdi + 8]            ; Call the function
    jmp .done
.not_a_function:
    ; Throw TypeError: x is not a function
    call __throw_type_error
.done:

; Parameter passing: processAny(x)  
mov rdi, r15
add rdi, x_offset             ; Pass entire DynamicValue as PointerFunctionInstance
call processAny

; Inside processAny (param is untyped)
mov rax, [rdi]                ; Load type tag from DynamicValue
cmp rax, FUNCTION_TYPE        ; Check if it's a function  
jne .not_callable             ; Branch if not callable
.is_callable:
    add rdi, 8                ; Adjust to point to function data
    call [rdi + 8]            ; Call the function
    jmp .done
.not_callable:
    call __throw_type_error
.done:
```

## Type System Integration

### Compile-Time Type Distinctions

**Important**: There are different types with similar names:

1. **`DataType::LocalFunctionInstance`**: Compile-time type for statically-known function variables
2. **`DynamicValue subtype LocalFunctionInstance`**: Runtime wrapper for dynamically-typed function variables

### Parameter Passing Mechanics

**Function-typed parameters** (`function(x: Function){}`) use `DataType::PointerFunctionInstance`:
- Just a pointer to the LocalFunctionInstance in the caller's lexical scope
- No runtime type checks needed
- Single indirection for calls

**Untyped parameters** (`function(x){}`) use `DynamicValue subtype PointerFunctionInstance`:
- Contains type tag + pointer to function data
- Runtime type checks required
- Type check + branch + indirection for calls

**Parameter Passing Strategy**: Pass by pointer (not by value) because:
- JavaScript functions are passed frequently (callbacks, higher-order functions)
- Lexical scope addresses are stable after compilation
- Memory overhead of copying function instances (with closures) outweighs indirection cost
- Functions are often called multiple times after being passed

## Performance Hierarchy

1. **Strategy 1 (Static Single Assignment)**: Direct call, zero indirection - **FASTEST**
2. **Strategy 2 (Function-Typed)**: Single pointer indirection, no type checks - **FAST**  
3. **Strategy 3 (Any-Typed Dynamic)**: Type check + branch + indirection - **SLOWER**

This hierarchy optimizes the most common JavaScript patterns while maintaining full dynamic typing compatibility when needed.

## Key Performance Benefits

### Compile-Time Optimizations
- **Static analysis eliminates checks**: Most function variables don't need runtime type checking
- **Conservative maximum size**: Function-typed variables avoid DynamicValue overhead
- **Optimal call paths**: Common cases get fastest possible execution
- **Custom prologues**: Each function loads exactly what it needs
- **Size prediction**: All memory layouts computed at compile time

### Runtime Performance
- **Direct calls**: No function table lookups or indirection
- **Register-based scope access**: Variables accessed as `[r12+offset]` (single instruction)
- **Minimal overhead**: Function instances are compact, fixed-size structures
- **Cache efficiency**: Related scopes stored in nearby registers

### Memory Efficiency
- **Heap allocation**: Supports escaping closures and long-lived functions
- **Compact representation**: No wasted space for unused scopes
- **Predictable layout**: Enables garbage collection and memory management optimizations

## Function Storage Strategy & Calling Mechanisms

### Pure Assembly Function Storage (No C++ Dependencies)

**JIT-Generated Code**: Pure assembly with no `std::variant` or C++ standard library calls. Functions stored as raw memory structures.

**Raw Function Variable Structure** (generated in assembly):
```asm
; Function variable memory layout (allocated in lexical scope)
function_variable:
    dq FUNCTION_TYPE_TAG        ; 8 bytes - type identifier (constant)
    dq function_instance_ptr    ; 8 bytes - pointer to function instance data
    ; Followed by inline function instance data:
    dq function_instance_size   ; 8 bytes - size of this function instance
    dq function_code_addr       ; 8 bytes - pointer to actual function machine code
    dq lex_scope_addr_1         ; 8 bytes - captured scope 1 (R12)
    dq lex_scope_addr_2         ; 8 bytes - captured scope 2 (R13)
    ; ... additional scope addresses as needed
```

### Local Function Calls vs Function Passing

#### Local Function Calls (Maximum Performance)
```asm
; f() - calling local function variable
mov rdi, [scope + f_offset]           ; Load function variable base address  
cmp qword [rdi], FUNCTION_TYPE_TAG    ; Verify it's a function type
jne error_not_function                ; Jump if not a function
mov rdi, [rdi + 8]                    ; Load function instance pointer (points to inline data)
call [rdi + 8]                        ; Call function_code_addr (zero additional indirection)
```

#### Function Passing (Runtime Allocation)  
When passing function as parameter, call runtime to create heap copy:
```asm
; setTimeout(f, 1000) - f must be heap-allocated for passing
mov rdi, [scope + f_offset]           ; Load source function variable
call __runtime_copy_function_to_heap  ; Create heap copy via runtime call
mov rdi, rax                         ; RAX = pointer to heap-allocated function wrapper
mov rsi, 1000                        ; Second argument
call setTimeout                      ; Pass heap-allocated function
```

#### Runtime Function Copying
```cpp
// In runtime.cpp - called by generated code when functions need to be passed
extern "C" void* __runtime_copy_function_to_heap(void* local_function_var) {
    // Extract function instance pointer from local variable
    void** var_ptr = static_cast<void**>(local_function_var);
    if (var_ptr[0] != reinterpret_cast<void*>(FUNCTION_TYPE_TAG)) {
        return nullptr; // Not a function
    }
    
    void* function_instance = var_ptr[1];
    uint64_t* instance_data = static_cast<uint64_t*>(function_instance);
    uint64_t instance_size = instance_data[0];
    
    // Allocate heap memory and copy function instance
    void* heap_copy = malloc(16 + instance_size); // Type tag + pointer + instance data
    uint64_t* heap_data = static_cast<uint64_t*>(heap_copy);
    heap_data[0] = FUNCTION_TYPE_TAG;
    heap_data[1] = reinterpret_cast<uint64_t>(heap_data + 2); // Points to following data
    
    // Copy function instance data
    memcpy(heap_data + 2, function_instance, instance_size);
    
    return heap_copy;
}
```

### Conservative Maximum Size Allocation

**Parse-time Analysis**: Each function variable allocated as:
```
Total size per function variable = 16 + max_function_instance_size
├─ 8 bytes: FUNCTION_TYPE_TAG
├─ 8 bytes: Pointer to inline function instance data  
└─ max_function_instance_size bytes: Largest possible function instance for this variable
```

**Performance**: Local function calls have zero additional indirection beyond the standard function instance call mechanism.

## Example Implementation

### Source Code
```javascript
var global_x = 10;
function outer() {
    var outer_y = 20;
    var inner = function() {
        console.log(global_x + outer_y); // Accesses global and outer scopes
    };
    inner();
}
```

### Analysis Results
- `inner` function accesses: global_scope (global_x), outer_scope (outer_y)
- Function instance size: `16 + (2 * 8) = 32 bytes`
- Register allocation: R12 = global_scope, R13 = outer_scope

### Generated Assembly
```asm
inner_function_code:
    ; RDI = pointer to inner's function instance
    ; Save caller's registers
    push r12
    push r13
    push r15
    
    ; Load our scopes
    mov r12, [rdi + 16]    ; Load global scope -> R12
    mov r13, [rdi + 24]    ; Load outer scope -> R13
    call __allocate_lexical_scope_heap_object
    mov r15, rax           ; R15 = inner's local scope
    
    ; Function body:
    mov rax, [r12 + global_x_offset]  ; Load global_x
    add rax, [r13 + outer_y_offset]   ; Add outer_y  
    ; ... console.log call ...
    
    ; Restore caller's registers
    pop r15
    pop r13  
    pop r12
    ret
```

This system provides native-level performance for closures while maintaining full lexical scoping semantics and supporting all advanced JavaScript/TypeScript closure features.
