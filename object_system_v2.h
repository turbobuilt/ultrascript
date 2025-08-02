#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <atomic>
#include <cstdint>
#include <cstring>

namespace ultraScript {

// Fast string hashing for property names
constexpr uint32_t hash_string_const(const char* str, size_t len) {
    uint32_t hash = 2166136261u;
    for (size_t i = 0; i < len; ++i) {
        hash ^= static_cast<uint32_t>(str[i]);
        hash *= 16777619u;
    }
    return hash;
}

inline uint32_t hash_string(const char* str) {
    return hash_string_const(str, strlen(str));
}

// Object type IDs for fast type checking
enum class ObjectTypeId : uint32_t {
    UNKNOWN = 0,
    STRING = 1,
    ARRAY = 2,
    OBJECT = 3,
    FUNCTION = 4,
    // User-defined classes start at 1000
    USER_CLASS_BASE = 1000
};

// Property type IDs for optimization
enum class PropertyTypeId : uint16_t {
    ANY = 0,
    INT64 = 1,
    FLOAT64 = 2,
    STRING = 3,
    OBJECT = 4,
    ARRAY = 5,
    BOOL = 6
};

// Property flags
enum PropertyFlags : uint16_t {
    NONE = 0,
    READONLY = 1,
    STATIC = 2,
    COMPUTED = 4
};

// Object header - embedded at start of every object
struct ObjectHeader {
    uint32_t type_id;        // ObjectTypeId for fast type checking
    uint32_t ref_count;      // Reference counting for GC
    uint32_t property_count; // Number of properties in this instance
    uint32_t instance_size;  // Total size of this object instance
};

// Property descriptor for fast access
struct PropertyDescriptor {
    uint32_t name_hash;      // FNV-1a hash of property name
    uint32_t offset;         // Byte offset from start of object data
    PropertyTypeId type_id;  // Property type for optimization
    PropertyFlags flags;     // Property flags
    std::string name;        // Actual property name (for debugging/dynamic access)
};

// Class metadata registry
struct ClassMetadata {
    std::string class_name;
    ObjectTypeId type_id;
    std::vector<PropertyDescriptor> properties;
    std::unordered_map<uint32_t, uint16_t> property_hash_to_index; // hash -> property index
    uint32_t base_instance_size;  // Size needed for all properties
    void* constructor_ptr;        // Direct function pointer to constructor
    
    ClassMetadata(const std::string& name, ObjectTypeId tid) 
        : class_name(name), type_id(tid), base_instance_size(sizeof(ObjectHeader)), constructor_ptr(nullptr) {}
    
    // Add a property to this class
    void add_property(const std::string& prop_name, PropertyTypeId prop_type, uint32_t prop_size) {
        PropertyDescriptor desc;
        desc.name = prop_name;
        desc.name_hash = hash_string(prop_name.c_str());
        desc.type_id = prop_type;
        desc.flags = PropertyFlags::NONE;
        desc.offset = base_instance_size;
        
        properties.push_back(desc);
        property_hash_to_index[desc.name_hash] = static_cast<uint16_t>(properties.size() - 1);
        
        // Align to 8-byte boundaries for performance
        base_instance_size += ((prop_size + 7) & ~7);
    }
};

// Global class registry
class ClassRegistry {
private:
    std::unordered_map<std::string, std::unique_ptr<ClassMetadata>> class_name_to_metadata;
    std::unordered_map<ObjectTypeId, ClassMetadata*> type_id_to_metadata;
    std::atomic<uint32_t> next_type_id{static_cast<uint32_t>(ObjectTypeId::USER_CLASS_BASE)};
    
public:
    static ClassRegistry& instance() {
        static ClassRegistry registry;
        return registry;
    }
    
    // Register a new class
    ObjectTypeId register_class(const std::string& class_name) {
        auto it = class_name_to_metadata.find(class_name);
        if (it != class_name_to_metadata.end()) {
            return it->second->type_id;
        }
        
        ObjectTypeId type_id = static_cast<ObjectTypeId>(next_type_id.fetch_add(1));
        auto metadata = std::make_unique<ClassMetadata>(class_name, type_id);
        
        ClassMetadata* meta_ptr = metadata.get();
        class_name_to_metadata[class_name] = std::move(metadata);
        type_id_to_metadata[type_id] = meta_ptr;
        
        return type_id;
    }
    
    // Get class metadata by name
    ClassMetadata* get_class_metadata(const std::string& class_name) {
        auto it = class_name_to_metadata.find(class_name);
        return (it != class_name_to_metadata.end()) ? it->second.get() : nullptr;
    }
    
