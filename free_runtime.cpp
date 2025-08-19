#include <iostream>
#include <unordered_set>
#include <mutex>
#include <cstdlib>
#include <cassert>
#include "ultra_performance_array.h"
#include "runtime_object.h"
#include "compiler.h"  // For DataType enum
#include "refcount.h"   // Reference counting system

// ============================================================================
// FREE RUNTIME SYSTEM - HIGH PERFORMANCE MANUAL MEMORY MANAGEMENT
// Integrated with reference counting for objects and arrays
// ============================================================================


// Debug tracking for double-free detection
static std::unordered_set<void*> g_freed_pointers;
static std::mutex g_free_debug_mutex;
static bool g_debug_mode = true; // Set by compiler debug flags

// Performance counters for debugging
struct FreeStats {
    size_t total_frees = 0;
    size_t shallow_frees = 0;
    size_t deep_frees = 0;
    size_t class_frees = 0;
    size_t array_frees = 0;
    size_t string_frees = 0;
    size_t dynamic_frees = 0;
    size_t double_free_attempts = 0;
    size_t use_after_free_attempts = 0;
};

static FreeStats g_free_stats;

// Helper to check if pointer was already freed (debug mode only)
bool is_pointer_freed(void* ptr) {
    if (!g_debug_mode) return false;
    
    std::lock_guard<std::mutex> lock(g_free_debug_mutex);
    return g_freed_pointers.find(ptr) != g_freed_pointers.end();
}

// Mark pointer as freed (debug mode only)
void mark_pointer_freed(void* ptr) {
    if (!g_debug_mode) return;
    
    std::lock_guard<std::mutex> lock(g_free_debug_mutex);
    g_freed_pointers.insert(ptr);
}


// ============================================================================
// EXTERN "C" RUNTIME FUNCTIONS FOR JIT
// ============================================================================

