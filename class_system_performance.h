#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <cstdint>
#include <cstring>
#include <atomic>
#include <mutex>

namespace ultraScript {

// Debug flags for tracing property access paths
#ifndef NDEBUG
#define ULTRASCRIPT_DEBUG_PROPERTY_ACCESS 1
#define ULTRASCRIPT_DEBUG_CODE_GENERATION 1
#define ULTRASCRIPT_DEBUG_CLASS_METADATA 1
#else
#define ULTRASCRIPT_DEBUG_PROPERTY_ACCESS 0
#define ULTRASCRIPT_DEBUG_CODE_GENERATION 0
#define ULTRASCRIPT_DEBUG_CLASS_METADATA 0
#endif

// Debug helper macros
#if ULTRASCRIPT_DEBUG_PROPERTY_ACCESS
#define DEBUG_PROPERTY_ACCESS(msg) do { std::cout << "[PROPERTY_ACCESS] " << msg << std::endl; } while(0)
#else
#define DEBUG_PROPERTY_ACCESS(msg) ((void)0)
#endif

#if ULTRASCRIPT_DEBUG_CODE_GENERATION
#define DEBUG_CODEGEN(msg) do { std::cout << "[CODEGEN] " << msg << std::endl; } while(0)
#else
#define DEBUG_CODEGEN(msg) ((void)0)
#endif

#if ULTRASCRIPT_DEBUG_CLASS_METADATA
#define DEBUG_CLASS_META(msg) do { std::cout << "[CLASS_META] " << msg << std::endl; } while(0)
#else
#define DEBUG_CLASS_META(msg) ((void)0)
#endif

// Forward declarations
struct DynamicValue;
class CodeGenerator;

// Fast string hashing using FNV-1a for property lookups
constexpr uint32_t FNV_OFFSET_BASIS_32 = 2166136261u;
constexpr uint32_t FNV_PRIME_32 = 16777619u;

constexpr uint32_t hash_property_name(const char* str, size_t len) {
    uint32_t hash = FNV_OFFSET_BASIS_32;
    for (size_t i = 0; i < len; ++i) {
        hash ^= static_cast<uint32_t>(str[i]);
        hash *= FNV_PRIME_32;
    }
    return hash;
}

inline uint32_t hash_property_name(const char* str) {
    return hash_property_name(str, strlen(str));
}

inline uint32_t hash_property_name(const std::string& str) {
    return hash_property_name(str.c_str(), str.length());
}

// Object type IDs for ultra-fast type checking
enum class ObjectTypeId : uint32_t {
    UNKNOWN = 0,
    // Built-in types
    STRING = 1,
    ARRAY = 2,
    FUNCTION = 3,
    DATE = 4,
    REGEX = 5,
    // User-defined classes start here
    USER_CLASS_BASE = 1000
};

// Property type IDs for optimal code generation
enum class PropertyType : uint8_t {
    DYNAMIC = 0,     // DynamicValue - slowest but most flexible
    INT64 = 1,       // Native int64_t - fastest integer
    FLOAT64 = 2,     // Native double - fastest float
    STRING = 3,      // String pointer - string operations
    OBJECT_PTR = 4,  // Pointer to another object
    BOOL = 5,        // Native bool
    INT32 = 6,       // Native int32_t
    FLOAT32 = 7,     // Native float
    UINT64 = 8,      // Native uint64_t
    UINT32 = 9       // Native uint32_t
};

// Property access flags for optimization
enum PropertyFlags : uint8_t {
    NONE = 0,
    READONLY = 1,
    STATIC = 2,
    PRIVATE = 4,
    PROTECTED = 8,
    COMPUTED = 16  // Property has getter/setter
};

// Size mapping for each property type
constexpr size_t get_property_type_size(PropertyType type) {
    switch (type) {
        case PropertyType::DYNAMIC: return sizeof(DynamicValue);
        case PropertyType::INT64: return sizeof(int64_t);
        case PropertyType::FLOAT64: return sizeof(double);
        case PropertyType::STRING: return sizeof(char*);
        case PropertyType::OBJECT_PTR: return sizeof(void*);
        case PropertyType::BOOL: return sizeof(bool);
        case PropertyType::INT32: return sizeof(int32_t);
        case PropertyType::FLOAT32: return sizeof(float);
        case PropertyType::UINT64: return sizeof(uint64_t);
        case PropertyType::UINT32: return sizeof(uint32_t);
        default: return sizeof(DynamicValue);
    }
}

// Alignment requirements for each property type
constexpr size_t get_property_type_alignment(PropertyType type) {
    switch (type) {
        case PropertyType::DYNAMIC: return alignof(DynamicValue);
        case PropertyType::INT64: return alignof(int64_t);
        case PropertyType::FLOAT64: return alignof(double);
        case PropertyType::STRING: return alignof(char*);
        case PropertyType::OBJECT_PTR: return alignof(void*);
        case PropertyType::BOOL: return alignof(bool);
        case PropertyType::INT32: return alignof(int32_t);
        case PropertyType::FLOAT32: return alignof(float);
        case PropertyType::UINT64: return alignof(uint64_t);
        case PropertyType::UINT32: return alignof(uint32_t);
        default: return alignof(DynamicValue);
    }
}

// Property descriptor for compile-time optimizations
struct PropertyDescriptor {
    std::string name;           // Property name for debugging/dynamic access
    uint32_t name_hash;         // FNV-1a hash of property name
    uint32_t offset;            // Byte offset from object data start
    PropertyType type;          // Property type for code generation
    PropertyFlags flags;        // Property flags
    uint16_t index;             // Property index for ultra-fast access
    
