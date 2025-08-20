#include "dynamic_properties.h"
#include "ultra_performance_array.h"
#include "compiler.h"
#include <iostream>
#include <cstring>

// Forward declaration for compiler access
extern GoTSCompiler* get_current_compiler();

// Runtime implementations for dynamic property access

extern "C" {
/**
 * Get the dynamic property map from an object, returning nullptr if not initialized
 */
DynamicPropertyMap* __get_dynamic_map(void* object_ptr) {
    if (!object_ptr) return nullptr;
    return GET_OBJECT_DYNAMIC_MAP(object_ptr);
}

/**
 * Ensure the object has a dynamic property map, creating one if necessary
 */
void __ensure_dynamic_map(void* object_ptr) {
    if (!object_ptr) return;
    
    DynamicPropertyMap* existing_map = GET_OBJECT_DYNAMIC_MAP(object_ptr);
    if (!existing_map) {
        // Lazy initialization - create the map only when first needed
        DynamicPropertyMap* new_map = new DynamicPropertyMap();
        SET_OBJECT_DYNAMIC_MAP(object_ptr, new_map);
        
        std::cout << "[DYNAMIC] Created dynamic property map for object " << object_ptr << std::endl;
    }
}

/**
 * Get a dynamic property value
 * Returns DynamicValue* if found, nullptr if not found
 */
void* __dynamic_property_get(void* object_ptr, const char* property_name) {
    if (!object_ptr || !property_name) {
        return nullptr;
    }
    
    std::cout << "[DYNAMIC] Getting property '" << property_name << "' from object " << object_ptr << std::endl;
    
    DynamicPropertyMap* map = __get_dynamic_map(object_ptr);
    if (!map) {
        std::cout << "[DYNAMIC] No dynamic property map found" << std::endl;
        return nullptr;
    }
    
    DynamicValue* result = map->get(std::string(property_name));
    std::cout << "[DYNAMIC] Property '" << property_name << "' " 
              << (result ? "found" : "not found") << std::endl;
    
    return result;
}

/**
 * Set a dynamic property value
 * Creates the property if it doesn't exist
 */
void __dynamic_property_set(void* object_ptr, const char* property_name, void* dynamic_value) {
    if (!object_ptr || !property_name || !dynamic_value) {
        return;
    }
    
    std::cout << "[DYNAMIC] Setting property '" << property_name << "' on object " << object_ptr 
              << " to value " << dynamic_value << std::endl;
    
    // Ensure the object has a dynamic property map
    __ensure_dynamic_map(object_ptr);
    
    DynamicPropertyMap* map = __get_dynamic_map(object_ptr);
    if (map) {
        // Clone the DynamicValue to store in the map
        DynamicValue* source = static_cast<DynamicValue*>(dynamic_value);
        DynamicValue* copy = new DynamicValue(*source);
        
        map->set(std::string(property_name), copy);
        
        std::cout << "[DYNAMIC] Successfully set property '" << property_name 
                  << "', map now has " << map->property_count << " properties" << std::endl;
    }
}

/**
 * Check if a dynamic property exists
 * Returns 1 if exists, 0 if not
 */
int __dynamic_property_has(void* object_ptr, const char* property_name) {
    if (!object_ptr || !property_name) {
        return 0;
    }
    
    DynamicPropertyMap* map = __get_dynamic_map(object_ptr);
    if (!map) {
        return 0;
    }
    
    bool exists = map->has(std::string(property_name));
    std::cout << "[DYNAMIC] Property '" << property_name << "' " 
              << (exists ? "exists" : "does not exist") << std::endl;
    
    return exists ? 1 : 0;
}

/**
 * Delete a dynamic property
 * Returns 1 if deleted, 0 if not found
 */
int __dynamic_property_delete(void* object_ptr, const char* property_name) {
    if (!object_ptr || !property_name) {
        return 0;
    }
    
    std::cout << "[DYNAMIC] Deleting property '" << property_name << "' from object " << object_ptr << std::endl;
    
    DynamicPropertyMap* map = __get_dynamic_map(object_ptr);
    if (!map) {
        return 0;
    }
    
    bool deleted = map->remove(std::string(property_name));
    if (deleted) {
        std::cout << "[DYNAMIC] Successfully deleted property '" << property_name 
                  << "', map now has " << map->property_count << " properties" << std::endl;
    }
    
    return deleted ? 1 : 0;
}

/**
 * Get all dynamic property keys for for...in loops
 * Returns an array of strings
 */
void* __dynamic_property_keys(void* object_ptr) {
    if (!object_ptr) {
        return nullptr;
    }
    
    DynamicPropertyMap* map = __get_dynamic_map(object_ptr);
    if (!map) {
        // Return empty array
        return nullptr;
    }
    
    std::vector<std::string> keys = map->get_keys();
    std::cout << "[DYNAMIC] Retrieved " << keys.size() << " dynamic property keys" << std::endl;
    
    // For now, return the map itself - the caller can iterate over it
    // TODO: Create a proper string array for the runtime
    return map;
}

/**
 * Create a DynamicValue from any type with type information
 */
void* __dynamic_value_create_any(void* value, int type_id) {
    DataType type = static_cast<DataType>(type_id);
    
    switch (type) {
        case DataType::INT64: {
            int64_t int_val = *static_cast<int64_t*>(value);
            return new DynamicValue(int_val);
        }
        case DataType::FLOAT64: {
            double double_val = *static_cast<double*>(value);
            return new DynamicValue(double_val);
        }
        case DataType::BOOLEAN: {
            bool bool_val = *static_cast<bool*>(value);
            return new DynamicValue(bool_val);
        }
        case DataType::STRING: {
            // Assuming value is a GoTSString pointer
            // TODO: Convert GoTSString to std::string
            const char* str_val = static_cast<const char*>(value);
            return new DynamicValue(std::string(str_val));
        }
        case DataType::CLASS_INSTANCE:
        case DataType::ARRAY: {
            // Store as void pointer
            return new DynamicValue(value);
        }
        default: {
            // Default to storing as pointer
            return new DynamicValue(value);
        }
    }
}

} // extern "C"

