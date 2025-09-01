# UltraScript Function System Redesign - Pure Machine Code Implementation

## Problem Analysis

The current function system uses runtime calls and a global scope registry, which violates UltraScript's core principle of "Direct Machine Code Generation: No intermediate representation for maximum speed". We need to eliminate all runtime overhead and implement closures with pure machine code.

### Current Issues
1. **Runtime Scope Registry**: Using hash maps with mutex locking to track scope addresses
2. **Runtime Function Calls**: `initialize_function_variable`, `__get_scope_address_for_depth`, etc.
3. **Memory Management**: Using `malloc`/`free` runtime calls instead of direct syscalls
4. **Scope Lifetime**: Incorrect scope cleanup causing segfaults in closures

## New Architecture: Stack-Based Closure System

### Core Principle
**All scope management and function calling should be compile-time determined stack operations with zero runtime overhead.**

## 1. Function Instance Structure (Pure Data)

```cpp
struct FunctionInstance {
    uint64_t size;                    // Total size of this structure
    void* function_code_addr;         // Direct pointer to function code
    uint64_t num_captured_scopes;     // Number of captured parent scopes
    void* scope_addresses[];          // ACTUAL scope addresses (not depth IDs!)
};
```

**Key Change**: Store **actual memory addresses** of captured scopes, not depth references.

## 2. Stack-Based Calling Convention

### Function Instance Call Stack Layout
```
[return address]              <- rsp (pushed by CALL)
[callee priority scope 0]     <- rsp + 8  (callee's most frequent parent scope)
[callee priority scope 1]     <- rsp + 16 (callee's 2nd most frequent parent scope)  
[callee priority scope N]     <- rsp + 8*(N+1) (callee's Nth priority parent scope)
[parameter 0]                 <- rsp + 8*(num_scopes+1)
[parameter 1]                 <- rsp + 8*(num_scopes+2)
[local variables...]          <- allocated by callee prologue
```

### Function Instance Creation (Uses Index Mappings)
```assembly
; CRITICAL: This is where index mappings are used to arrange scopes correctly
; When creating function instance for child function inside parent function:

; Create function instance header
mov rax, FUNCTION_INSTANCE_SIZE     ; Size of function instance (static analysis)
mov [r15 + func_offset], rax        ; Store size at function variable offset

mov rax, function_code_addr         ; Function code address (patched later by linker)
mov [r15 + func_offset + 8], rax    ; Store function_code_addr

mov rax, NUM_CAPTURED_SCOPES        ; Number of captured scopes (compile-time constant)
mov [r15 + func_offset + 16], rax   ; Store num_captured_scopes

; NOW: Copy captured scope addresses using pre-computed index-to-index mappings
; Static analysis computed parent_location_indexes[] for this child function
; Example: parent_location_indexes = [-1, 0, 2] means:
;   child's scopes[0] comes from parent's local scope (index -1 = r15)  
;   child's scopes[1] comes from parent's register index 0 (r12)
;   child's scopes[2] comes from parent's register index 2 (r14)

; Copy scope for child_index 0 from parent_location_indexes[0] = -1 (parent's r15)
mov rax, r15                        ; Get parent's local scope address
mov [r15 + func_offset + 24], rax   ; Store at scopes[0] in child's expected order

; Copy scope for child_index 1 from parent_location_indexes[1] = 0 (parent's r12)  
mov [r15 + func_offset + 32], r12   ; Store at scopes[1] in child's expected order

; Copy scope for child_index 2 from parent_location_indexes[2] = 2 (parent's r14)
mov [r15 + func_offset + 40], r14   ; Store at scopes[2] in child's expected order

; ... continue for all mappings in parent_location_indexes[]
; The function instance now contains scopes in the child's expected priority order
```

### Calling Sequence (Uses Pre-Arranged Scopes)
```assembly
; The function instance already has scopes arranged in callee's priority order
; thanks to the index mapping during creation above

; Load function instance address  
mov rax, [r15 + func_var_offset]     ; Load function instance pointer

; Push captured scopes in reverse order (stack grows down, callee expects them in forward order)
mov rbx, [rax + 16]                  ; Load num_captured_scopes
; Push scopes[num_scopes-1] down to scopes[0] 
; These scopes are already in the correct order thanks to index mapping during creation
push qword ptr [rax + 24 + (rbx-1)*8] ; Push last scope first (child's lowest priority parent)
push qword ptr [rax + 24 + (rbx-2)*8] ; Push second-to-last scope
; ... continue until:
push qword ptr [rax + 24]              ; Push scopes[0] last (child's highest priority parent)

; Push parameters (if any) - standard calling convention  
push parameter_1
push parameter_0

; Call the function
mov rax, [rax + 8]                   ; Load function_code_addr from function instance
call rax                             ; Call function directly

; Clean up stack after call (caller responsibility)
add rsp, TOTAL_PUSHED_BYTES          ; Remove scopes + parameters from stack
```

