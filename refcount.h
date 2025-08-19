#pragma once

#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <type_traits>

// ============================================================================
// HIGH PERFORMANCE REFERENCE COUNTING SYSTEM
// UltraScript equivalent of std::shared_ptr with manual optimizations
// ============================================================================

// Configuration - compile time options for performance tuning
#define REFCOUNT_USE_INTRINSICS 1      // Use CPU intrinsics for atomic ops
#define REFCOUNT_CACHE_ALIGNED 1       // Align refcount to cache lines
#define REFCOUNT_WEAK_REFS 1           // Support weak references
#define REFCOUNT_THREAD_SAFE 1         // Thread safety via atomics
#define REFCOUNT_DEBUG_MODE 0          // Debug tracking (disable in release)

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// REFERENCE COUNTED OBJECT HEADER
// ============================================================================

// Cache-aligned reference count structure for maximum performance
struct alignas(64) RefCountHeader {
    // Main reference count - uses atomic operations for thread safety
    #if REFCOUNT_THREAD_SAFE
    std::atomic<uint32_t> ref_count;
    #if REFCOUNT_WEAK_REFS
    std::atomic<uint32_t> weak_count;
    #endif
    #else
    uint32_t ref_count;
    #if REFCOUNT_WEAK_REFS
    uint32_t weak_count;
    #endif
    #endif
    
    // Object metadata
    uint32_t type_id;           // Type information for smart deallocation
    uint32_t size;              // Size of the allocated object
    uint32_t flags;             // Various object flags
    
    // Destructor function pointer for polymorphic cleanup
    void (*destructor)(void* obj);
    
    #if REFCOUNT_DEBUG_MODE
    void* allocation_site;      // Debug: where was this allocated
    uint64_t allocation_id;     // Debug: unique allocation ID
    #endif
    
    // Constructor
    RefCountHeader(uint32_t initial_refs = 1, uint32_t obj_type_id = 0, 
                   uint32_t obj_size = 0, void (*dtor)(void*) = nullptr) 
        : ref_count(initial_refs), type_id(obj_type_id), size(obj_size), 
          flags(0), destructor(dtor)
    {
        #if REFCOUNT_WEAK_REFS
        weak_count.store(1); // Control block reference
        #endif
        #if REFCOUNT_DEBUG_MODE
        allocation_site = __builtin_return_address(0);
        static std::atomic<uint64_t> next_id{1};
        allocation_id = next_id.fetch_add(1);
        #endif
    }
};

// Object flags
#define REFCOUNT_FLAG_DESTROYING    0x01    // Object is being destroyed
#define REFCOUNT_FLAG_WEAK_ONLY     0x02    // Only weak references remain
#define REFCOUNT_FLAG_CYCLIC        0x04    // Part of reference cycle
#define REFCOUNT_FLAG_IMMUTABLE     0x08    // Immutable object (copy-on-write eligible)

// ============================================================================
// HIGH PERFORMANCE INTRINSIC-BASED OPERATIONS
// ============================================================================

// Ultra-fast atomic increment using CPU intrinsics
static inline uint32_t refcount_atomic_inc(std::atomic<uint32_t>* counter) {
    #if REFCOUNT_USE_INTRINSICS && defined(__x86_64__)
    // Use lock xadd for maximum performance on x86_64
    uint32_t result;
    __asm__ volatile (
        "lock xaddl %0, %1"
        : "=r" (result), "+m" (*counter)
        : "0" (1)
        : "memory"
    );
    return result + 1;
    #else
    return counter->fetch_add(1, std::memory_order_acq_rel) + 1;
    #endif
}

// Ultra-fast atomic decrement with zero check
static inline uint32_t refcount_atomic_dec(std::atomic<uint32_t>* counter) {
    #if REFCOUNT_USE_INTRINSICS && defined(__x86_64__)
    // Use lock xadd for maximum performance on x86_64
    uint32_t result;
    __asm__ volatile (
        "lock xaddl %0, %1"
        : "=r" (result), "+m" (*counter)
        : "0" (-1)
        : "memory"
    );
    return result - 1;
    #else
    return counter->fetch_sub(1, std::memory_order_acq_rel) - 1;
    #endif
}

// ============================================================================
// CORE REFERENCE COUNTING API
// ============================================================================

// Get header from user pointer
static inline RefCountHeader* get_refcount_header(void* ptr) {
    if (!ptr) return nullptr;
    return reinterpret_cast<RefCountHeader*>(
        static_cast<char*>(ptr) - sizeof(RefCountHeader)
    );
}

// Get user pointer from header
static inline void* get_user_pointer(RefCountHeader* header) {
    if (!header) return nullptr;
    return static_cast<char*>(static_cast<void*>(header)) + sizeof(RefCountHeader);
}

// ============================================================================
// ALLOCATION FUNCTIONS
// ============================================================================

// Allocate reference counted object
void* rc_alloc(size_t size, uint32_t type_id, void (*destructor)(void*));

// Allocate reference counted array
void* rc_alloc_array(size_t element_size, size_t count, uint32_t type_id, void (*destructor)(void*));

// ============================================================================
// REFERENCE MANAGEMENT FUNCTIONS
// ============================================================================

// Add reference (equivalent to shared_ptr copy)
void* rc_retain(void* ptr);

