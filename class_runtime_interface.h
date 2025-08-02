#pragma once

// // #include "class_system_performance.h" // Removed - property access system redesigned // Removed - property access system redesigned

namespace ultraScript {

// C runtime interface functions for maximum performance object operations
extern "C" {

// ==================== Class Registration (Compile-time) ====================

// Register a new class during compilation
uint32_t __register_class_performance(const char* class_name);

// Add property to a class during compilation
bool __class_add_property_performance(uint32_t type_id, const char* property_name, 
                                    uint8_t property_type, uint8_t property_flags);

// Set inheritance relationship
bool __class_set_inheritance_performance(uint32_t child_type_id, uint32_t parent_type_id);

// Finalize class layout after all properties added
bool __class_finalize_layout_performance(uint32_t type_id);

// Get property index by name (for compilation phase)
int16_t __class_get_property_index_performance(uint32_t type_id, const char* property_name);

// Get property offset by index (for direct code generation)
uint32_t __class_get_property_offset_performance(uint32_t type_id, uint16_t property_index);

// Get property type by index (for code generation optimization)
uint8_t __class_get_property_type_performance(uint32_t type_id, uint16_t property_index);

// ==================== Object Creation (Runtime) ====================

// Ultra-fast object creation by type ID
void* __object_create_by_type_id_performance(uint32_t type_id);

// Object creation by class name (slower)
void* __object_create_by_name_performance(const char* class_name);

// Object destruction
void __object_destroy_performance(void* obj_ptr);

// ==================== Property Access (Runtime) ====================

// Ultra-fast property access by index (compiled code path - FASTEST)
void* __object_get_property_by_index_performance(void* obj_ptr, uint16_t property_index);

// Typed property access functions for maximum performance
int64_t __object_get_property_int64_performance(void* obj_ptr, uint16_t property_index);
double __object_get_property_double_performance(void* obj_ptr, uint16_t property_index);
void* __object_get_property_ptr_performance(void* obj_ptr, uint16_t property_index);
bool __object_get_property_bool_performance(void* obj_ptr, uint16_t property_index);

// Fast property access by name hash (dynamic code path)
void* __object_get_property_by_hash_performance(void* obj_ptr, uint32_t name_hash);

// Property access by name (slowest - for debugging/fallback)
void* __object_get_property_by_name_performance(void* obj_ptr, const char* property_name);

// ==================== Property Assignment (Runtime) ====================

// Ultra-fast property assignment by index
bool __object_set_property_by_index_int64_performance(void* obj_ptr, uint16_t property_index, int64_t value);
bool __object_set_property_by_index_double_performance(void* obj_ptr, uint16_t property_index, double value);
bool __object_set_property_by_index_ptr_performance(void* obj_ptr, uint16_t property_index, void* value);
bool __object_set_property_by_index_bool_performance(void* obj_ptr, uint16_t property_index, bool value);

// Generic property assignment by index (for any type)
bool __object_set_property_by_index_performance(void* obj_ptr, uint16_t property_index, 
                                               const void* value, uint32_t value_size);

// Fast property assignment by hash
bool __object_set_property_by_hash_performance(void* obj_ptr, uint32_t name_hash, 
                                              const void* value, uint32_t value_size);

// Property assignment by name (slowest)
bool __object_set_property_by_name_performance(void* obj_ptr, const char* property_name, 
                                              const void* value, uint32_t value_size);

// ==================== Dynamic Properties (Runtime) ====================

// Access to unregistered properties (stored in map)
void* __object_get_dynamic_property_performance(void* obj_ptr, uint32_t name_hash);
bool __object_set_dynamic_property_performance(void* obj_ptr, uint32_t name_hash, 
                                               const void* value, uint32_t value_size);

// Check if object has dynamic property
bool __object_has_dynamic_property_performance(void* obj_ptr, uint32_t name_hash);

// ==================== Method Calls (Runtime) ====================

// Call method by name (with argument array)
void* __object_call_method_performance(void* obj_ptr, const char* method_name, 
                                       void** args, uint32_t arg_count);

// Call method by hash (faster)
void* __object_call_method_by_hash_performance(void* obj_ptr, uint32_t method_hash, 
                                               void** args, uint32_t arg_count);

// ==================== Type Checking (Runtime) ====================

// Ultra-fast type checking
bool __object_is_instance_of_performance(void* obj_ptr, uint32_t type_id);
bool __object_inherits_from_performance(void* obj_ptr, const char* class_name);

// Get object type information
uint32_t __object_get_type_id_performance(void* obj_ptr);
const char* __object_get_class_name_performance(void* obj_ptr);

// ==================== Debugging and Introspection ====================

// Print object debug information
void __object_print_debug_performance(void* obj_ptr);

// Get object property count
uint16_t __object_get_property_count_performance(void* obj_ptr);

// Get property name by index
const char* __object_get_property_name_performance(void* obj_ptr, uint16_t property_index);

// ==================== Utility Functions ====================

// String hashing for property lookups
uint32_t __hash_property_name_performance(const char* str);

// Get class metadata for debugging
void* __get_class_metadata_performance(const char* class_name);
void* __get_class_metadata_by_id_performance(uint32_t type_id);

// Memory management statistics
uint64_t __get_object_memory_usage_performance();
uint32_t __get_allocated_object_count_performance();

} // extern "C"

// ==================== C++ Helper Functions ====================

// Template functions for fast property access removed - will be reimplemented according to new architecture

// Compile-time property offset calculation macro
#define PROPERTY_OFFSET_FAST(ClassName, PropertyName) \
    (ClassRegistry::instance().get_class_metadata(#ClassName)->get_property_index(#PropertyName))

// Ultra-fast property access macro for known types
#define GET_PROPERTY_FAST(obj, ClassName, PropertyName, Type) \
    object_get_property_fast<Type>(obj, PROPERTY_OFFSET_FAST(ClassName, PropertyName))

// Ultra-fast property assignment macro for known types
#define SET_PROPERTY_FAST(obj, ClassName, PropertyName, value) \
    object_set_property_fast(obj, PROPERTY_OFFSET_FAST(ClassName, PropertyName), value)

} // namespace ultraScript