    PropertyDescriptor() = default;
    PropertyDescriptor(const std::string& prop_name, PropertyType prop_type, PropertyFlags prop_flags = PropertyFlags::NONE)
        : name(prop_name), name_hash(hash_property_name(prop_name)), type(prop_type), flags(prop_flags) {}
};

// Class metadata for JIT compilation
class ClassMetadata {
public:
    std::string class_name;
    ObjectTypeId type_id;
    std::string parent_class_name;  // For inheritance
    ClassMetadata* parent_metadata; // Cached parent metadata pointer
    
    // Property storage
    std::vector<PropertyDescriptor> properties;
    std::unordered_map<uint32_t, uint16_t> property_hash_to_index; // Hash -> property index
    std::unordered_map<std::string, uint16_t> property_name_to_index; // Name -> property index
    
    // Memory layout information
    uint32_t instance_size;      // Total size needed for object instance
    uint32_t data_size;         // Size needed for property data only
    
    // Constructor and method information
    void* constructor_ptr;       // Direct function pointer to constructor
    std::unordered_map<std::string, void*> method_ptrs; // Method name -> function pointer
    
    // Inheritance chain caching
    std::vector<ClassMetadata*> inheritance_chain; // For faster inheritance lookups
    
    ClassMetadata(const std::string& name, ObjectTypeId tid) 
        : class_name(name), type_id(tid), parent_metadata(nullptr), 
          instance_size(sizeof(ObjectHeader)), data_size(0), constructor_ptr(nullptr) {}
    
    // Add a property to this class
    uint16_t add_property(const std::string& prop_name, PropertyType prop_type, PropertyFlags flags = PropertyFlags::NONE);
    
    // Find property by name (compile-time)
    const PropertyDescriptor* find_property(const std::string& name) const;
    
    // Find property by hash (runtime)
    const PropertyDescriptor* find_property_by_hash(uint32_t hash) const;
    
    // Get property index by name (fastest for known properties)
    int16_t get_property_index(const std::string& name) const;
    
    // Set parent class for inheritance
    void set_parent_class(const std::string& parent_name);
    
    // Calculate final memory layout (call after all properties added)
    void finalize_layout();
    
    // Check if this class inherits from another
    bool inherits_from(const std::string& ancestor_class_name) const;
    
private:
    void calculate_property_offsets();
    void build_inheritance_chain();
};

// Object header - embedded at start of every object
struct ObjectHeader {
    ObjectTypeId type_id;           // Type ID for ultra-fast type checking
    uint32_t ref_count;             // Reference count for GC
    uint16_t property_count;        // Number of properties in this instance
    uint16_t flags;                 // Object flags (finalized, etc.)
};

// High-performance object instance
class ObjectInstance {
public:
    ObjectHeader header;
    char data[];  // Property data follows immediately after header
    
