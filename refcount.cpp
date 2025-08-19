#include "refcount.h"
#include <iostream>
#include <unordered_map>
#include <mutex>
#include <cassert>
#include <cstring>

// ============================================================================
// HIGH PERFORMANCE REFERENCE COUNTING IMPLEMENTATION
// UltraScript equivalent of std::shared_ptr with manual optimizations
// ============================================================================

// Global statistics
static RefCountStats g_rc_stats = {0};
static std::mutex g_rc_stats_mutex;
static bool g_rc_debug_mode = false;

// Type destructor registry
static std::unordered_map<uint32_t, void (*)(void*)> g_destructor_registry;
static std::mutex g_destructor_registry_mutex;

// Debug tracking
#if REFCOUNT_DEBUG_MODE
static std::unordered_map<void*, RefCountHeader*> g_active_objects;
static std::mutex g_debug_mutex;
#endif

// ============================================================================
// ALLOCATION FUNCTIONS
// ============================================================================

extern "C" void* rc_alloc(size_t size, uint32_t type_id, void (*destructor)(void*)) {
    // Allocate memory for header + object with cache line alignment
    size_t total_size = sizeof(RefCountHeader) + size;
    
    #if REFCOUNT_CACHE_ALIGNED
    // Align to 64-byte cache line boundary for optimal performance
    void* raw_memory = aligned_alloc(64, (total_size + 63) & ~63);
    #else
    void* raw_memory = malloc(total_size);
    #endif
    
    if (!raw_memory) {
        return nullptr;
    }
    
    // Initialize header with placement new for proper atomic initialization
    RefCountHeader* header = new (raw_memory) RefCountHeader(1, type_id, size, destructor);
    
    // Get user pointer
    void* user_ptr = get_user_pointer(header);
    
    // Update statistics
    {
        std::lock_guard<std::mutex> lock(g_rc_stats_mutex);
        g_rc_stats.total_allocations++;
        g_rc_stats.current_objects++;
        if (g_rc_stats.current_objects > g_rc_stats.peak_objects) {
            g_rc_stats.peak_objects = g_rc_stats.current_objects;
        }
    }
    
    #if REFCOUNT_DEBUG_MODE
    if (g_rc_debug_mode) {
        std::lock_guard<std::mutex> lock(g_debug_mutex);
        g_active_objects[user_ptr] = header;
        std::cout << "[REFCOUNT] Allocated object " << user_ptr 
                  << " (type=" << type_id << ", size=" << size << ")" << std::endl;
    }
    #endif
    
    return user_ptr;
}

extern "C" void* rc_alloc_array(size_t element_size, size_t count, uint32_t type_id, void (*destructor)(void*)) {
    size_t total_size = element_size * count;
    
    // Arrays get a special destructor that handles element destruction
    void* ptr = rc_alloc(total_size, type_id | 0x80000000, destructor); // Set array flag in type_id
    
    if (ptr && g_rc_debug_mode) {
        std::cout << "[REFCOUNT] Allocated array " << ptr 
                  << " (elements=" << count << ", element_size=" << element_size << ")" << std::endl;
    }
    
    return ptr;
}

// ============================================================================
// REFERENCE MANAGEMENT - ULTRA OPTIMIZED
// ============================================================================

extern "C" void* rc_retain(void* ptr) {
    if (!ptr) return ptr;
    
    RefCountHeader* header = get_refcount_header(ptr);
    if (!header) return ptr;
    
    // Check if object is being destroyed
    if (header->flags & REFCOUNT_FLAG_DESTROYING) {
        std::cerr << "[REFCOUNT ERROR] Attempting to retain object being destroyed: " << ptr << std::endl;
        return ptr;
    }
    
    // Ultra-fast atomic increment using intrinsics
    uint32_t new_count = refcount_atomic_inc(&header->ref_count);
    
    // Update statistics (only in debug mode for performance)
    if (g_rc_debug_mode) {
        std::lock_guard<std::mutex> lock(g_rc_stats_mutex);
        g_rc_stats.total_retains++;
        
        std::cout << "[REFCOUNT] Retained " << ptr << " (count: " << new_count << ")" << std::endl;
    }
    
    return ptr;
}