    // Get class metadata by type ID (fastest)
    ClassMetadata* get_class_metadata(ObjectTypeId type_id) {
        auto it = type_id_to_metadata.find(type_id);
        return (it != type_id_to_metadata.end()) ? it->second : nullptr;
    }
};

// High-performance object instance
class ObjectInstance {
public:
    ObjectHeader header;
    char data[];  // Property data follows immediately
    
    // Get property by index (compiled code path - FASTEST)
    template<typename T>
    T* get_property_by_index(uint16_t property_index) {
        if (property_index >= header.property_count) return nullptr;
        
        ClassMetadata* meta = ClassRegistry::instance().get_class_metadata(
            static_cast<ObjectTypeId>(header.type_id));
        if (!meta || property_index >= meta->properties.size()) return nullptr;
        
        uint32_t offset = meta->properties[property_index].offset - sizeof(ObjectHeader);
        return reinterpret_cast<T*>(data + offset);
    }
    
    // Get property by name hash (dynamic code path - FAST)
    void* get_property_by_hash(uint32_t name_hash) {
        ClassMetadata* meta = ClassRegistry::instance().get_class_metadata(
            static_cast<ObjectTypeId>(header.type_id));
        if (!meta) return nullptr;
        
        auto it = meta->property_hash_to_index.find(name_hash);
        if (it == meta->property_hash_to_index.end()) return nullptr;
        
        return get_property_by_index<void>(it->second);
    }
    
    // Get property by name (slowest - for debugging/runtime)
    void* get_property_by_name(const char* property_name) {
        if (!property_name) return nullptr;
        return get_property_by_hash(hash_string(property_name));
    }
    
    // Set property by index (compiled code path)
    template<typename T>
    bool set_property_by_index(uint16_t property_index, const T& value) {
        T* prop_ptr = get_property_by_index<T>(property_index);
        if (!prop_ptr) return false;
        *prop_ptr = value;
        return true;
    }
    
    // Set property by name hash (dynamic code path)
    bool set_property_by_hash(uint32_t name_hash, const void* value, size_t value_size) {
        void* prop_ptr = get_property_by_hash(name_hash);
        if (!prop_ptr) return false;
        memcpy(prop_ptr, value, value_size);
        return true;
    }
};

// Object creation and management
class ObjectManager {
public:
    // Create object instance of a given class
    static ObjectInstance* create_object(const std::string& class_name) {
        ClassMetadata* meta = ClassRegistry::instance().get_class_metadata(class_name);
        if (!meta) return nullptr;
        
        // Allocate memory for object header + all properties
        size_t total_size = meta->base_instance_size;
        ObjectInstance* obj = reinterpret_cast<ObjectInstance*>(malloc(total_size));
        if (!obj) return nullptr;
        
        // Initialize header
        obj->header.type_id = static_cast<uint32_t>(meta->type_id);
        obj->header.ref_count = 1;
        obj->header.property_count = static_cast<uint32_t>(meta->properties.size());
        obj->header.instance_size = static_cast<uint32_t>(total_size);
        
        // Zero-initialize all property data
        memset(obj->data, 0, total_size - sizeof(ObjectHeader));
        
        return obj;
    }
    
    // Create object with type ID (fastest path)
    static ObjectInstance* create_object(ObjectTypeId type_id) {
        ClassMetadata* meta = ClassRegistry::instance().get_class_metadata(type_id);
        if (!meta) return nullptr;
        
        size_t total_size = meta->base_instance_size;
        ObjectInstance* obj = reinterpret_cast<ObjectInstance*>(malloc(total_size));
        if (!obj) return nullptr;
        
        obj->header.type_id = static_cast<uint32_t>(type_id);
        obj->header.ref_count = 1;
        obj->header.property_count = static_cast<uint32_t>(meta->properties.size());
        obj->header.instance_size = static_cast<uint32_t>(total_size);
        
        memset(obj->data, 0, total_size - sizeof(ObjectHeader));
        
        return obj;
    }
    
    // Destroy object instance
    static void destroy_object(ObjectInstance* obj) {
        if (obj) {
            // TODO: Call destructors for object properties if needed
            free(obj);
        }
    }
};

// Compile-time property offset calculation helpers
#define PROPERTY_OFFSET(ClassName, PropertyName) \
    (ClassRegistry::instance().get_class_metadata(#ClassName)->property_hash_to_index.at(hash_string(#PropertyName)))

} // namespace ultraScript