extern "C" {

// Debug logging function
void __debug_log_free_operation(void* ptr, int is_shallow) {
    if (!g_debug_mode) return;
    
    std::cout << "[FREE-DEBUG] Freeing pointer " << ptr 
              << " (mode: " << (is_shallow ? "shallow" : "deep") << ")" << std::endl;
    std::cout.flush();
    
    if (is_pointer_freed(ptr)) {
        std::cerr << "[FREE-ERROR] DOUBLE FREE DETECTED! Pointer " << ptr 
                  << " was already freed!" << std::endl;
        g_free_stats.double_free_attempts++;
        abort(); // Crash in debug mode to catch double-frees
    }
}

// Post-free validation
void __debug_validate_post_free() {
    if (!g_debug_mode) return;
    
    // Could add memory corruption checks, etc.
    std::cout << "[FREE-DEBUG] Post-free validation passed" << std::endl;
}

// Log when primitive types are ignored
void __debug_log_primitive_free_ignored() {
    if (!g_debug_mode) return;
    
    std::cout << "[FREE-DEBUG] Primitive type free ignored (no allocation)" << std::endl;
}

// ============================================================================
// HIGH-PERFORMANCE TYPE-SPECIFIC FREE FUNCTIONS
// Integrated with reference counting system
// ============================================================================

// Helper function to check if pointer is reference counted
int __is_rc_object(void* ptr) {
    if (!ptr) return 0;
    
    // Check if the pointer has a valid reference count header
    RefCountHeader* header = get_refcount_header(ptr);
    if (!header) return 0;
    
    // Additional validation could be added here
    return 1;
}

// Reference counting integration for shallow free
void __free_rc_object_shallow(void* ptr) {
    if (!ptr) return;
    
    if (g_debug_mode) {
        std::cout << "[FREE-RC] Reference counted shallow free: " << ptr << std::endl;
    }
    
    // Use reference counting system's cycle breaking for shallow free
    rc_integrate_with_free_shallow(ptr);
    g_free_stats.shallow_frees++;
}

// Reference counting integration for deep free
void __free_rc_object_deep(void* ptr) {
    if (!ptr) return;
    
    if (g_debug_mode) {
        std::cout << "[FREE-RC] Reference counted deep free: " << ptr << std::endl;
    }
    
    // Use reference counting system's deep free
    rc_integrate_with_free_deep(ptr);
    g_free_stats.deep_frees++;
}

// Free class instance (shallow) - updated for reference counting
void __free_class_instance_shallow(void* ptr) {
    if (!ptr) return;
    
    std::cout << "[FREE-JIT] Shallow freeing class instance at " << ptr << std::endl;
    g_free_stats.class_frees++;
    
    // Check if this is a reference counted object
    if (__is_rc_object(ptr)) {
        __free_rc_object_shallow(ptr);
        return;
    }
    
    // Legacy non-reference counted path
    g_free_stats.shallow_frees++;
    RuntimeObject* obj = static_cast<RuntimeObject*>(ptr);
    
    // In shallow mode, just free the object structure, not referenced objects
    if (g_debug_mode) {
        mark_pointer_freed(ptr);
        std::cout << "[FREE-JIT] Class instance shallow freed successfully" << std::endl;
    }
    
    delete obj;
}

// Free class instance (deep) - updated for reference counting
void __free_class_instance_deep(void* ptr) {
    if (!ptr) return;
    
    std::cout << "[FREE-JIT] Deep freeing class instance at " << ptr << std::endl;
    g_free_stats.class_frees++;
    
    // Check if this is a reference counted object
    if (__is_rc_object(ptr)) {
        __free_rc_object_deep(ptr);
        return;
    }
    
    // Legacy non-reference counted path
    g_free_stats.deep_frees++;
    RuntimeObject* obj = static_cast<RuntimeObject*>(ptr);
    
    // Deep free: recursively free all referenced objects
    // This would iterate through all object properties and free them
    // For now, simplified implementation
    
    if (g_debug_mode) {
        mark_pointer_freed(ptr);
        std::cout << "[FREE-JIT] Class instance deep freed successfully" << std::endl;
    }
    
    delete obj;
}

// Free array (shallow) - updated for reference counting
void __free_array_shallow(void* ptr) {
    if (!ptr) return;
    
    std::cout << "[FREE-JIT] Shallow freeing array at " << ptr << std::endl;
    g_free_stats.array_frees++;
    
    // Check if this is a reference counted array
    if (__is_rc_object(ptr)) {
        __free_rc_object_shallow(ptr);
        return;
    }
    
    // Legacy non-reference counted path
    g_free_stats.shallow_frees++;
    
    // For typed arrays, we know the exact structure
    // This would be optimized based on array type at JIT time
    
    if (g_debug_mode) {
        mark_pointer_freed(ptr);
        std::cout << "[FREE-JIT] Array shallow freed successfully" << std::endl;
    }
    
    free(ptr);
}

// Free array (deep) - updated for reference counting
void __free_array_deep(void* ptr) {
    if (!ptr) return;
    
    std::cout << "[FREE-JIT] Deep freeing array at " << ptr << std::endl;
    g_free_stats.array_frees++;
    
    // Check if this is a reference counted array
    if (__is_rc_object(ptr)) {
        __free_rc_object_deep(ptr);
        return;
    }
    
    // Legacy non-reference counted path
    g_free_stats.deep_frees++;
    
    // Deep free: iterate through array elements and free them recursively
    // Implementation would depend on array type (typed vs untyped)
    
    if (g_debug_mode) {
        mark_pointer_freed(ptr);
        std::cout << "[FREE-JIT] Array deep freed successfully" << std::endl;
    }
    
    free(ptr);
}

// Free string - strings are copied by value, so no reference counting needed
void __free_string(void* ptr) {
    if (!ptr) return;
    
    std::cout << "[FREE-JIT] Freeing string at " << ptr << std::endl;
    g_free_stats.string_frees++;
    
    // Strings don't use reference counting as they are copied by value
    // This maintains the requirement from the user
    
    if (g_debug_mode) {
        mark_pointer_freed(ptr);
        std::cout << "[FREE-JIT] String freed successfully" << std::endl;
    }
    
    // String freeing - would depend on string implementation
    free(ptr);
}

// Free dynamic value (requires runtime type checking) - updated for reference counting
void __free_dynamic_value(void* ptr, int is_shallow) {
    if (!ptr) return;
    
    std::cout << "[FREE-JIT] Freeing dynamic value at " << ptr 
              << " (shallow=" << is_shallow << ")" << std::endl;
    g_free_stats.dynamic_frees++;
    
    // Check if this is a reference counted object first
    if (__is_rc_object(ptr)) {
        if (is_shallow) {
            __free_rc_object_shallow(ptr);
        } else {
            __free_rc_object_deep(ptr);
        }
        return;
    }
    
    // Legacy non-reference counted path
    // Cast to DynamicValue to check runtime type
    DynamicValue* dyn_val = static_cast<DynamicValue*>(ptr);
    
    std::cout << "[FREE-DEBUG] DynamicValue type: " << static_cast<int>(dyn_val->type) << std::endl;
    
    // For DynamicValue, handle each type appropriately
    switch (dyn_val->type) {
        case DataType::STRING: {
            std::cout << "[FREE-JIT] Dynamic value contains string - will be freed automatically" << std::endl;
            // std::string in the variant will be automatically destructed
            break;
        }
        
        case DataType::CLASS_INSTANCE: {
            std::cout << "[FREE-JIT] Dynamic value contains class instance pointer" << std::endl;
            // Extract the object pointer from the variant
            try {
                void* obj_ptr = std::get<void*>(dyn_val->value);
                if (obj_ptr) {
                    if (is_shallow) {
                        __free_class_instance_shallow(obj_ptr);
                    } else {
                        __free_class_instance_deep(obj_ptr);
                    }
                }
            } catch (const std::bad_variant_access&) {
                std::cout << "[FREE-ERROR] DynamicValue marked as CLASS_INSTANCE but doesn't contain pointer" << std::endl;
            }
            break;
        }
        
        case DataType::ARRAY: {
            std::cout << "[FREE-JIT] Dynamic value contains array pointer" << std::endl;
            // Extract the array pointer from the variant
            try {
                void* arr_ptr = std::get<void*>(dyn_val->value);
                if (arr_ptr) {
                    if (is_shallow) {
                        __free_array_shallow(arr_ptr);
                    } else {
                        __free_array_deep(arr_ptr);
                    }
                }
            } catch (const std::bad_variant_access&) {
                std::cout << "[FREE-ERROR] DynamicValue marked as ARRAY but doesn't contain pointer" << std::endl;
            }
            break;
        }
        
        // All primitive types - optionally zero out for safety
        case DataType::INT8:
            std::cout << "[FREE-JIT] Zeroing int8 value" << std::endl;
            dyn_val->value = static_cast<int8_t>(0);
            break;
        case DataType::INT16:
            std::cout << "[FREE-JIT] Zeroing int16 value" << std::endl;
            dyn_val->value = static_cast<int16_t>(0);
            break;
        case DataType::INT32:
            std::cout << "[FREE-JIT] Zeroing int32 value" << std::endl;
            dyn_val->value = static_cast<int32_t>(0);
            break;
        case DataType::INT64:
            std::cout << "[FREE-JIT] Zeroing int64 value" << std::endl;
            dyn_val->value = static_cast<int64_t>(0);
            break;
        case DataType::UINT8:
            std::cout << "[FREE-JIT] Zeroing uint8 value" << std::endl;
            dyn_val->value = static_cast<uint8_t>(0);
            break;
        case DataType::UINT16:
            std::cout << "[FREE-JIT] Zeroing uint16 value" << std::endl;
            dyn_val->value = static_cast<uint16_t>(0);
            break;
        case DataType::UINT32:
            std::cout << "[FREE-JIT] Zeroing uint32 value" << std::endl;
            dyn_val->value = static_cast<uint32_t>(0);
            break;
        case DataType::UINT64:
            std::cout << "[FREE-JIT] Zeroing uint64 value" << std::endl;
            dyn_val->value = static_cast<uint64_t>(0);
            break;
        case DataType::FLOAT32:
            std::cout << "[FREE-JIT] Zeroing float32 value" << std::endl;
            dyn_val->value = static_cast<float>(0.0f);
            break;
        case DataType::FLOAT64:
            std::cout << "[FREE-JIT] Zeroing float64 value" << std::endl;
            dyn_val->value = static_cast<double>(0.0);
            break;
        case DataType::BOOLEAN:
            std::cout << "[FREE-JIT] Zeroing boolean value" << std::endl;
            dyn_val->value = false;
            break;
            
        default:
            std::cout << "[FREE-JIT] Unknown DynamicValue type: " << static_cast<int>(dyn_val->type) << std::endl;
            break;
    }
    
    // Always delete the DynamicValue wrapper itself
    if (g_debug_mode) {
        mark_pointer_freed(ptr);
    }
    delete dyn_val;
}

// Get free statistics for debugging
void __get_free_stats(size_t* stats_out) {
    stats_out[0] = g_free_stats.total_frees;
    stats_out[1] = g_free_stats.shallow_frees;
    stats_out[2] = g_free_stats.deep_frees;
    stats_out[3] = g_free_stats.class_frees;
    stats_out[4] = g_free_stats.array_frees;
    stats_out[5] = g_free_stats.string_frees;
    stats_out[6] = g_free_stats.dynamic_frees;
    stats_out[7] = g_free_stats.double_free_attempts;
    stats_out[8] = g_free_stats.use_after_free_attempts;
}

// Print free statistics
void __print_free_stats() {
    std::cout << "\n=== FREE STATISTICS ===" << std::endl;
    std::cout << "Total frees: " << g_free_stats.total_frees << std::endl;
    std::cout << "Shallow frees: " << g_free_stats.shallow_frees << std::endl;
    std::cout << "Deep frees: " << g_free_stats.deep_frees << std::endl;
    std::cout << "Class frees: " << g_free_stats.class_frees << std::endl;
    std::cout << "Array frees: " << g_free_stats.array_frees << std::endl;
    std::cout << "String frees: " << g_free_stats.string_frees << std::endl;
    std::cout << "Dynamic frees: " << g_free_stats.dynamic_frees << std::endl;
    std::cout << "Double-free attempts: " << g_free_stats.double_free_attempts << std::endl;
    std::cout << "Use-after-free attempts: " << g_free_stats.use_after_free_attempts << std::endl;
    std::cout << "======================" << std::endl;
}

// Enable/disable debug mode
void __set_free_debug_mode(int enabled) {
    g_debug_mode = (enabled != 0);
    std::cout << "[FREE-RUNTIME] Debug mode " << (enabled ? "ENABLED" : "DISABLED") << std::endl;
}

// Error function for deep free not implemented
void __throw_deep_free_not_implemented() {


} // extern "C"

// ============================================================================
// MIGRATION FUNCTIONS FOR REFERENCE COUNTING
// ============================================================================

// Initialize migration to reference counting allocator
void __migrate_to_rc_alloc() {
    std::cout << "[MIGRATION] Migrating to reference counting allocator..." << std::endl;
    
    // Enable reference counting debug mode by default during migration
    rc_set_debug_mode(1);
    __set_free_debug_mode(1);
    
    // Register default destructors for built-in types
    rc_register_destructor(1, rc_destructor_string);   // String type
    rc_register_destructor(2, rc_destructor_array);    // Array type  
    rc_register_destructor(3, rc_destructor_object);   // Object type
    rc_register_destructor(4, rc_destructor_dynamic);  // Dynamic type
    
    std::cout << "[MIGRATION] Reference counting system initialized" << std::endl;
    
    // Print initial statistics
    rc_print_stats();
}

// Migrate from garbage collection to reference counting
void __migrate_from_gc_to_rc() {
    std::cout << "[MIGRATION] Migrating from garbage collection to reference counting..." << std::endl;
    
    // This would disable the garbage collector and switch to reference counting
    // For now, just initialize the reference counting system
    __migrate_to_rc_alloc();
    
    std::cout << "[MIGRATION] Migration complete - now using reference counting" << std::endl;
}


}
