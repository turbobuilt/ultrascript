# UltraScript Function System Implementation Plan

## Current Problems Identified

1. **Global Scope Registry Race Condition**:
   - `GlobalScopeAddressRegistry g_scope_registry` in runtime.cpp 
   - Functions like `__register_scope_address_for_depth`, `__get_scope_address_for_depth`
   - Causes segfaults when scope addresses are freed before function calls

2. **Runtime Memory Management**:
   - `initialize_function_variable` using malloc/free
   - Complex scope lifetime tracking with escape analysis
   - Runtime hash map lookups for scope addresses

3. **Incorrect Calling Convention**:
   - Functions don't receive their captured scopes via stack parameters
   - Reliance on global registry for scope access during execution

## UltraScript Function Calling Overhaul Implementation Plan

### 1. Goals & Requirements
- Eliminate all runtime scope registries and function call overhead.
- Implement stack-based closure system: all scope management and function calling are compile-time determined stack operations.
- Function instances store actual scope addresses, not depth IDs.
- Function calls push scopes and parameters in callee's expected order.
- Remove all uses of runtime calls (e.g., initialize_function_variable, __get_scope_address_for_depth).

### 2. Affected Components & Files
- Static analysis: LexicalScopeNode, FunctionDecl, FunctionStaticAnalysis (likely in analyze_code.py, analyze_final_machine_code.py, and C++ headers).
- Code generation: ast_codegen_new.cpp, ast_codegen_fixed_patterns.cpp, and related codegen files.
- Runtime: Remove global scope registry and related runtime calls (compiler.cpp, runtime.cpp, function_runtime.h).
- Tests: Update and add tests for nested closures, scope lifetime, and function instance correctness.

### 3. Implementation Phases

#### Phase 1: Static Analysis Enhancement
- Add FunctionStaticAnalysis struct to FunctionDecl.
- Compute priority_sorted_parent_scopes and parent_location_indexes for each function.
- Propagate static analysis results through parsing and codegen.

#### Phase 2: Code Generation Overhaul
- Update codegen to emit new function instance creation (struct layout, scope address copying).
- Update function call codegen to push scopes and parameters in correct order.
- Generate function prologue/epilogue to load scopes from stack and allocate/free local scope.

#### Phase 3: Remove Runtime Registry & Calls
- Delete all uses of global scope registry, runtime scope lookup, and function variable initialization.
- Replace with direct memory operations and static mappings.

#### Phase 4: Testing & Validation
- Update and expand tests for nested closures, scope lifetime, and function instance correctness.
- Validate with comprehensive_test.gts and other deep closure scenarios.

---

### Next Steps
1. Implement Phase 1: Update static analysis structures and propagation.
2. Implement Phase 2: Overhaul codegen for new function instance and calling convention.
3. Remove runtime registry and calls.
4. Update and validate tests.
## Implementation Plan

### Phase 1: Static Analysis Enhancement
**Goal**: Extend SimpleLexicalScopeAnalyzer to compute parent-child scope mappings

**Files to Modify**:
- `simple_lexical_scope.h` - Add FunctionStaticAnalysis struct
- `simple_lexical_scope.cpp` - Implement scope mapping computation
- `compiler.h` - Extend FunctionDecl with static analysis data

**New Data Structures**:
```cpp
struct FunctionStaticAnalysis {
    std::vector<int> needed_parent_scopes;              // From priority_sorted_parent_scopes  
    std::vector<int> parent_location_indexes;           // child_index → parent_index mapping
    int num_registers_needed;                           // 0-3 (r12/r13/r14)
    size_t function_instance_size;                     // Computed from captured scopes
    size_t local_scope_size;                           // From total_scope_frame_size
};
```

### Phase 2: Direct Assembly Generation  
**Goal**: Replace runtime calls with direct memory operations

**Files to Modify**:
- `ast_codegen.cpp` - Replace `initialize_hoisted_function_variable`
- `x86_codegen_v2.h` - Add direct function instance creation methods

**Changes**:
- Remove `initialize_function_variable` runtime calls
- Generate direct MOV instructions for function instance creation
- Use compile-time computed offsets and sizes