    // Ultra-fast property access by index (compiled code path - FASTEST)
    template<typename T>
    T* get_property_by_index(uint16_t property_index) {
        DEBUG_PROPERTY_ACCESS("ULTRA-FAST PATH: Accessing property by index " << property_index);
        
        ClassMetadata* meta = ClassRegistry::instance().get_class_metadata(header.type_id);
        if (!meta || property_index >= meta->properties.size()) {
            DEBUG_PROPERTY_ACCESS("ULTRA-FAST PATH: Failed - invalid metadata or index");
            return nullptr;
        }
        
        uint32_t offset = meta->properties[property_index].offset;
        DEBUG_PROPERTY_ACCESS("ULTRA-FAST PATH: Found property '" << meta->properties[property_index].name 
                             << "' at offset " << offset << " (type: " << static_cast<int>(meta->properties[property_index].type) << ")");
        return reinterpret_cast<T*>(data + offset);
    }
    
    // Fast property access by name hash (dynamic code path)
    void* get_property_by_hash(uint32_t name_hash) {
        DEBUG_PROPERTY_ACCESS("DYNAMIC PATH: Accessing property by hash 0x" << std::hex << name_hash << std::dec);
        
        ClassMetadata* meta = ClassRegistry::instance().get_class_metadata(header.type_id);
        if (!meta) {
            DEBUG_PROPERTY_ACCESS("DYNAMIC PATH: Failed - no metadata for type " << static_cast<uint32_t>(header.type_id));
            return nullptr;
        }
        
        DEBUG_PROPERTY_ACCESS("DYNAMIC PATH: Checking class '" << meta->class_name << "'");
        
        // Check this class first
        auto it = meta->property_hash_to_index.find(name_hash);
        if (it != meta->property_hash_to_index.end()) {
            DEBUG_PROPERTY_ACCESS("DYNAMIC PATH: Found in class - property index " << it->second);
            return get_property_by_index<void>(it->second);
        }
        
        // Check inheritance chain
        DEBUG_PROPERTY_ACCESS("DYNAMIC PATH: Not found in class, checking " << meta->inheritance_chain.size() << " ancestors");
        for (ClassMetadata* ancestor : meta->inheritance_chain) {
            DEBUG_PROPERTY_ACCESS("DYNAMIC PATH: Checking ancestor '" << ancestor->class_name << "'");
            auto ancestor_it = ancestor->property_hash_to_index.find(name_hash);
            if (ancestor_it != ancestor->property_hash_to_index.end()) {
                DEBUG_PROPERTY_ACCESS("DYNAMIC PATH: Found in ancestor - property index " << ancestor_it->second);
                return get_property_by_index<void>(ancestor_it->second);
            }
        }
        
        // Not found in class hierarchy - check dynamic properties
        DEBUG_PROPERTY_ACCESS("DYNAMIC PATH: Not found in class hierarchy, checking dynamic properties");
        return get_dynamic_property_by_hash(name_hash);
    }
    
    // Property access by name (slowest - for debugging)
    void* get_property_by_name(const char* property_name) {
        if (!property_name) {
            DEBUG_PROPERTY_ACCESS("NAME PATH: Failed - null property name");
            return nullptr;
        }
        DEBUG_PROPERTY_ACCESS("NAME PATH: Accessing property '" << property_name << "'");
        return get_property_by_hash(hash_property_name(property_name));
    }
    
    // Ultra-fast property setting by index
    template<typename T>
    bool set_property_by_index(uint16_t property_index, const T& value) {
        DEBUG_PROPERTY_ACCESS("ULTRA-FAST SET: Setting property by index " << property_index);
        T* prop_ptr = get_property_by_index<T>(property_index);
        if (!prop_ptr) {
            DEBUG_PROPERTY_ACCESS("ULTRA-FAST SET: Failed - invalid property pointer");
            return false;
        }
        *prop_ptr = value;
        DEBUG_PROPERTY_ACCESS("ULTRA-FAST SET: Successfully set property");
        return true;
    }
    