// Remove reference (equivalent to shared_ptr destructor)
void rc_release(void* ptr);

// Get current reference count (for debugging)
uint32_t rc_get_count(void* ptr);

// Check if object is uniquely referenced
int rc_is_unique(void* ptr);

// ============================================================================
// WEAK REFERENCE SUPPORT
// ============================================================================

#if REFCOUNT_WEAK_REFS
// Create weak reference
void* rc_weak_retain(void* ptr);

// Release weak reference
void rc_weak_release(void* weak_ptr);

// Try to upgrade weak reference to strong reference
void* rc_weak_lock(void* weak_ptr);

// Check if weak reference is expired
int rc_weak_expired(void* weak_ptr);
#endif

// ============================================================================
// CYCLE BREAKING SUPPORT (for "free shallow" keyword)
// ============================================================================

// Manually break reference cycles - called by "free shallow"
void rc_break_cycles(void* ptr);

// Mark object as part of a cycle (for cycle detection)
void rc_mark_cyclic(void* ptr);

// ============================================================================
// TYPE-SPECIFIC DESTRUCTORS
// ============================================================================

// Built-in destructors for common types
void rc_destructor_array(void* ptr);
void rc_destructor_string(void* ptr);
void rc_destructor_object(void* ptr);
void rc_destructor_dynamic(void* ptr);

// Custom destructor registration
void rc_register_destructor(uint32_t type_id, void (*destructor)(void*));

// ============================================================================
// PERFORMANCE OPTIMIZATIONS
// ============================================================================

// Batch operations for better cache locality
void rc_retain_batch(void** ptrs, size_t count);
void rc_release_batch(void** ptrs, size_t count);

// Prefetch operations for predicted access patterns
void rc_prefetch_for_access(void* ptr);

// ============================================================================
// STATISTICS AND DEBUGGING
// ============================================================================

typedef struct {
    uint64_t total_allocations;
    uint64_t total_deallocations;
    uint64_t current_objects;
    uint64_t peak_objects;
    uint64_t total_retains;
    uint64_t total_releases;
    uint64_t cycle_breaks;
    uint64_t weak_creates;
    uint64_t weak_expires;
} RefCountStats;

// Get current statistics
void rc_get_stats(RefCountStats* stats);

// Print debugging information
void rc_print_stats();
void rc_print_object_info(void* ptr);

// Enable/disable debug mode
void rc_set_debug_mode(int enabled);

// ============================================================================
// INTEGRATION WITH FREE RUNTIME
// ============================================================================

// Integration functions called by the existing free runtime
void rc_integrate_with_free_shallow(void* ptr);
void rc_integrate_with_free_deep(void* ptr);

#ifdef __cplusplus
}
#endif

// ============================================================================
// C++ TEMPLATE INTERFACE (for C++ code)
// ============================================================================

#ifdef __cplusplus

template<typename T>
class RefPtr {
private:
    T* ptr_;
    
public:
    // Constructors
    RefPtr() : ptr_(nullptr) {}
    
    explicit RefPtr(T* p) : ptr_(p) {
        if (ptr_) rc_retain(ptr_);
    }
    
    RefPtr(const RefPtr& other) : ptr_(other.ptr_) {
        if (ptr_) rc_retain(ptr_);
    }
    
    RefPtr(RefPtr&& other) noexcept : ptr_(other.ptr_) {
        other.ptr_ = nullptr;
    }
    
    // Destructor
    ~RefPtr() {
        if (ptr_) rc_release(ptr_);
    }
    
    // Assignment operators
    RefPtr& operator=(const RefPtr& other) {
        if (this != &other) {
            if (ptr_) rc_release(ptr_);
            ptr_ = other.ptr_;
            if (ptr_) rc_retain(ptr_);
        }
        return *this;
    }
    
    RefPtr& operator=(RefPtr&& other) noexcept {
        if (this != &other) {
            if (ptr_) rc_release(ptr_);
            ptr_ = other.ptr_;
            other.ptr_ = nullptr;
        }
        return *this;
    }
    
    // Access operators
    T* operator->() const { return ptr_; }
    T& operator*() const { return *ptr_; }
    T* get() const { return ptr_; }
    
    // Utility functions
    explicit operator bool() const { return ptr_ != nullptr; }
    bool unique() const { return ptr_ && rc_is_unique(ptr_); }
    uint32_t use_count() const { return ptr_ ? rc_get_count(ptr_) : 0; }
    
    // Reset and release
    void reset() {
        if (ptr_) {
            rc_release(ptr_);
            ptr_ = nullptr;
        }
    }
    
    void reset(T* p) {
        if (ptr_) rc_release(ptr_);
        ptr_ = p;
        if (ptr_) rc_retain(ptr_);
    }
};

// Factory functions
template<typename T, typename... Args>
RefPtr<T> make_ref(Args&&... args) {
    static_assert(std::is_destructible_v<T>, "T must be destructible");
    
    void* memory = rc_alloc(sizeof(T), 0, [](void* p) {
        static_cast<T*>(p)->~T();
    });
    
    if (!memory) return RefPtr<T>();
    
    new (memory) T(std::forward<Args>(args)...);
    return RefPtr<T>(static_cast<T*>(memory));
}

#endif // __cplusplus
