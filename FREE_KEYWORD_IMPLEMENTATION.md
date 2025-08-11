# UltraScript Free Keyword Implementation

## Overview

The `free` keyword provides high-performance manual memory management in UltraScript with C++-level performance through JIT optimization.

## Features

### 🚀 JIT-Optimized Performance
- **Type-aware code generation**: Different optimized paths for classes, arrays, strings, and dynamic values
- **Direct machine code**: No runtime interpretation overhead  
- **Inline debugging**: Debug information compiled away in production mode
- **Static analysis**: Compile-time optimizations based on known types

### 🛡️ Debug Safety Features
- **Double-free detection**: Runtime tracking prevents double-free errors
- **Use-after-free detection**: Debug mode catches access to freed memory
- **Memory leak detection**: Comprehensive allocation/deallocation tracking
- **Stack trace logging**: Full allocation/deallocation history in debug mode

### 📊 Performance Monitoring
- **Real-time statistics**: Track frees by type and mode
- **Memory usage tracking**: Monitor allocation patterns
- **Performance counters**: Measure free operation overhead

## Syntax

### Basic Usage
```ultraScript
// Deep free (default) - frees object and all children
var obj = new MyClass();
free obj;

// Shallow free - frees only the container, not contents
var arr = [new Obj1(), new Obj2()];
free shallow arr;  // Memory leak! Objects still allocated
```

### Type-Specific Behavior

#### Objects (Class Instances)
```ultraScript
class Person {
    name: string;
    age: int64;
}

var person = new Person{name: "Alice", age: 30};
free person;  // Calls destructor, frees all properties
```

#### Arrays
```ultraScript
// Typed arrays - ultra-optimized freeing
var typed_arr: [int64] = [1, 2, 3, 4, 5];
free typed_arr;  // Direct memory deallocation

// Dynamic arrays - runtime type checking
var dynamic_arr = [new Obj(), "string", 42];
free dynamic_arr;  // Recursively frees each element
```

#### Strings
```ultraScript
var str = "Hello, World!";
free str;  // Frees string buffer
```

#### Dynamic Values
```ultraScript
var dynamic = 42;           // Initially number
dynamic = new MyClass();    // Now holds object
free dynamic;               // Runtime type detection, frees object
```

## JIT Code Generation

### Type-Optimized Paths

The compiler generates different machine code based on the compile-time type:

#### Known Class Type
```asm
; Compile-time known: MyClass*
mov rdi, rax                    ; Load pointer
call __free_class_instance_deep  ; Direct optimized call
```

#### Known Array Type  
```asm
; Compile-time known: int64[]
mov rdi, rax                    ; Load pointer
mov rsi, 8                      ; Element size (compile-time constant)
call __free_typed_array_int64   ; Ultra-optimized typed array free
```

#### Dynamic Type (Runtime Dispatch)
```asm
; Runtime type: DynamicValue
mov rdi, rax                    ; Load pointer
mov rsi, 0                      ; Deep free flag
call __free_dynamic_value       ; Runtime type checking
```

## Debug Mode Features

### Comprehensive Logging
```
[FREE-DEBUG] Freeing pointer 0x7fff12345678 (mode: deep)
[FREE-JIT] Deep freeing class instance at 0x7fff12345678
[FREE-JIT] Class instance deep freed successfully
[FREE-DEBUG] Post-free validation passed
```

### Error Detection
```
[FREE-ERROR] DOUBLE FREE DETECTED! Pointer 0x7fff12345678 was already freed!
[ABORT] Program terminated to prevent corruption
```

### Performance Statistics
```
=== FREE STATISTICS ===
Total frees: 1547
Shallow frees: 23
Deep frees: 1524
Class frees: 892
Array frees: 445
String frees: 167
Dynamic frees: 43
Double-free attempts: 0
Use-after-free attempts: 0
======================
```

## Runtime Functions

### Core Free Functions
- `__free_class_instance_shallow(ptr)` - Free object structure only
- `__free_class_instance_deep(ptr)` - Recursive object freeing
- `__free_array_shallow(ptr)` - Free array container only
- `__free_array_deep(ptr)` - Free array and all elements
- `__free_string(ptr)` - Free string buffer
- `__free_dynamic_value(ptr, shallow)` - Runtime type dispatch

### Debug Functions
- `__debug_log_free_operation(ptr, shallow)` - Log free operations
- `__debug_validate_post_free()` - Post-free validation
- `__set_free_debug_mode(enabled)` - Enable/disable debug mode
- `__print_free_stats()` - Print performance statistics

## Performance Characteristics

### Production Mode
- **Zero overhead for primitives**: Compile-time elimination
- **Direct syscalls**: Optimal memory deallocation paths
- **No debugging code**: Debug calls optimized away
- **SIMD optimizations**: Vectorized bulk operations where applicable

### Debug Mode  
- **Comprehensive tracking**: All allocations/deallocations logged
- **Safety checks**: Runtime validation of all operations
- **Statistical collection**: Performance monitoring
- **Stack trace capture**: Full debugging information

## Integration with Garbage Collector

The `free` keyword is designed to work alongside the optional garbage collector:

```ultraScript
// Manual management when you need control
var critical_buffer = new LargeBuffer(1_000_000);
// ... use buffer ...
free critical_buffer;  // Immediate deallocation

// Automatic management for convenience
var temp_objects = generate_temp_data();
// No explicit free - GC handles it
```

## Testing

Run the comprehensive test suite:
```bash
./ultraScript test_free_keyword.gts
```

This tests all aspects:
- Basic object freeing
- Deep vs shallow array freeing  
- String memory management
- Dynamic value handling
- Nested data structures
- Error detection
- Performance statistics

## Building

The free runtime is automatically included in the build:
```bash
make clean && make debug  # Include debug symbols
make                      # Production build
```

## Best Practices

### ✅ Good Practices
```ultraScript
// Free large objects immediately when done
var large_buffer = new Buffer(1_000_000);
process_data(large_buffer);
free large_buffer;  // Free immediately

// Use shallow free carefully with good documentation
var cache = build_cache();  // Objects still needed elsewhere
free shallow cache;  // Just free cache structure
```

### ❌ Avoid These Patterns
```ultraScript
// DON'T: Double free
var obj = new MyClass();
free obj;
free obj;  // ERROR: Double free detected!

// DON'T: Use after free
var obj = new MyClass();
free obj;
console.log(obj.name);  // ERROR: Use after free!
```

## Future Enhancements

- **Escape analysis**: Automatic stack allocation for short-lived objects
- **Region-based memory**: Bulk deallocation of related objects
- **Memory pools**: Object-type-specific allocation pools
- **Weak references**: Non-owning pointers that become null when target is freed