    // Set property by name hash
    bool set_property_by_hash(uint32_t name_hash, const void* value, size_t value_size) {
        DEBUG_PROPERTY_ACCESS("DYNAMIC SET: Setting property by hash 0x" << std::hex << name_hash << std::dec);
        void* prop_ptr = get_property_by_hash(name_hash);
        if (prop_ptr) {
            DEBUG_PROPERTY_ACCESS("DYNAMIC SET: Found class property, copying " << value_size << " bytes");
            memcpy(prop_ptr, value, value_size);
            return true;
        }
        
        // Property not found in class - add to dynamic properties
        DEBUG_PROPERTY_ACCESS("DYNAMIC SET: Not found in class, adding to dynamic properties");
        return set_dynamic_property_by_hash(name_hash, value, value_size);
    }
    
    // Dynamic property access (for unregistered properties)
    void* get_dynamic_property_by_hash(uint32_t name_hash);
    bool set_dynamic_property_by_hash(uint32_t name_hash, const void* value, size_t value_size);
    
    // Get dynamic properties map (lazy initialization)
    std::unordered_map<uint32_t, DynamicValue>* get_dynamic_properties_map();
    
private:
    // Dynamic properties map stored after object data (lazy initialization)
    std::unordered_map<uint32_t, DynamicValue>* dynamic_properties = nullptr;
};

// Global class registry for compile-time and runtime optimizations
class ClassRegistry {
private:
    std::unordered_map<std::string, std::unique_ptr<ClassMetadata>> class_name_to_metadata;
    std::unordered_map<ObjectTypeId, ClassMetadata*> type_id_to_metadata;
    std::atomic<uint32_t> next_type_id{static_cast<uint32_t>(ObjectTypeId::USER_CLASS_BASE)};
    std::mutex registry_mutex;
    
public:
    static ClassRegistry& instance() {
        static ClassRegistry registry;
        return registry;
    }
    
    // Register a new class (compile-time)
    ObjectTypeId register_class(const std::string& class_name);
    
    // Get class metadata by name
    ClassMetadata* get_class_metadata(const std::string& class_name);
    
    // Get class metadata by type ID (fastest)
    ClassMetadata* get_class_metadata(ObjectTypeId type_id);
    
    // Check if class exists
    bool class_exists(const std::string& class_name);
    
    // Set inheritance relationship
    void set_inheritance(const std::string& child_class, const std::string& parent_class);
    
    // Finalize all registered classes (call after registration complete)
    void finalize_all_classes();
};

// Object factory for creating instances
class ObjectFactory {
public:
    // Create object instance by class name
    static ObjectInstance* create_object(const std::string& class_name);
    
    // Create object instance by type ID (fastest)
    static ObjectInstance* create_object(ObjectTypeId type_id);
    
    // Create object with constructor arguments
    static ObjectInstance* create_object_with_args(const std::string& class_name, 
                                                 const std::vector<DynamicValue>& args);
    
    // Destroy object instance
    static void destroy_object(ObjectInstance* obj);
    
private:
    static ObjectInstance* allocate_object(ClassMetadata* meta);
    static void initialize_object(ObjectInstance* obj, ClassMetadata* meta);
};

// JIT code generation for class operations
class ClassCodeGenerator {
public:
    // Generate code for property access by index (ultra-fast path)
    static void generate_property_access_by_index(CodeGenerator& gen, uint16_t property_index, PropertyType prop_type);
    
    // Generate code for property access by hash (dynamic path)
    static void generate_property_access_by_hash(CodeGenerator& gen, uint32_t name_hash);
    
    // Generate code for property assignment by index
    static void generate_property_assignment_by_index(CodeGenerator& gen, uint16_t property_index, PropertyType prop_type);
    
    // Generate code for object construction
    static void generate_object_construction(CodeGenerator& gen, ObjectTypeId type_id);
    
    // Generate code for method calls
    static void generate_method_call(CodeGenerator& gen, const std::string& class_name, const std::string& method_name);
    
    // Generate code for inheritance checks
    static void generate_instanceof_check(CodeGenerator& gen, const std::string& class_name);
    
    // Debug helpers for generated code
    static void emit_debug_property_access(CodeGenerator& gen, const std::string& path_type, const std::string& info);
    static void emit_debug_property_set(CodeGenerator& gen, const std::string& path_type, const std::string& info);
};

} // namespace ultraScript
