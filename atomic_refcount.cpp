#include "atomic_refcount.h"

// ============================================================================
// ATOMIC REFERENCE COUNTING - LEGACY COMPATIBILITY IMPLEMENTATION
// ============================================================================

extern "C" {

void* atomic_ref_alloc(size_t size) {
    return rc_alloc(size, 0, nullptr);
}

void atomic_ref_retain(void* ptr) {
    rc_retain(ptr);
}

void atomic_ref_release(void* ptr) {
    rc_release(ptr);
}

int atomic_ref_count(void* ptr) {
    return static_cast<int>(rc_get_count(ptr));
}

}