# UltraScript Class System - Assembly Generation Demonstration

This shows the actual assembly code that would be generated for each property access pattern.

## 1. ULTRA-FAST Path: `bob.name`

**Source Code:**
```javascript
class Person { name: string; }
var bob = new Person();
bob.name  // ← This access pattern
```

**AST Analysis:**
- Compiler sees `bob.name` where `name` is a known class property
- Property `name` has compile-time known offset: 0
- Property type: STRING (pointer)

**Generated Assembly:**
```assembly
; ULTRA-FAST property access - bob.name
; INPUT: RAX = bob object pointer
; DEBUG: Direct offset access for property 'name' at offset 0

    ; Optional debug output (only in debug builds)
    push rax
    lea rdi, [debug_msg_ultrafast]     ; "[ULTRA-FAST ACCESS] name at offset 0"
    call puts
    pop rax

    ; Direct memory access - ZERO runtime lookup cost!
    mov rax, [rax + 12 + 0]            ; Skip object header (12 bytes) + property offset (0)
    ; Result: RAX contains the string pointer
```

## 2. DYNAMIC Path: `bob[propName]`

**Source Code:**
```javascript
var propName = "age";
bob[propName]  // ← This access pattern
```

**Runtime Analysis:**
- Property name is dynamic (runtime string)
- Must hash the property name and lookup in class metadata
- If found in class, use the offset; otherwise check dynamic properties

**Generated Assembly:**
```assembly
; DYNAMIC property access - bob[propName]
; INPUT: RAX = bob object pointer, RDX = property name string

    ; Optional debug output
    push rax
    push rdx
    lea rdi, [debug_msg_dynamic]       ; "[DYNAMIC ACCESS] hash lookup"
    call puts
    pop rdx
    pop rax

    ; Hash the property name
    mov rdi, rdx                       ; RDI = property name string
    call hash_property_name            ; Result in RAX
    mov r8, rax                        ; R8 = property hash

    ; Get object type ID and lookup class metadata
    mov rdx, [rax]                     ; Load object header
    mov edx, [rdx]                     ; Load type_id from header
    mov rdx, [type_id_to_metadata + rdx*8]  ; Get ClassMetadata pointer

    ; Search hash table in class metadata
    mov rdi, rdx                       ; RDI = ClassMetadata*
    mov rsi, r8                        ; RSI = property hash
    call find_property_by_hash         ; Result in RAX (PropertyDescriptor* or nullptr)

    test rax, rax
    jz check_dynamic_properties        ; If not found in class, check dynamic properties

    ; Found in class - get offset and access
    mov edx, [rax + offsetof(PropertyDescriptor, offset)]  ; Get property offset
    mov rax, [rax + 12 + rdx]          ; Access property at offset
    jmp done

check_dynamic_properties:
    ; Property not in class - check per-object dynamic properties
    mov rdi, rax                       ; RDI = object pointer
    mov rsi, r8                        ; RSI = property hash
    call get_dynamic_property_by_hash  ; Slower path

done:
    ; Result in RAX
```

## 3. DYNAMIC_DICT Path: `bob.xyz = 123`

**Source Code:**
```javascript
bob.xyz = 123;  // ← Property not defined in class
```

**Runtime Analysis:**
- Property `xyz` not found in class definition
- Must store in per-object dynamic properties hash table
- Lazy initialization of dynamic properties map

**Generated Assembly:**
```assembly
; DYNAMIC_DICT property assignment - bob.xyz = 123
; INPUT: RAX = bob object pointer, RDX = value, R8 = property hash

    ; Optional debug output
    push rax
    lea rdi, [debug_msg_dynamic_dict]  ; "[DYNAMIC_DICT SET] xyz property"
    call puts
    pop rax

    ; First try to find in class (will fail for 'xyz')
    call find_property_by_hash_fast
    test rax, rax
    jnz found_in_class                 ; Unlikely for new properties

    ; Not found in class - use dynamic properties
    mov rdi, rax                       ; RDI = object pointer
    mov rsi, r8                        ; RSI = property hash
    mov rdx, rdx                       ; RDX = value
    call set_dynamic_property_by_hash

    ; This function will:
    ; 1. Check if object->dynamic_properties is null
    ; 2. If null, allocate new hash table (lazy init)
    ; 3. Insert hash->value mapping
    ; 4. Return success

found_in_class:
    ; Handle case where property exists in class (faster path)
    ; ... (similar to dynamic path above)
```

## Performance Comparison

| Access Pattern | Runtime Cost | Description |
|---------------|--------------|-------------|
| `bob.name` | **0 cycles** | Direct memory access with compile-time offset |
| `bob[propName]` | **~10-20 cycles** | Hash calculation + hash table lookup + memory access |
| `bob.xyz = value` | **~20-50 cycles** | Hash calculation + class lookup (miss) + dynamic table access/creation |

## Key Optimizations

1. **Compile-time property resolution** - When AST knows the property name, emit direct offset access
2. **Efficient hashing** - FNV-1a hash for fast property name hashing
3. **Lazy dynamic properties** - Only allocate per-object hash table when needed
4. **Cache-friendly layout** - Properties stored contiguously with proper alignment
5. **Type-specific codegen** - Different assembly for different property types

The debug output you saw demonstrates the decision-making process that determines which of these paths gets taken for each property access pattern.
