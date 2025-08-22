# Reference Counting Fix Summary

## Issue Description
The user correctly identified that newly created objects were starting with `ref_count = 2` instead of the expected `ref_count = 1`.

## Root Cause Analysis
The issue was in `Assignment::generate_code()` in `ast_codegen.cpp` around line 2923. When assigning a newly created object to a variable:

```cpp
let p = new Person();
```

The assignment logic was treating this as a **copy operation** instead of a **transfer operation**:

1. `new Person()` correctly created object with `ref_count = 1` 
2. Assignment code saw `value->result_type == DataType::CLASS_INSTANCE` 
3. It unconditionally called `gen.emit_ref_count_increment(0)` → `ref_count = 2`
4. Then stored the object pointer

## The Fix
Modified the assignment logic to differentiate between transfer and copy semantics:

```cpp
if (value->result_type == DataType::CLASS_INSTANCE) {
    // Check if this is a NewExpression - if so, don't increment (transfer ownership)
    auto* new_expr = dynamic_cast<NewExpression*>(value.get());
    if (new_expr) {
        // TRANSFER SEMANTICS: new object already has ref_count = 1, just transfer ownership
        // No reference count increment needed - this is a move operation
    } else {
        // COPY SEMANTICS: existing object assignment - increment ref count
        gen.emit_ref_count_increment(0);
    }
}
```

## Correct Behavior After Fix

- ✅ `let p = new Person()` → Transfer semantics, `ref_count = 1`
- ✅ `let p2 = p` → Copy semantics, `ref_count = 2` for both p and p2  
- ✅ `let p3 = p` → Copy semantics, `ref_count = 3` for all references

## Technical Details

**Before Fix:**
- New object creation: `ref_count = 1`
- Assignment increment: `ref_count = 2` ❌ (incorrect)

**After Fix:**
- New object creation: `ref_count = 1` 
- Transfer to variable: `ref_count = 1` ✅ (correct)
- Copy to another variable: `ref_count = 2` ✅ (correct)

This fix ensures proper C++/Rust-style move semantics for newly created objects while maintaining correct copy semantics for existing object references.