### Phase 3: Stack-Based Calling Convention
**Goal**: Implement new function call sequence with scope parameters

**Files to Modify**:
- `ast_codegen.cpp` - Modify function call generation
- `ast_codegen.cpp` - Modify function prologue generation

**New Calling Convention**:
```assembly
; Push captured scopes in reverse order (stack grows down)
push qword ptr [function_instance + 24 + (N-1)*8]  ; Last scope first  
push qword ptr [function_instance + 24 + (N-2)*8]  ; Second-to-last scope
; ...
push qword ptr [function_instance + 24]            ; First scope last

; Push parameters (if any)
push parameter_1
push parameter_0

; Call function
call function_address

; Clean up stack
add rsp, TOTAL_PUSHED_BYTES
```

### Phase 4: Function Prologue/Epilogue Generation
**Goal**: Generate optimized prologues based on static analysis

**Files to Modify**:
- `ast_codegen.cpp` - Generate static prologues
- `x86_codegen_v2.cpp` - Add prologue generation helpers

**New Prologue**:
```assembly  
function_name:
    push rbp
    mov rbp, rsp
    
    ; Save only needed registers (from static analysis)
    push r12    ; Only if function needs 1+ parent scopes
    push r13    ; Only if function needs 2+ parent scopes  
    push r14    ; Only if function needs 3+ parent scopes
    
    ; Load parent scopes from stack parameters
    mov r12, [rbp + 16]     ; Load scopes[0] 
    mov r13, [rbp + 24]     ; Load scopes[1] (if needed)
    mov r14, [rbp + 32]     ; Load scopes[2] (if needed)
    
    ; Allocate local scope using direct syscall
    mov rax, 9              ; sys_mmap
    mov rdi, 0              ; addr = NULL
    mov rsi, SCOPE_SIZE     ; length
    mov rdx, 3              ; prot = PROT_READ | PROT_WRITE  
    mov r10, 34             ; flags = MAP_PRIVATE | MAP_ANONYMOUS
    mov r8, -1              ; fd = -1
    mov r9, 0               ; offset = 0
    syscall
    
    mov r15, rax            ; Store local scope address
```

### Phase 5: Memory Management Overhaul
**Goal**: Replace malloc/free with direct syscalls

**Files to Modify**:
- `ast_codegen.cpp` - Replace memory allocation calls
- Remove dependency on runtime.cpp scope registry

**Changes**:
- Use mmap/munmap syscalls for scope allocation  
- Eliminate all malloc/free calls in generated code
- Remove global scope registry entirely

### Phase 6: Cleanup and Testing
**Goal**: Remove legacy runtime components and validate

**Files to Remove/Clean**:
- Remove `GlobalScopeAddressRegistry` from runtime.cpp
- Remove `initialize_function_variable` and related functions
- Clean up function_runtime.h legacy code

**Testing**:
- Verify `debug_nested_closure.gts` prints both x=42 and y=21
- Run comprehensive function tests
- Verify zero runtime overhead

## Expected Benefits

1. **Zero Runtime Overhead** - All scope operations are compile-time determined
2. **No Global State** - All scope information passed explicitly via stack  
3. **Proper Closure Semantics** - Captured scopes remain alive as function instances exist
4. **True Compiled Language** - No runtime dependencies or complex runtime system

## Success Metrics

1. ✅ **Zero Runtime Function Calls** - No `__get_scope_address`, `initialize_function_variable`
2. ✅ **Direct Memory Operations** - All memory allocation via syscalls
3. ✅ **Stack-Based Scope Passing** - All parent scopes passed via stack parameters  
4. ✅ **Closure Test Pass** - `debug_nested_closure.gts` prints both `x=42` and `y=21`

## Implementation Order

1. **Phase 1**: Static Analysis (Foundation)
2. **Phase 2**: Direct Assembly (Core Implementation) 
3. **Phase 3**: Stack Calling (Calling Convention)
4. **Phase 4**: Prologue Generation (Function Entry)
5. **Phase 5**: Memory Management (System Calls)
6. **Phase 6**: Cleanup (Remove Legacy)