## 3. Function Prologue (Statically Generated)

Each function gets a **compile-time generated prologue** based on static analysis:

### Prologue Assembly (Example for 2 parent scopes needed)
```assembly
function_name:
    push rbp                    ; Standard prologue
    mov rbp, rsp
    
    ; Save only needed callee-saved registers (determined by static analysis)
    push r12                    ; Save r12 (only if this function needs 1+ parent scopes)
    push r13                    ; Save r13 (only if this function needs 2+ parent scopes)
    ; Skip r14 if function only needs 0-2 scopes
    
    ; Load parent scopes from stack parameters (passed by caller)
    ; Stack layout: [return_addr][scope0][scope1][param0][param1]...
    ; Use rbp for stable stack access
    mov r12, [rbp + 16]         ; Load scopes[0] from stack (caller's first pushed scope)
    mov r13, [rbp + 24]         ; Load scopes[1] from stack (caller's second pushed scope)
    ; r14 would get [rbp + 32] if needed, else 4+ scopes use direct [rbp + offset] access
    
    ; Save current scope register and allocate new scope
    push r15                    ; Save caller's r15
    
    ; Allocate local scope memory (direct syscall, not malloc)
    mov rax, 9                  ; sys_mmap
    mov rdi, 0                  ; addr = NULL
    mov rsi, SCOPE_SIZE         ; length = computed scope size (from static analysis)
    mov rdx, 3                  ; prot = PROT_READ | PROT_WRITE
    mov r10, 34                 ; flags = MAP_PRIVATE | MAP_ANONYMOUS
    mov r8, -1                  ; fd = -1
    mov r9, 0                   ; offset = 0
    syscall                     ; Direct system call
    
    mov r15, rax                ; Store local scope address in r15
```

### Epilogue Assembly (Restore in Reverse Order)
```assembly
    ; Free local scope memory (direct syscall)
    mov rdi, r15                ; addr = local scope
    mov rsi, SCOPE_SIZE         ; length
    mov rax, 11                 ; sys_munmap
    syscall
    
    ; Restore registers in reverse order
    pop r15                     ; Restore caller's r15
    pop r13                     ; Restore r13 (only if was saved)
    pop r12                     ; Restore r12 (only if was saved)
    
    pop rbp                     ; Standard epilogue
    ret
```

## 4. Function Instance Creation (Direct Memory Operations)

### Priority Mapping Between Functions

The key complexity is that each function has its own `priority_sorted_parent_scopes` based on its access patterns, but when creating nested function instances, we need to map between different priority orderings.

#### Example Scenario:
```javascript
function outer() {        // depth 1, priority_sorted_parent_scopes = []
  let x = 42; let y = 21;
  
  function middle() {     // depth 2, priority_sorted_parent_scopes = [1]  
    let z = 7;
    
    function inner() {    // depth 3, priority_sorted_parent_scopes = [1, 2]
      console.log(x, z);  // accesses depth 1 most, depth 2 less frequently  
    }
    
    return inner;
  }
}
```

#### Static Analysis Results:
- **`middle` function**: Needs depth 1 → r12 = depth 1, r15 = depth 2 (local scope)
- **`inner` function**: Needs depths [1, 2] in that priority → r12 = depth 1, r13 = depth 2

