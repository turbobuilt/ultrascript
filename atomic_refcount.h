#pragma once

// ============================================================================
// ATOMIC REFERENCE COUNTING - LEGACY COMPATIBILITY HEADER
// This file provides compatibility with the new refcount.h system
// ============================================================================

#include "refcount.h"

// Legacy compatibility aliases
typedef RefCountHeader AtomicRefCountHeader;
typedef RefCountStats AtomicRefCountStats;

// Legacy function aliases for backward compatibility
#define atomic_rc_alloc(size, type_id, destructor) rc_alloc(size, type_id, destructor)
#define atomic_rc_retain(ptr) rc_retain(ptr)
#define atomic_rc_release(ptr) rc_release(ptr)
#define atomic_rc_get_count(ptr) rc_get_count(ptr)
#define atomic_rc_is_unique(ptr) rc_is_unique(ptr)

// Legacy C++ interface
template<typename T>
using AtomicRefPtr = RefPtr<T>;

#ifdef __cplusplus
extern "C" {
#endif

// Legacy function declarations (redirected to new implementation)
void* atomic_ref_alloc(size_t size);
void atomic_ref_retain(void* ptr);
void atomic_ref_release(void* ptr);
int atomic_ref_count(void* ptr);

#ifdef __cplusplus
}
#endif