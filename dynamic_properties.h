#pragma once

#include <unordered_map>
#include <string>
#include <memory>
#include <atomic>
#include "ultra_performance_array.h"

// Forward declarations
struct DynamicValue;

/**
 * Dynamic Property Map - High-performance hash map for JavaScript-style dynamic properties
 * 
 * This structure is attached to objects to support:
 * - obj.unknownProperty = value  
 * - obj["dynamicKey"] = value
 * - for...in loops over dynamic properties
 * 
 * Performance optimizations:
 * - Lazy initialization (only created when first dynamic property is set)
 * - Small string optimization for property names
 * - Cache-friendly memory layout
 */
struct DynamicPropertyMap {
    // Hash map storing dynamic properties
    std::unordered_map<std::string, DynamicValue*> properties;
    
    // Property count for fast iteration
    size_t property_count;
    
    // Reference count for garbage collection
    std::atomic<int> ref_count;
    
    DynamicPropertyMap() : property_count(0), ref_count(1) {}
    
    ~DynamicPropertyMap() {
        // Clean up all dynamic values
        for (auto& [key, value] : properties) {
            delete value;
        }
        properties.clear();
    }
    
    // Get a property (returns nullptr if not found)
    DynamicValue* get(const std::string& key) {
        auto it = properties.find(key);
        return (it != properties.end()) ? it->second : nullptr;
    }
    
    // Set a property (creates new DynamicValue if needed)
    void set(const std::string& key, DynamicValue* value) {
        auto it = properties.find(key);
        if (it != properties.end()) {
            // Replace existing value
            delete it->second;
            it->second = value;
        } else {
            // Add new property
            properties[key] = value;
            property_count++;
        }
    }
    
    // Check if property exists
    bool has(const std::string& key) const {
        return properties.find(key) != properties.end();
    }
    
    // Remove a property
    bool remove(const std::string& key) {
        auto it = properties.find(key);
        if (it != properties.end()) {
            delete it->second;
            properties.erase(it);
            property_count--;
            return true;
        }
        return false;
    }
    
    // Get all property keys (for for...in loops)
    std::vector<std::string> get_keys() const {
        std::vector<std::string> keys;
        keys.reserve(property_count);
        for (const auto& [key, value] : properties) {
            keys.push_back(key);
        }
        return keys;
    }
    
    // Add reference (for GC)
    void add_ref() {
        ref_count.fetch_add(1);
    }
    
    // Release reference (for GC)
    void release() {
        if (ref_count.fetch_sub(1) == 1) {
            delete this;
        }
    }
};

/**
 * Extended Object Layout for Dynamic Properties
 * 
 * New layout: [class_name_ptr][property_count][dynamic_map_ptr][property0][property1]...
 * 
 * Offsets:
 * - 0:  class_name_ptr (GoTSString*)
 * - 8:  property_count (int64_t) 
 * - 16: dynamic_map_ptr (DynamicPropertyMap*)
 * - 24: property0 (first static property)
 * - 32: property1 (second static property)
 * - ...
 */

// Runtime functions for dynamic property access
extern "C" {
    // Get dynamic property - returns DynamicValue* or nullptr
    void* __dynamic_property_get(void* object_ptr, const char* property_name);
    
    // Set dynamic property - creates property if it doesn't exist
    void __dynamic_property_set(void* object_ptr, const char* property_name, void* dynamic_value);
    
    // Check if dynamic property exists
    int __dynamic_property_has(void* object_ptr, const char* property_name);
    
    // Delete dynamic property
    int __dynamic_property_delete(void* object_ptr, const char* property_name);
    
    // Get all dynamic property keys (for for...in loops)
    void* __dynamic_property_keys(void* object_ptr);
    
    // Create DynamicValue from various types
    void* __dynamic_value_create_any(void* value, int type_id);
    
    // Helper functions for object layout
    DynamicPropertyMap* __get_dynamic_map(void* object_ptr);
    void __ensure_dynamic_map(void* object_ptr);
}

// Object layout access macros
#define OBJECT_CLASS_NAME_OFFSET 0
#define OBJECT_PROPERTY_COUNT_OFFSET 8
#define OBJECT_DYNAMIC_MAP_OFFSET 16
#define OBJECT_PROPERTIES_START_OFFSET 24

#define GET_OBJECT_CLASS_NAME(obj) \
    (*reinterpret_cast<void**>(reinterpret_cast<char*>(obj) + OBJECT_CLASS_NAME_OFFSET))

#define GET_OBJECT_PROPERTY_COUNT(obj) \
    (*reinterpret_cast<int64_t*>(reinterpret_cast<char*>(obj) + OBJECT_PROPERTY_COUNT_OFFSET))

#define GET_OBJECT_DYNAMIC_MAP(obj) \
    (*reinterpret_cast<DynamicPropertyMap**>(reinterpret_cast<char*>(obj) + OBJECT_DYNAMIC_MAP_OFFSET))

#define SET_OBJECT_DYNAMIC_MAP(obj, map) \
    (*reinterpret_cast<DynamicPropertyMap**>(reinterpret_cast<char*>(obj) + OBJECT_DYNAMIC_MAP_OFFSET) = map)

#define GET_OBJECT_PROPERTY_PTR(obj, index) \
    (reinterpret_cast<void**>(reinterpret_cast<char*>(obj) + OBJECT_PROPERTIES_START_OFFSET + (index * 8)))