extern "C" void rc_release(void* ptr) {
    if (!ptr) return;
    
    RefCountHeader* header = get_refcount_header(ptr);
    if (!header) return;
    
    // Check for double-release in debug mode
    #if REFCOUNT_DEBUG_MODE
    if (g_rc_debug_mode && (header->flags & REFCOUNT_FLAG_DESTROYING)) {
        std::cerr << "[REFCOUNT ERROR] Double release detected: " << ptr << std::endl;
        abort();
    }
    #endif
    
    // Ultra-fast atomic decrement
    uint32_t new_count = refcount_atomic_dec(&header->ref_count);
    
    if (g_rc_debug_mode) {
        std::lock_guard<std::mutex> lock(g_rc_stats_mutex);
        g_rc_stats.total_releases++;
        std::cout << "[REFCOUNT] Released " << ptr << " (count: " << new_count << ")" << std::endl;
    }
    
    // Check if we need to destroy the object
    if (new_count == 0) {
        // Mark as destroying to prevent retain during destruction
        header->flags |= REFCOUNT_FLAG_DESTROYING;
        
        // Call type-specific destructor if available
        if (header->destructor) {
            header->destructor(ptr);
        }
        
        #if REFCOUNT_WEAK_REFS
        // Check if we can deallocate the control block
        uint32_t weak_count = refcount_atomic_dec(&header->weak_count);
        if (weak_count == 0) {
            // No weak references, safe to deallocate
            free(header);
        } else {
            // Mark as weak-only
            header->flags |= REFCOUNT_FLAG_WEAK_ONLY;
        }
        #else
        // No weak references, deallocate immediately
        free(header);
        #endif
        
        // Update statistics
        {
            std::lock_guard<std::mutex> lock(g_rc_stats_mutex);
            g_rc_stats.total_deallocations++;
            g_rc_stats.current_objects--;
        }
        
        #if REFCOUNT_DEBUG_MODE
        if (g_rc_debug_mode) {
            std::lock_guard<std::mutex> lock(g_debug_mutex);
            g_active_objects.erase(ptr);
            std::cout << "[REFCOUNT] Destroyed object " << ptr << std::endl;
        }
        #endif
    }
}

extern "C" uint32_t rc_get_count(void* ptr) {
    if (!ptr) return 0;
    
    RefCountHeader* header = get_refcount_header(ptr);
    if (!header) return 0;
    
    return header->ref_count.load(std::memory_order_acquire);
}

extern "C" int rc_is_unique(void* ptr) {
    return rc_get_count(ptr) == 1 ? 1 : 0;
}

// ============================================================================
// WEAK REFERENCE SUPPORT
// ============================================================================

#if REFCOUNT_WEAK_REFS
extern "C" void* rc_weak_retain(void* ptr) {
    if (!ptr) return nullptr;
    
    RefCountHeader* header = get_refcount_header(ptr);
    if (!header) return nullptr;
    
    // Increment weak count
    refcount_atomic_inc(&header->weak_count);
    
    if (g_rc_debug_mode) {
        std::lock_guard<std::mutex> lock(g_rc_stats_mutex);
        g_rc_stats.weak_creates++;
    }
    
    // Return the header as the weak reference (not the user pointer)
    return header;
}

extern "C" void rc_weak_release(void* weak_ptr) {
    if (!weak_ptr) return;
    
    RefCountHeader* header = static_cast<RefCountHeader*>(weak_ptr);
    
    uint32_t weak_count = refcount_atomic_dec(&header->weak_count);
    
    // If both strong and weak counts are zero, deallocate
    if (weak_count == 0 && (header->flags & REFCOUNT_FLAG_WEAK_ONLY)) {
        free(header);
    }
}

extern "C" void* rc_weak_lock(void* weak_ptr) {
    if (!weak_ptr) return nullptr;
    
    RefCountHeader* header = static_cast<RefCountHeader*>(weak_ptr);
    
    // Try to increment strong count if object still alive
    uint32_t current_count = header->ref_count.load(std::memory_order_acquire);
    
    while (current_count > 0) {
        if (header->ref_count.compare_exchange_weak(
                current_count, current_count + 1,
                std::memory_order_acq_rel, std::memory_order_acquire)) {
            // Successfully incremented, return user pointer
            return get_user_pointer(header);
        }
        // CAS failed, retry with updated value
    }
    
    // Object is dead
    return nullptr;
}