#### Priority Mapping Algorithm:
```cpp
// When creating function instance for nested_func inside current_func:
void create_function_instance(FunctionDecl* nested_func, size_t func_offset) {
    for (int i = 0; i < nested_func->priority_sorted_parent_scopes.size(); i++) {
        int needed_depth = nested_func->priority_sorted_parent_scopes[i];
        
        // Find where this scope lives in current function's register/stack layout
        Register source_reg = find_scope_location_in_current_function(needed_depth);
        
        // Copy to function instance scopes[i] in nested function's expected order
        emit_mov([r15 + func_offset + 24 + (i * 8)], source_reg);
    }
}

Register get_parent_scope_location(int parent_index) {
    if (parent_index == -1) {
        return r15;  // Parent's local scope
    }
    
    // Map parent index to register/stack location
    if (parent_index == 0) return r12;      // Parent's index 0 → r12
    if (parent_index == 1) return r13;      // Parent's index 1 → r13  
    if (parent_index == 2) return r14;      // Parent's index 2 → r14
    else return stack_location(parent_index - 3);  // Parent's index 3+ → stack
}

// Usage during function instance creation:
void create_function_instance(FunctionDecl* child_func, size_t func_offset) {
    FunctionStaticAnalysis& analysis = child_func->static_analysis;
    
    for (int child_idx = 0; child_idx < analysis.parent_location_indexes.size(); child_idx++) {
        int parent_idx = analysis.parent_location_indexes[child_idx];
        Register source_reg = get_parent_scope_location(parent_idx);
        
        // Copy from parent location to child's function instance
        emit_mov([r15 + func_offset + 24 + (child_idx * 8)], source_reg);
    }
}
```

### Current (Runtime Calls)
```cpp
// BAD: Runtime function call
initialize_function_variable(func_instance_addr, captured_scopes);
```

### New (Direct Machine Code)
```assembly
; Create function instance directly in scope memory at compile-time determined offset
mov rax, FUNCTION_INSTANCE_SIZE     ; Size of function instance (static analysis)
mov [r15 + func_offset], rax        ; Store size at function variable offset

mov rax, function_code_addr         ; Function code address (patched later by linker)
mov [r15 + func_offset + 8], rax    ; Store function_code_addr

mov rax, NUM_CAPTURED_SCOPES        ; Number of captured scopes (compile-time constant)
mov [r15 + func_offset + 16], rax   ; Store num_captured_scopes

; Copy captured scope addresses using pre-computed index-to-index mappings
; Static analysis computed parent_location_indexes[] = [-1, 0, 2] for example

; Copy scope for child_index 0 from parent_index -1 (parent's local scope r15)
mov rax, r15                        ; Get parent's local scope address
mov [r15 + func_offset + 24], rax   ; Store at scopes[0]

; Copy scope for child_index 1 from parent_index 0 (parent's r12)  
mov [r15 + func_offset + 32], r12   ; Store at scopes[1]

; Copy scope for child_index 2 from parent_index 2 (parent's r14)
mov [r15 + func_offset + 40], r14   ; Store at scopes[2]

; ... continue for all scopes using parent_location_indexes mapping
```

## 5. Complete Assembly Code Generation Flow

### Function Instance Creation (Using Index Mappings)
```cpp
// Code generator uses pre-computed mappings to emit assembly
void emit_function_instance_creation(FunctionDecl* child_func, size_t func_offset) {
    FunctionStaticAnalysis& analysis = child_func->static_analysis;
    
    // Emit instance header
    emit_mov_imm_to_mem(FUNCTION_INSTANCE_SIZE, r15 + func_offset);
    emit_mov_imm_to_mem(child_func->code_address, r15 + func_offset + 8);  // Patched later
    emit_mov_imm_to_mem(analysis.parent_location_indexes.size(), r15 + func_offset + 16);
    
    // Emit scope copying using index mappings
    for (int child_idx = 0; child_idx < analysis.parent_location_indexes.size(); child_idx++) {
        int parent_idx = analysis.parent_location_indexes[child_idx];
        size_t dest_offset = func_offset + 24 + (child_idx * 8);
        
        if (parent_idx == -1) {
            // Copy from parent's local scope (r15)
            emit_mov_reg_to_mem(r15, r15 + dest_offset);
        } else {
            // Copy from parent's register based on index
            Register source_reg = get_register_for_parent_index(parent_idx);
            emit_mov_reg_to_mem(source_reg, r15 + dest_offset);
        }
    }
}

Register get_register_for_parent_index(int parent_idx) {
    switch (parent_idx) {
        case 0: return r12;  // Parent's highest priority scope
        case 1: return r13;  // Parent's second priority scope  
        case 2: return r14;  // Parent's third priority scope
        default: 
            // For 3+ parent scopes, need to load from stack
            // This would emit: mov rax, [rsp + stack_offset]; then use rax
            throw std::runtime_error("Stack-based parent scopes not yet implemented");
    }
}
```

