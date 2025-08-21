#pragma once

// ============================================================================
// FREE RUNTIME HEADER - HIGH PERFORMANCE MANUAL MEMORY MANAGEMENT
// ============================================================================

#ifdef __cplusplus
extern "C" {
#endif

// Debug and validation functions
void __debug_log_free_operation(void* ptr, int is_shallow);
void __debug_validate_post_free();
void __debug_log_primitive_free_ignored();

// Type-specific free functions (JIT-optimized)
void __free_class_instance_shallow(void* ptr);
void __free_class_instance_deep(void* ptr);
void __free_array_shallow(void* ptr);
void __free_array_deep(void* ptr);
void __free_string(void* ptr);
void __free_dynamic_value(void* ptr, int is_shallow);

// Migration functions for existing code
void __migrate_to_rc_alloc();
void __migrate_from_gc_to_rc();

// Statistics and debugging
void __get_free_stats(size_t* stats_out);
void __print_free_stats();
void __set_free_debug_mode(int enabled);

// Error functions
void __throw_deep_free_not_implemented();

#ifdef __cplusplus
}
#endif