extern "C" int rc_weak_expired(void* weak_ptr) {
    if (!weak_ptr) return 1;
    
    RefCountHeader* header = static_cast<RefCountHeader*>(weak_ptr);
    return header->ref_count.load(std::memory_order_acquire) == 0 ? 1 : 0;
}
#endif

// ============================================================================
// CYCLE BREAKING SUPPORT (for "free shallow" keyword)
// ============================================================================

extern "C" void rc_break_cycles(void* ptr) {
    if (!ptr) return;
    
    RefCountHeader* header = get_refcount_header(ptr);
    if (!header) return;
    
    // Mark as cyclic for special handling
    header->flags |= REFCOUNT_FLAG_CYCLIC;
    
    // Forcibly set reference count to 1 to break cycles
    header->ref_count.store(1, std::memory_order_release);
    
    if (g_rc_debug_mode) {
        std::lock_guard<std::mutex> lock(g_rc_stats_mutex);
        g_rc_stats.cycle_breaks++;
        std::cout << "[REFCOUNT] Broke cycles for object " << ptr << std::endl;
    }
    
    // Now release it normally, which should trigger destruction
    rc_release(ptr);
}

extern "C" void rc_mark_cyclic(void* ptr) {
    if (!ptr) return;
    
    RefCountHeader* header = get_refcount_header(ptr);
    if (!header) return;
    
    header->flags |= REFCOUNT_FLAG_CYCLIC;
}

// ============================================================================
// TYPE-SPECIFIC DESTRUCTORS
// ============================================================================

extern "C" void rc_destructor_array(void* ptr) {
    if (!ptr) return;
    
    RefCountHeader* header = get_refcount_header(ptr);
    if (!header) return;
    
    // For arrays, we need to call destructors on individual elements
    // This would be type-specific based on the array element type
    
    if (g_rc_debug_mode) {
        std::cout << "[REFCOUNT] Array destructor called for " << ptr << std::endl;
    }
    
    // The actual memory will be freed by the caller
}

extern "C" void rc_destructor_string(void* ptr) {
    if (!ptr) return;
    
    if (g_rc_debug_mode) {
        std::cout << "[REFCOUNT] String destructor called for " << ptr << std::endl;
    }
    
    // String-specific cleanup would go here
    // For now, just ensure proper cleanup
}

extern "C" void rc_destructor_object(void* ptr) {
    if (!ptr) return;
    
    if (g_rc_debug_mode) {
        std::cout << "[REFCOUNT] Object destructor called for " << ptr << std::endl;
    }
    
    // Object-specific cleanup - call object destructor
    // This would integrate with the class system
}

extern "C" void rc_destructor_dynamic(void* ptr) {
    if (!ptr) return;
    
    if (g_rc_debug_mode) {
        std::cout << "[REFCOUNT] Dynamic value destructor called for " << ptr << std::endl;
    }
    
    // Dynamic value cleanup - check runtime type and dispatch
}

extern "C" void rc_register_destructor(uint32_t type_id, void (*destructor)(void*)) {
    std::lock_guard<std::mutex> lock(g_destructor_registry_mutex);
    g_destructor_registry[type_id] = destructor;
    
    if (g_rc_debug_mode) {
        std::cout << "[REFCOUNT] Registered destructor for type " << type_id << std::endl;
    }
}

// ============================================================================
// PERFORMANCE OPTIMIZATIONS
// ============================================================================

extern "C" void rc_retain_batch(void** ptrs, size_t count) {
    if (!ptrs) return;
    
    for (size_t i = 0; i < count; ++i) {
        if (ptrs[i]) {
            // Prefetch next object for better cache performance
            if (i + 1 < count && ptrs[i + 1]) {
                __builtin_prefetch(get_refcount_header(ptrs[i + 1]), 1, 3);
            }
            rc_retain(ptrs[i]);
        }
    }
}