### Function Call Generation (Using Function Instance)
```cpp
// Code generator emits assembly to call function instance using index mappings
void emit_function_instance_call(const std::string& func_var_name, size_t func_offset) {
    FunctionStaticAnalysis& analysis = get_function_analysis(func_var_name);
    
    // Load function instance address
    emit_mov_mem_to_reg(r15 + func_offset, rax);  // rax = function instance pointer
    
    // Push captured scopes using the callee's expected order (from function instance)
    // Function instance already has scopes arranged by the index mappings
    int num_scopes = analysis.parent_location_indexes.size();
    
    // Push scopes in reverse order since stack grows down but callee expects forward order
    for (int i = num_scopes - 1; i >= 0; i--) {
        size_t scope_offset = 24 + (i * 8);  // Offset in function instance
        emit_push_mem(rax + scope_offset);    // push qword ptr [rax + scope_offset]
    }
    
    // Push parameters (if any) - standard calling convention
    // ... parameter pushing code ...
    
    // Call the function  
    emit_mov_mem_to_reg(rax + 8, rax);      // Load function_code_addr
    emit_call_reg(rax);                     // call rax
    
    // Clean up stack
    int total_pushed = (num_scopes + num_parameters) * 8;
    emit_add_imm_to_reg(total_pushed, rsp); // add rsp, total_pushed
}
```

## 6. Static Analysis Integration

### Use Existing LexicalScopeNode Data
```cpp
// Already available in LexicalScopeNode:
std::vector<int> priority_sorted_parent_scopes;  // Which scopes this function needs
std::unordered_map<std::string, size_t> variable_offsets;  // Where variables are stored
size_t total_scope_frame_size;  // Total memory needed for scope
```

### Pre-Code-Generation Static Analysis

**CRITICAL**: Before generating any function code, we must compute the mapping between parent and child lexical scope orderings, since they can be completely different.

#### Scope Priority Mapping Analysis
```cpp
struct ScopeMappingInfo {
    int child_index;          // Index in child's priority_sorted_parent_scopes  
    int parent_index;         // Index in parent's register/stack layout (-1 = parent's local scope)
    int scope_depth;          // The actual scope depth (for validation)
};

struct FunctionStaticAnalysis {
    std::vector<int> needed_parent_scopes;              // From priority_sorted_parent_scopes
    int num_registers_needed;                           // 0-3 (r12/r13/r14)
    bool needs_r12, needs_r13, needs_r14;              // Register usage flags
    size_t function_instance_offset;                   // From variable_offsets
    size_t function_instance_size;                     // Computed from captured scopes
    size_t local_scope_size;                           // From total_scope_frame_size
    
    // NEW: Index-to-index mapping computed during static analysis
    std::vector<int> parent_location_indexes;          // child_index → parent_index mapping
    // Example: parent_location_indexes[0] = 2 means child's scope[0] comes from parent's index 2
};
```

#### Static Analysis Algorithm (Pre-Code-Generation)
```cpp
void compute_function_scope_mappings(FunctionDecl* parent_func, FunctionDecl* child_func) {
    FunctionStaticAnalysis& analysis = child_func->static_analysis;
    analysis.parent_location_indexes.resize(child_func->priority_sorted_parent_scopes.size());
    
    // For each scope the child function needs (by child's index)
    for (int child_idx = 0; child_idx < child_func->priority_sorted_parent_scopes.size(); child_idx++) {
        int needed_scope_depth = child_func->priority_sorted_parent_scopes[child_idx];
        
        // Find where this scope exists in the parent function's layout
        if (needed_scope_depth == parent_func->scope_depth) {
            // It's the parent's local scope (always at r15, use index -1)
            analysis.parent_location_indexes[child_idx] = -1;
        } else {
            // Find this scope in parent's priority_sorted_parent_scopes
            auto it = std::find(parent_func->priority_sorted_parent_scopes.begin(),
                               parent_func->priority_sorted_parent_scopes.end(),
                               needed_scope_depth);
            
            if (it == parent_func->priority_sorted_parent_scopes.end()) {
                throw std::runtime_error("Child function needs scope not available in parent");
            }
            
            // Store the INDEX in parent's priority list
            int parent_index = it - parent_func->priority_sorted_parent_scopes.begin();
            analysis.parent_location_indexes[child_idx] = parent_index;
        }
    }
}
```

#### Example: 3-Level Nested Functions
```javascript
function outer() {        // depth 1, priority_sorted_parent_scopes = []
  function middle() {     // depth 2, priority_sorted_parent_scopes = [1]  
    function inner() {    // depth 3, priority_sorted_parent_scopes = [2, 1] // Note: different order!
      // inner accesses middle's scope most frequently, outer's scope less frequently
    }
  }
}
```

