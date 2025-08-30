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
- Function accesses no scopes directly, but descendant needs parent scope = `16 + (1 * 8) = 24 bytes`
- Function has no closures and no descendants with closures = `16 + (0 * 8) = 16 bytes`

## Lexical Scope Integration

### Scope Packing

Function instances are stored **within** the lexical scope objects they belong to:
- Regular variables are packed first in the lexical scope
- Function instances are packed at the end
- Each function instance gets a name/offset in the lexical scope's variable map

### Scope Size Calculation

The total lexical scope size includes:
1. Regular variables (computed by existing packer)
2. All function instances defined in this scope (computed from their individual sizes)

### Memory Layout Example
```
Lexical Scope Object (heap-allocated):
[Regular variables...] [FunctionInstance1] [FunctionInstance2] [...]
^                     ^                  ^
offset 0              offset N          offset N+size1
```

## Function Call Mechanism

### Call Site Generation

When calling a function instance:
1. Load pointer to the function instance (from lexical scope offset)
2. Pass this pointer as first argument (RDI)
3. Call the function directly

### Custom Function Prologue

Each function gets a custom prologue generated based on its scope analysis:

**Register Usage Optimization**: Only the registers that a function actually needs are saved/restored:
- **Pure function** (no closures): Only pushes/pops R15 for local scope
- **Function with 1 closure**: Pushes/pops R12 and R15
- **Function with 2 closures**: Pushes/pops R12, R13, and R15
- **Function with 3+ closures**: Pushes/pops R12, R13, R14, and R15 (plus stack for extras)

```asm
function_x_code:
    ; RDI = pointer to our function instance
    ; Custom prologue - save ONLY the registers this function will use
    ; push r12             ; Save caller's R12 (ONLY IF this function uses R12)
    ; push r13             ; Save caller's R13 (ONLY IF this function uses R13)  
    ; push r14             ; Save caller's R14 (ONLY IF this function uses R14)
    push r15               ; Save caller's R15 (ALWAYS - every function needs local scope)
    
    ; Now load exactly the scopes we need
    mov rax, [rdi + 8]     ; Load function_code_addr (verify if needed)
    ; mov r12, [rdi + 16]  ; Load most frequent ancestor scope (ONLY IF needed)
    ; mov r13, [rdi + 24]  ; Load 2nd most frequent ancestor scope (ONLY IF needed) 
    ; mov r14, [rdi + 32]  ; Load 3rd most frequent ancestor scope (ONLY IF needed)
    
    ; If more than 3 scopes, push additional ones onto stack
    ; (This case is rare in practice)
    ; Example: if function needs 5 ancestor scopes total
    mov rax, [rdi + 40]    ; Load 4th ancestor scope
    push rax               ; Push to stack (accessed as [rsp + 8])
    mov rax, [rdi + 48]    ; Load 5th ancestor scope  
    push rax               ; Push to stack (accessed as [rsp + 0])
    ; Note: Stack grows downward, so last pushed is at [rsp + 0]
    
    ; Allocate heap object for R15 (current scope)
    call __allocate_lexical_scope_heap_object
    mov r15, rax           ; R15 = current function's scope
    
    ; Function body follows...
    ; Variables accessed as: 
    ; [r12+offset] - Most frequent ancestor scope
    ; [r13+offset] - 2nd most frequent ancestor scope  
    ; [r14+offset] - 3rd most frequent ancestor scope
    ; [r15+offset] - Current function's local scope
    ; For additional scopes on stack:
    ; mov rax, [rsp + 0]     ; Get 5th ancestor scope pointer
    ; mov rbx, [rax + offset] ; Access variable in that scope
    ; mov rax, [rsp + 8]     ; Get 4th ancestor scope pointer  
    ; mov rcx, [rax + offset] ; Access variable in that scope
    
    ; ... main function body ...
    
    ; Function epilogue - cleanup and restore registers
    ; First clean up any extra scope pointers we pushed
    ; add rsp, 16            ; Remove 2 pushed scope pointers (2 * 8 bytes) if applicable
    
    ; Restore caller's registers in reverse order (LIFO) - ONLY the ones we saved
    pop r15                ; Restore caller's R15 (ALWAYS)
    ; pop r14              ; Restore caller's R14 (ONLY IF we pushed it)
    ; pop r13              ; Restore caller's R13 (ONLY IF we pushed it)
    ; pop r12              ; Restore caller's R12 (ONLY IF we pushed it)
    ret                    ; Return to caller
```

## Implementation Phases

### Phase 1: Parse-Time Analysis

1. **AST Traversal**: Analyze each function to determine which ancestor scopes it accesses
2. **Scope Ranking**: Order accessed scopes by frequency (most frequent gets R12)
3. **Size Computation**: Calculate exact size of each function instance
4. **Lexical Scope Integration**: Add function instances to lexical scope packing

### Phase 2: Code Generation

1. **Function Definition Code**: Generate the raw machine code with custom prologue
2. **Function Instance Allocation**: Generate code to allocate space in lexical scopes
3. **Call Site Generation**: Generate direct calls with function instance pointers

### Phase 3: Runtime Initialization

1. **Scope Allocation**: Allocate heap objects for lexical scopes (including function instances)
2. **Function Instance Population**: Fill in function code addresses and captured scope pointers
3. **Execution**: Direct function calls with optimal register allocation

## Key Performance Benefits

### Compile-Time Optimizations
- **Zero unused scopes**: Only accessed ancestor scopes are captured
- **Optimal register allocation**: Most frequent scopes get fastest register access
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
