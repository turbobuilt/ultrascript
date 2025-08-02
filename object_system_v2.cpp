#include "object_system_v2.h"
#include <iostream>
#include <cstdio>

namespace ultraScript {

// Global object tracking for debugging and GC
static std::unordered_set<ObjectInstance*> g_allocated_objects;
static std::mutex g_object_registry_mutex;

// Track allocated objects for GC
void track_object(ObjectInstance* obj) {
    std::lock_guard<std::mutex> lock(g_object_registry_mutex);
    g_allocated_objects.insert(obj);
}

void untrack_object(ObjectInstance* obj) {
    std::lock_guard<std::mutex> lock(g_object_registry_mutex);
    g_allocated_objects.erase(obj);
}

// Enhanced ObjectManager implementation
ObjectInstance* ObjectManager::create_object_tracked(const std::string& class_name) {
    ObjectInstance* obj = create_object(class_name);
    if (obj) {
        track_object(obj);
    }
    return obj;
}

ObjectInstance* ObjectManager::create_object_tracked(ObjectTypeId type_id) {
    ObjectInstance* obj = create_object(type_id);
    if (obj) {
        track_object(obj);
    }
    return obj;
}

void ObjectManager::destroy_object_tracked(ObjectInstance* obj) {
    if (obj) {
        untrack_object(obj);
        destroy_object(obj);
    }
}

} // namespace ultraScript

// C runtime interface functions for maximum performance
extern "C" {

using namespace ultraScript;

// Class registration interface
uint32_t __register_class(const char* class_name) {
    if (!class_name) return 0;
    
    ObjectTypeId type_id = ClassRegistry::instance().register_class(class_name);
    return static_cast<uint32_t>(type_id);
}

// Add property to a class during compilation
bool __class_add_property(uint32_t type_id, const char* property_name, uint16_t property_type, uint32_t property_size) {
    if (!property_name) return false;
    
    ClassMetadata* meta = ClassRegistry::instance().get_class_metadata(static_cast<ObjectTypeId>(type_id));
    if (!meta) return false;
    
    meta->add_property(property_name, static_cast<PropertyTypeId>(property_type), property_size);
    return true;
}

// Get property index by name (for compilation phase)
int32_t __class_get_property_index(uint32_t type_id, const char* property_name) {
    if (!property_name) return -1;
    
    ClassMetadata* meta = ClassRegistry::instance().get_class_metadata(static_cast<ObjectTypeId>(type_id));
    if (!meta) return -1;
    
    uint32_t hash = hash_string(property_name);
    auto it = meta->property_hash_to_index.find(hash);
    if (it == meta->property_hash_to_index.end()) return -1;
    
    return static_cast<int32_t>(it->second);
}

// Object creation (runtime interface)
void* __object_create_by_type_id(uint32_t type_id) {
    return ObjectManager::create_object_tracked(static_cast<ObjectTypeId>(type_id));
}

void* __object_create_by_name(const char* class_name) {
    if (!class_name) return nullptr;
    return ObjectManager::create_object_tracked(class_name);
}

// Ultra-fast property access by index (for compiled code)
void* __object_get_property_by_index(void* obj_ptr, uint16_t property_index) {
    if (!obj_ptr) return nullptr;
    
    ObjectInstance* obj = static_cast<ObjectInstance*>(obj_ptr);
    return obj->get_property_by_index<void>(property_index);
}

// Fast property access by name hash (for dynamic code)
void* __object_get_property_by_hash(void* obj_ptr, uint32_t name_hash) {
    if (!obj_ptr) return nullptr;
    
    ObjectInstance* obj = static_cast<ObjectInstance*>(obj_ptr);
    return obj->get_property_by_hash(name_hash);
}

// Property access by name (slowest, for debugging/fallback)
void* __object_get_property_by_name(void* obj_ptr, const char* property_name) {
    if (!obj_ptr || !property_name) return nullptr;
    
    ObjectInstance* obj = static_cast<ObjectInstance*>(obj_ptr);
    return obj->get_property_by_name(property_name);
}

// Ultra-fast property setting by index
bool __object_set_property_by_index_int64(void* obj_ptr, uint16_t property_index, int64_t value) {
    if (!obj_ptr) return false;
    
    ObjectInstance* obj = static_cast<ObjectInstance*>(obj_ptr);
    return obj->set_property_by_index(property_index, value);
}

bool __object_set_property_by_index_double(void* obj_ptr, uint16_t property_index, double value) {
    if (!obj_ptr) return false;
    
    ObjectInstance* obj = static_cast<ObjectInstance*>(obj_ptr);
    return obj->set_property_by_index(property_index, value);
}

bool __object_set_property_by_index_ptr(void* obj_ptr, uint16_t property_index, void* value) {
    if (!obj_ptr) return false;
    
    ObjectInstance* obj = static_cast<ObjectInstance*>(obj_ptr);
    return obj->set_property_by_index(property_index, value);
}

// Fast property setting by hash
bool __object_set_property_by_hash(void* obj_ptr, uint32_t name_hash, const void* value, uint32_t value_size) {
    if (!obj_ptr || !value) return false;
    
    ObjectInstance* obj = static_cast<ObjectInstance*>(obj_ptr);
    return obj->set_property_by_hash(name_hash, value, value_size);
}

// Object destruction
void __object_destroy(void* obj_ptr) {
    if (!obj_ptr) return;
    
    ObjectInstance* obj = static_cast<ObjectInstance*>(obj_ptr);
    ObjectManager::destroy_object_tracked(obj);
}

// Debugging functions
void __object_print_debug(void* obj_ptr) {
    if (!obj_ptr) {
        printf("Object: NULL\n");
        return;
    }
    
    ObjectInstance* obj = static_cast<ObjectInstance*>(obj_ptr);
    ClassMetadata* meta = ClassRegistry::instance().get_class_metadata(
        static_cast<ObjectTypeId>(obj->header.type_id));
    
    if (meta) {
        printf("Object: %s (type_id=%u, properties=%u, size=%u)\n", 
               meta->class_name.c_str(), 
               obj->header.type_id,
               obj->header.property_count,
               obj->header.instance_size);
               
        for (size_t i = 0; i < meta->properties.size(); i++) {
            const PropertyDescriptor& prop = meta->properties[i];
            void* prop_ptr = obj->get_property_by_index<void>(static_cast<uint16_t>(i));
            printf("  %s: %p (offset=%u, type=%u)\n", 
                   prop.name.c_str(), prop_ptr, prop.offset, static_cast<uint16_t>(prop.type_id));
        }
    } else {
        printf("Object: Unknown class (type_id=%u)\n", obj->header.type_id);
    }
}

// String hash function for runtime use
uint32_t __hash_string(const char* str) {
    return str ? hash_string(str) : 0;
}

// Get class metadata for debugging
void* __get_class_metadata(const char* class_name) {
    if (!class_name) return nullptr;
    return ClassRegistry::instance().get_class_metadata(class_name);
}

void* __get_class_metadata_by_id(uint32_t type_id) {
    return ClassRegistry::instance().get_class_metadata(static_cast<ObjectTypeId>(type_id));
}

} // extern "C"