**Static Analysis Results:**
- **`middle`**: r12 = index 0 (depth 1), r15 = local scope (depth 2)
- **`inner`**: needs [depth 2, depth 1] in that order

**Index Mapping Computation for `inner`:**
- Child index 0 needs depth 2 → Parent has depth 2 in local scope (r15) → `parent_location_indexes[0] = -1`
- Child index 1 needs depth 1 → Parent has depth 1 at index 0 (r12) → `parent_location_indexes[1] = 0`

**Result: `inner.parent_location_indexes = [-1, 0]`**

**This means when `middle` creates `inner` function instance:**
```assembly
; Use index mapping to copy scopes:
; child_index 0 → parent_index -1 (r15)
mov [r15 + func_offset + 24], r15     ; scopes[0] = from parent's local scope (r15)

; child_index 1 → parent_index 0 (r12)  
mov [r15 + func_offset + 32], r12     ; scopes[1] = from parent's index 0 (r12)
```

### Function Analysis Structure
```cpp
struct FunctionStaticAnalysis {
    std::vector<int> needed_parent_scopes;              // From priority_sorted_parent_scopes
    int num_registers_needed;                           // 0-3 (r12/r13/r14)
    bool needs_r12, needs_r13, needs_r14;              // Register usage flags
    size_t function_instance_offset;                   // From variable_offsets
    size_t function_instance_size;                     // Computed from captured scopes
    size_t local_scope_size;                           // From total_scope_frame_size
    
    // Critical mapping information computed during static analysis
    std::vector<int> parent_location_indexes;          // child_index → parent_index mapping
    // Example: parent_location_indexes[0] = 2 means child's scope[0] comes from parent's index 2
    //          parent_location_indexes[1] = -1 means child's scope[1] comes from parent's local scope (r15)
};
```

## 6. Implementation Plan

### Phase 0: Pre-Code-Generation Static Analysis (NEW - CRITICAL)
1. **Scope Mapping Computation**: For each function, compute how its needed scopes map to parent function locations
2. **Function Analysis Structure**: Populate `FunctionStaticAnalysis` with all mapping information
3. **Validation**: Ensure all needed scopes are available in parent functions
4. **Register Allocation**: Determine optimal register usage based on scope priorities

### Phase 1: Eliminate Runtime Calls
1. **Remove Scope Registry**: Delete all `__register_scope_address_for_depth` calls
2. **Direct Function Instance Creation**: Replace `initialize_function_variable` with direct memory writes
3. **Direct Memory Allocation**: Replace `malloc`/`free` with direct syscalls

### Phase 2: Implement Stack-Based Calling Convention  
1. **Function Call Generation**: Push captured scopes + parameters, then call
2. **Static Prologue Generation**: Generate optimal prologue based on scope analysis
3. **Static Epilogue Generation**: Generate matching epilogue with register restoration

### Phase 3: Scope Lifetime Management
1. **Reference Counting**: Only for captured scopes (non-captured scopes freed immediately)
2. **Escape Analysis**: Determine which scopes need to outlive their functions
3. **Direct Memory Management**: Use syscalls for heap allocation when needed

### Phase 4: Integration and Testing
1. **Function Address Patching**: Update existing system to work with new calling convention
2. **Comprehensive Testing**: Test nested closures, multiple captures, complex scenarios
3. **Performance Validation**: Verify zero runtime overhead for scope operations

## Expected Benefits

### Performance
- **Zero Runtime Overhead**: All scope operations are compile-time determined
- **Direct System Calls**: No libc dependency for memory management
- **Optimal Register Usage**: Only save/restore registers actually needed
- **Stack-Based Operations**: Fastest possible calling convention

### Correctness  
- **Proper Closure Semantics**: Captured scopes remain alive as long as function instances exist
- **No Global State**: All scope information passed explicitly via stack
- **Predictable Memory Usage**: All allocations statically determined

### Maintainability
- **Pure Machine Code**: True compiled language behavior
- **No Runtime Dependencies**: Eliminates complex runtime system
- **Static Analysis**: All complexity moved to compile time

## Success Metrics

1. **Zero Runtime Function Calls**: No `__get_scope_address`, `initialize_function_variable`, etc.
2. **Direct Memory Operations**: All memory allocation via syscalls
3. **Stack-Based Scope Passing**: All parent scopes passed via stack parameters
4. **Closure Test Pass**: `debug_nested_closure.gts` prints both `x=42` and `y=21`

This redesign transforms UltraScript from a hybrid runtime/compiled system into a true compiled language with zero-cost abstractions for closures and scope management.
