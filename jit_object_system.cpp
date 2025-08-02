#include "jit_class_registry.h"
#include <cstdlib>
#include <cstring>
#include <iostream>

namespace ultraScript {

// Simple object header for JIT objects
struct JITObjectHeader {
    uint32_t class_id;     // Hash of class name for type checking
    uint32_t ref_count;    // Reference counting for GC
};

} // namespace ultraScript

extern "C" {

using namespace ultraScript;

// JIT-optimized object creation - returns direct pointer, not ID
void* __jit_object_create(const char* class_name) {
    if (!class_name) return nullptr;
    
    const JITClassInfo* class_info = JITClassRegistry::instance().get_class_info(class_name);
    if (!class_info) {
        std::cerr << "ERROR: Unknown class: " << class_name << std::endl;
        return nullptr;
    }
    
    // Allocate memory for the entire object
    size_t total_size = class_info->instance_size;
    void* obj_ptr = malloc(total_size);
    if (!obj_ptr) return nullptr;
    
    // Initialize memory to zero
    memset(obj_ptr, 0, total_size);
    
    // Set up object header
    JITObjectHeader* header = static_cast<JITObjectHeader*>(obj_ptr);
    header->class_id = std::hash<std::string>{}(class_name);
    header->ref_count = 1;
    
    return obj_ptr;
}

// JIT-optimized object creation with known size (for ultimate performance)
void* __jit_object_create_sized(uint32_t size, uint32_t class_id) {
    void* obj_ptr = malloc(size);
    if (!obj_ptr) return nullptr;
    
    memset(obj_ptr, 0, size);
    
    JITObjectHeader* header = static_cast<JITObjectHeader*>(obj_ptr);
    header->class_id = class_id;
    header->ref_count = 1;
    
    return obj_ptr;
}

// Object destruction
void __jit_object_destroy(void* obj_ptr) {
    if (obj_ptr) {
        free(obj_ptr);
    }
}

// Register class during compilation - called by compiler
void __jit_register_class(const char* class_name) {
    if (class_name) {
        JITClassRegistry::instance().register_class(class_name);
    }
}

// Add property during compilation - called by compiler
void __jit_add_property(const char* class_name, const char* prop_name, uint8_t type_id, uint32_t size) {
    if (class_name && prop_name) {
        JITClassRegistry::instance().add_property(class_name, prop_name, type_id, size);
    }
}

// Get property offset for JIT code generation - called by compiler
uint32_t __jit_get_property_offset(const char* class_name, const char* prop_name) {
    if (class_name && prop_name) {
        return JITClassRegistry::instance().get_property_offset(class_name, prop_name);
    }
    return 0;
}

// Get instance size for JIT code generation - called by compiler
uint32_t __jit_get_instance_size(const char* class_name) {
    if (class_name) {
        return JITClassRegistry::instance().get_instance_size(class_name);
    }
    return 8; // Default header size
}

// Debug function to print object layout
void __jit_debug_object(void* obj_ptr, const char* class_name) {
    if (!obj_ptr || !class_name) return;
    
    const JITClassInfo* class_info = JITClassRegistry::instance().get_class_info(class_name);
    if (!class_info) return;
    
    JITObjectHeader* header = static_cast<JITObjectHeader*>(obj_ptr);
    printf("Object %s at %p:\n", class_name, obj_ptr);
    printf("  class_id: %u, ref_count: %u\n", header->class_id, header->ref_count);
    printf("  total_size: %u bytes\n", class_info->instance_size);
    
    for (const auto& prop : class_info->properties) {
        void* prop_ptr = static_cast<char*>(obj_ptr) + prop.offset;
        printf("  %s: offset=%u, size=%u, value_ptr=%p\n", 
               prop.name.c_str(), prop.offset, prop.size, prop_ptr);
    }
}

} // extern "C"