extern "C" void rc_release_batch(void** ptrs, size_t count) {
    if (!ptrs) return;
    
    for (size_t i = 0; i < count; ++i) {
        if (ptrs[i]) {
            // Prefetch next object for better cache performance
            if (i + 1 < count && ptrs[i + 1]) {
                __builtin_prefetch(get_refcount_header(ptrs[i + 1]), 1, 3);
            }
            rc_release(ptrs[i]);
        }
    }
}

extern "C" void rc_prefetch_for_access(void* ptr) {
    if (!ptr) return;
    
    RefCountHeader* header = get_refcount_header(ptr);
    if (header) {
        __builtin_prefetch(header, 1, 3);    // Prefetch for read/write
        __builtin_prefetch(ptr, 0, 3);       // Prefetch user data for read
    }
}

// ============================================================================
// STATISTICS AND DEBUGGING
// ============================================================================

extern "C" void rc_get_stats(RefCountStats* stats) {
    if (!stats) return;
    
    std::lock_guard<std::mutex> lock(g_rc_stats_mutex);
    *stats = g_rc_stats;
}

extern "C" void rc_print_stats() {
    RefCountStats stats;
    rc_get_stats(&stats);
    
    std::cout << "\n=== REFERENCE COUNTING STATISTICS ===" << std::endl;
    std::cout << "Total allocations: " << stats.total_allocations << std::endl;
    std::cout << "Total deallocations: " << stats.total_deallocations << std::endl;
    std::cout << "Current objects: " << stats.current_objects << std::endl;
    std::cout << "Peak objects: " << stats.peak_objects << std::endl;
    std::cout << "Total retains: " << stats.total_retains << std::endl;
    std::cout << "Total releases: " << stats.total_releases << std::endl;
    std::cout << "Cycle breaks: " << stats.cycle_breaks << std::endl;
    std::cout << "Weak creates: " << stats.weak_creates << std::endl;
    std::cout << "Weak expires: " << stats.weak_expires << std::endl;
    std::cout << "=====================================" << std::endl;
}

extern "C" void rc_print_object_info(void* ptr) {
    if (!ptr) {
        std::cout << "[REFCOUNT] NULL pointer" << std::endl;
        return;
    }
    
    RefCountHeader* header = get_refcount_header(ptr);
    if (!header) {
        std::cout << "[REFCOUNT] Invalid pointer: " << ptr << std::endl;
        return;
    }
    
    std::cout << "[REFCOUNT] Object " << ptr << ":" << std::endl;
    std::cout << "  Reference count: " << header->ref_count.load() << std::endl;
    #if REFCOUNT_WEAK_REFS
    std::cout << "  Weak count: " << header->weak_count.load() << std::endl;
    #endif
    std::cout << "  Type ID: " << header->type_id << std::endl;
    std::cout << "  Size: " << header->size << std::endl;
    std::cout << "  Flags: 0x" << std::hex << header->flags << std::dec << std::endl;
    std::cout << "  Destructor: " << (header->destructor ? "Yes" : "No") << std::endl;
}

extern "C" void rc_set_debug_mode(int enabled) {
    g_rc_debug_mode = (enabled != 0);
    std::cout << "[REFCOUNT] Debug mode " << (enabled ? "enabled" : "disabled") << std::endl;
}

// ============================================================================
// INTEGRATION WITH FREE RUNTIME
// ============================================================================

extern "C" void rc_integrate_with_free_shallow(void* ptr) {
    if (!ptr) return;
    
    // For shallow free with reference counting, we break reference cycles
    // This allows the "free shallow" keyword to work with ref counting
    rc_break_cycles(ptr);
}

extern "C" void rc_integrate_with_free_deep(void* ptr) {
    if (!ptr) return;
    
    // For deep free, we recursively release all referenced objects
    // This is more complex and would require type system integration
    RefCountHeader* header = get_refcount_header(ptr);
    if (!header) return;
    
    // Mark as destroying to prevent new references
    header->flags |= REFCOUNT_FLAG_DESTROYING;
    
    // Force reference count to 1 and release
    header->ref_count.store(1, std::memory_order_release);
    rc_release(ptr);
}
