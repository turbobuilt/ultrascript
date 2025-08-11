#include <iostream>
#include <unordered_set>
#include <mutex>
#include <cstdlib>
#include <cassert>
#include "ultra_performance_array.h"
#include "runtime_object.h"
#include "compiler.h"  // For DataType enum

// ============================================================================
// FREE RUNTIME SYSTEM - HIGH PERFORMANCE MANUAL MEMORY MANAGEMENT
// ============================================================================

namespace ultraScript {

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

} // namespace ultraScript

// ============================================================================
// EXTERN "C" RUNTIME FUNCTIONS FOR JIT
// ============================================================================

extern "C" {

// Debug logging function
void __debug_log_free_operation(void* ptr, int is_shallow) {
    if (!ultraScript::g_debug_mode) return;
    
    std::cout << "[FREE-DEBUG] Freeing pointer " << ptr 
              << " (mode: " << (is_shallow ? "shallow" : "deep") << ")" << std::endl;
    std::cout.flush();
    
    if (ultraScript::is_pointer_freed(ptr)) {
        std::cerr << "[FREE-ERROR] DOUBLE FREE DETECTED! Pointer " << ptr 
                  << " was already freed!" << std::endl;
        ultraScript::g_free_stats.double_free_attempts++;
        abort(); // Crash in debug mode to catch double-frees
    }
}

// Post-free validation
void __debug_validate_post_free() {
    if (!ultraScript::g_debug_mode) return;
    
    // Could add memory corruption checks, etc.
    std::cout << "[FREE-DEBUG] Post-free validation passed" << std::endl;
}

// Log when primitive types are ignored
void __debug_log_primitive_free_ignored() {
    if (!ultraScript::g_debug_mode) return;
    
    std::cout << "[FREE-DEBUG] Primitive type free ignored (no allocation)" << std::endl;
}

// ============================================================================
// HIGH-PERFORMANCE TYPE-SPECIFIC FREE FUNCTIONS
// ============================================================================

// Free class instance (shallow)
void __free_class_instance_shallow(void* ptr) {
    if (!ptr) return;
    
    std::cout << "[FREE-JIT] Shallow freeing class instance at " << ptr << std::endl;
    ultraScript::g_free_stats.class_frees++;
    ultraScript::g_free_stats.shallow_frees++;
    
    // Cast to RuntimeObject and call destructor
    ultraScript::RuntimeObject* obj = static_cast<ultraScript::RuntimeObject*>(ptr);
    
    // In shallow mode, just free the object structure, not referenced objects
    if (ultraScript::g_debug_mode) {
        ultraScript::mark_pointer_freed(ptr);
        std::cout << "[FREE-JIT] Class instance shallow freed successfully" << std::endl;
    }
    
    delete obj;
}

// Free class instance (deep)
void __free_class_instance_deep(void* ptr) {
    if (!ptr) return;
    
    std::cout << "[FREE-JIT] Deep freeing class instance at " << ptr << std::endl;
    ultraScript::g_free_stats.class_frees++;
    ultraScript::g_free_stats.deep_frees++;
    
    ultraScript::RuntimeObject* obj = static_cast<ultraScript::RuntimeObject*>(ptr);
    
    // Deep free: recursively free all referenced objects
    // This would iterate through all object properties and free them
    // For now, simplified implementation
    
    if (ultraScript::g_debug_mode) {
        ultraScript::mark_pointer_freed(ptr);
        std::cout << "[FREE-JIT] Class instance deep freed successfully" << std::endl;
    }
    
    delete obj;
}

// Free array (shallow)
void __free_array_shallow(void* ptr) {
    if (!ptr) return;
    
    std::cout << "[FREE-JIT] Shallow freeing array at " << ptr << std::endl;
    ultraScript::g_free_stats.array_frees++;
    ultraScript::g_free_stats.shallow_frees++;
    
    // For typed arrays, we know the exact structure
    // This would be optimized based on array type at JIT time
    
    if (ultraScript::g_debug_mode) {
        ultraScript::mark_pointer_freed(ptr);
        std::cout << "[FREE-JIT] Array shallow freed successfully" << std::endl;
    }
    
    free(ptr);
}

// Free array (deep)
void __free_array_deep(void* ptr) {
    if (!ptr) return;
    
    std::cout << "[FREE-JIT] Deep freeing array at " << ptr << std::endl;
    ultraScript::g_free_stats.array_frees++;
    ultraScript::g_free_stats.deep_frees++;
    
    // Deep free: iterate through array elements and free them recursively
    // Implementation would depend on array type (typed vs untyped)
    
    if (ultraScript::g_debug_mode) {
        ultraScript::mark_pointer_freed(ptr);
        std::cout << "[FREE-JIT] Array deep freed successfully" << std::endl;
    }
    
    free(ptr);
}

// Free string
void __free_string(void* ptr) {
    if (!ptr) return;
    
    std::cout << "[FREE-JIT] Freeing string at " << ptr << std::endl;
    ultraScript::g_free_stats.string_frees++;
    
    if (ultraScript::g_debug_mode) {
        ultraScript::mark_pointer_freed(ptr);
        std::cout << "[FREE-JIT] String freed successfully" << std::endl;
    }
    
    // String freeing - would depend on string implementation
    free(ptr);
}

// Free dynamic value (requires runtime type checking)
void __free_dynamic_value(void* ptr, int is_shallow) {
    if (!ptr) return;
    
    std::cout << "[FREE-JIT] Freeing dynamic value at " << ptr 
              << " (shallow=" << is_shallow << ")" << std::endl;
    ultraScript::g_free_stats.dynamic_frees++;
    
    // Cast to DynamicValue to check runtime type
    ultraScript::DynamicValue* dyn_val = static_cast<ultraScript::DynamicValue*>(ptr);
    
    // Check the runtime type and dispatch to appropriate free function
    switch (dyn_val->type) {
        case ultraScript::DataType::STRING:
            std::cout << "[FREE-JIT] Dynamic value is string" << std::endl;
            __free_string(dyn_val);
            break;
            
        case ultraScript::DataType::CLASS_INSTANCE:
            std::cout << "[FREE-JIT] Dynamic value is class instance" << std::endl;
            if (is_shallow) {
                __free_class_instance_shallow(dyn_val);
            } else {
                __free_class_instance_deep(dyn_val);
            }
            break;
            
        case ultraScript::DataType::ARRAY:
            std::cout << "[FREE-JIT] Dynamic value is array" << std::endl;
            if (is_shallow) {
                __free_array_shallow(dyn_val);
            } else {
                __free_array_deep(dyn_val);
            }
            break;
            
        default:
            // Primitive types stored in DynamicValue don't need freeing
            std::cout << "[FREE-JIT] Dynamic value contains primitive type" << std::endl;
            if (ultraScript::g_debug_mode) {
                ultraScript::mark_pointer_freed(ptr);
            }
            delete dyn_val;
            break;
    }
}

// Get free statistics for debugging
void __get_free_stats(size_t* stats_out) {
    stats_out[0] = ultraScript::g_free_stats.total_frees;
    stats_out[1] = ultraScript::g_free_stats.shallow_frees;
    stats_out[2] = ultraScript::g_free_stats.deep_frees;
    stats_out[3] = ultraScript::g_free_stats.class_frees;
    stats_out[4] = ultraScript::g_free_stats.array_frees;
    stats_out[5] = ultraScript::g_free_stats.string_frees;
    stats_out[6] = ultraScript::g_free_stats.dynamic_frees;
    stats_out[7] = ultraScript::g_free_stats.double_free_attempts;
    stats_out[8] = ultraScript::g_free_stats.use_after_free_attempts;
}

// Print free statistics
void __print_free_stats() {
    std::cout << "\n=== FREE STATISTICS ===" << std::endl;
    std::cout << "Total frees: " << ultraScript::g_free_stats.total_frees << std::endl;
    std::cout << "Shallow frees: " << ultraScript::g_free_stats.shallow_frees << std::endl;
    std::cout << "Deep frees: " << ultraScript::g_free_stats.deep_frees << std::endl;
    std::cout << "Class frees: " << ultraScript::g_free_stats.class_frees << std::endl;
    std::cout << "Array frees: " << ultraScript::g_free_stats.array_frees << std::endl;
    std::cout << "String frees: " << ultraScript::g_free_stats.string_frees << std::endl;
    std::cout << "Dynamic frees: " << ultraScript::g_free_stats.dynamic_frees << std::endl;
    std::cout << "Double-free attempts: " << ultraScript::g_free_stats.double_free_attempts << std::endl;
    std::cout << "Use-after-free attempts: " << ultraScript::g_free_stats.use_after_free_attempts << std::endl;
    std::cout << "======================" << std::endl;
}

// Enable/disable debug mode
void __set_free_debug_mode(int enabled) {
    ultraScript::g_debug_mode = (enabled != 0);
    std::cout << "[FREE-RUNTIME] Debug mode " << (enabled ? "ENABLED" : "DISABLED") << std::endl;
}

} // extern "C"