// Global class metadata for for-in loop support
// This would ideally be populated by the compiler during class compilation
static std::unordered_map<std::string, std::vector<std::string>> class_property_names;

extern "C" {

/**
 * Debug function to trace loop variables
 */
void __debug_loop_compare(int64_t index, int64_t count) {
    std::cout << "[LOOP-DEBUG] Comparing index=" << index << " with count=" << count 
              << " (index >= count is " << (index >= count ? "true" : "false") << ")" << std::endl;
    std::cout << "[LOOP-DEBUG] Should " << (index >= count ? "EXIT" : "CONTINUE") << " loop" << std::endl;
}

/**
 * Debug function to indicate loop body is entered
 */
void __debug_loop_body_entered() {
    std::cout << "[LOOP-DEBUG] Loop body entered!" << std::endl;
}

// Debug function to trace before loop body
void __debug_before_loop_body() {
    std::cout << "[LOOP-DEBUG] About to enter loop body - comparison passed!" << std::endl;
}

/**
 * Get the number of static class properties for an object
 */
int64_t __get_class_property_count(void* object_ptr) {
    std::cout << "[FOR-IN-DEBUG] __get_class_property_count called with object_ptr=" << object_ptr << std::endl;
    std::cout.flush();
    
    if (!object_ptr) return 0;
    
    // Get class name from object layout  
    int64_t* obj = static_cast<int64_t*>(object_ptr);
    const char* class_name_ptr = reinterpret_cast<const char*>(obj[0]);
    
    if (!class_name_ptr) return 0;
    
    // Get the current compiler context to access class metadata
    auto* compiler = get_current_compiler();
    if (!compiler) return 0;
    
    std::string class_name(class_name_ptr);
    auto* class_info = compiler->get_class(class_name);
    if (!class_info) return 0;
    
    return static_cast<int64_t>(class_info->fields.size());
}

/**
 * Get the name of a static class property by index
 */
const char* __get_class_property_name(void* object_ptr, int64_t index) {
    if (!object_ptr) return nullptr;
    
    // Get class name from object layout  
    int64_t* obj = static_cast<int64_t*>(object_ptr);
    const char* class_name_ptr = reinterpret_cast<const char*>(obj[0]);
    
    if (!class_name_ptr) return nullptr;
    
    // Get the current compiler context to access class metadata
    auto* compiler = get_current_compiler();
    if (!compiler) return nullptr;
    
    std::string class_name(class_name_ptr);
    auto* class_info = compiler->get_class(class_name);
    if (!class_info) return nullptr;
    
    if (index >= 0 && index < static_cast<int64_t>(class_info->fields.size())) {
        const char* prop_name = class_info->fields[index].name.c_str();
        return prop_name;
    }
    
    return nullptr;
}

extern "C" void __debug_reached_static_loop_body() {
    std::cout << "[DEBUG] *** REACHED STATIC LOOP BODY ***" << std::endl;
    std::cout.flush();
}

extern "C" void __debug_reached_static_loop_body_with_values(int64_t index, int64_t count) {
    std::cout << "[DEBUG] *** REACHED STATIC LOOP BODY *** index=" << index << " count=" << count << std::endl;
    std::cout.flush();
}

extern "C" void __debug_about_to_call_property_name() {
    std::cout << "[DEBUG] *** ABOUT TO CALL __get_class_property_name ***" << std::endl;
    std::cout.flush();
}

/**
 * Get the number of dynamic properties for an object
 */
int64_t __get_dynamic_property_count(void* object_ptr) {
    if (!object_ptr) return 0;
    
    DynamicPropertyMap* map = __get_dynamic_map(object_ptr);
    if (!map) {
        std::cout << "[FOR-IN] No dynamic properties (map not initialized)" << std::endl;
        return 0;
    }
    
    std::cout << "[FOR-IN] Object has " << map->property_count << " dynamic properties" << std::endl;
    return static_cast<int64_t>(map->property_count);
}

/**
 * Get the name of a dynamic property by index
 */
const char* __get_dynamic_property_name(void* object_ptr, int64_t index) {
    if (!object_ptr) return nullptr;
    
    DynamicPropertyMap* map = __get_dynamic_map(object_ptr);
    if (!map) return nullptr;
    
    // Convert the unordered_map to a vector for indexed access
    std::vector<std::string> keys = map->get_keys();
    
    if (index >= 0 && index < static_cast<int64_t>(keys.size())) {
        // Return a pointer to the string - this is a bit unsafe but works for the immediate iteration
        // In a production system, we'd want to use a more robust string management approach
        static std::string current_key;
        current_key = keys[index];
        std::cout << "[FOR-IN] Dynamic property at index " << index << " is '" << current_key << "'" << std::endl;
        return current_key.c_str();
    }
    
    return nullptr;
}

} // extern "C"
