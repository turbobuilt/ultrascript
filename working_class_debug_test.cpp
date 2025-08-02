#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <cstdlib>

// Quick debug macros
#define DEBUG_CLASS_META(msg) do { std::cout << "[CLASS_META] " << msg << std::endl; } while(0)

namespace ultraScript {

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

inline uint32_t hash_property_name(const std::string& str) {
    return hash_property_name(str.c_str(), str.length());
}

// Object type IDs for ultra-fast type checking
enum class ObjectTypeId : uint32_t {
    UNKNOWN = 0,
    USER_CLASS_BASE = 1000
};

// Property type IDs for optimal code generation
enum class PropertyType : uint8_t {
    DYNAMIC = 0,
    INT64 = 1,
    FLOAT64 = 2,
    STRING = 3,
    OBJECT_PTR = 4,
    BOOL = 5
};

// Property access flags
enum PropertyFlags : uint8_t {
    NONE = 0,
    READONLY = 1
};

// Size mapping for each property type
constexpr size_t get_property_type_size(PropertyType type) {
    switch (type) {
        case PropertyType::INT64: return sizeof(int64_t);
        case PropertyType::FLOAT64: return sizeof(double);
        case PropertyType::STRING: return sizeof(char*);
        case PropertyType::OBJECT_PTR: return sizeof(void*);
        case PropertyType::BOOL: return sizeof(bool);
        default: return sizeof(void*);
    }
}

// Alignment requirements
constexpr size_t get_property_type_alignment(PropertyType type) {
    switch (type) {
        case PropertyType::INT64: return alignof(int64_t);
        case PropertyType::FLOAT64: return alignof(double);
        case PropertyType::STRING: return alignof(char*);
        case PropertyType::OBJECT_PTR: return alignof(void*);
        case PropertyType::BOOL: return alignof(bool);
        default: return alignof(void*);
    }
}

// Property descriptor
struct PropertyDescriptor {
    std::string name;
    uint32_t name_hash;
    uint32_t offset;
    PropertyType type;
    PropertyFlags flags;
    uint16_t index;
    
    PropertyDescriptor() = default;
    PropertyDescriptor(const std::string& prop_name, PropertyType prop_type, PropertyFlags prop_flags = PropertyFlags::NONE)
        : name(prop_name), name_hash(hash_property_name(prop_name)), type(prop_type), flags(prop_flags) {}
};

// Object header
struct ObjectHeader {
    ObjectTypeId type_id;
    uint32_t ref_count;
    uint16_t property_count;
    uint16_t flags;
};

// Class metadata
class ClassMetadata {
public:
    std::string class_name;
    ObjectTypeId type_id;
    std::vector<PropertyDescriptor> properties;
    std::unordered_map<uint32_t, uint16_t> property_hash_to_index;
    std::unordered_map<std::string, uint16_t> property_name_to_index;
    uint32_t instance_size;
    uint32_t data_size;
    
    ClassMetadata(const std::string& name, ObjectTypeId tid) 
        : class_name(name), type_id(tid), instance_size(sizeof(ObjectHeader)), data_size(0) {}
    
    uint16_t add_property(const std::string& prop_name, PropertyType prop_type, PropertyFlags flags = PropertyFlags::NONE) {
        DEBUG_CLASS_META("Adding property '" << prop_name << "' to class '" << class_name << "' (type: " << static_cast<int>(prop_type) << ")");
        
        auto it = property_name_to_index.find(prop_name);
        if (it != property_name_to_index.end()) {
            DEBUG_CLASS_META("Property '" << prop_name << "' already exists with index " << it->second);
            return it->second;
        }
        
        PropertyDescriptor prop(prop_name, prop_type, flags);
        prop.index = static_cast<uint16_t>(properties.size());
        
        properties.push_back(prop);
        property_hash_to_index[prop.name_hash] = prop.index;
        property_name_to_index[prop_name] = prop.index;
        
        DEBUG_CLASS_META("Property '" << prop_name << "' added with index " << prop.index << " and hash 0x" << std::hex << prop.name_hash << std::dec);
        
        return prop.index;
    }
    
    const PropertyDescriptor* find_property(const std::string& name) const {
        DEBUG_CLASS_META("COMPILE-TIME: Finding property '" << name << "' in class '" << class_name << "'");
        
        auto it = property_name_to_index.find(name);
        if (it != property_name_to_index.end()) {
            DEBUG_CLASS_META("COMPILE-TIME: Found property '" << name << "' at index " << it->second << " - WILL EMIT DIRECT OFFSET ASM");
            return &properties[it->second];
        }
        
        DEBUG_CLASS_META("COMPILE-TIME: Property '" << name << "' not found in class '" << class_name << "'");
        return nullptr;
    }
    
    const PropertyDescriptor* find_property_by_hash(uint32_t hash) const {
        DEBUG_CLASS_META("RUNTIME: Finding property by hash 0x" << std::hex << hash << std::dec << " in class '" << class_name << "'");
        
        auto it = property_hash_to_index.find(hash);
        if (it != property_hash_to_index.end()) {
            DEBUG_CLASS_META("RUNTIME: Found property by hash at index " << it->second << " ('" << properties[it->second].name << "') - USING HASH LOOKUP PATH");
            return &properties[it->second];
        }
        
        DEBUG_CLASS_META("RUNTIME: Property not found by hash in class '" << class_name << "' - WILL CHECK DYNAMIC PROPERTIES");
        return nullptr;
    }
    
    void finalize_layout() {
        DEBUG_CLASS_META("Finalizing layout for class '" << class_name << "' with " << properties.size() << " properties");
        
        uint32_t current_offset = 0;
        for (PropertyDescriptor& prop : properties) {
            size_t alignment = get_property_type_alignment(prop.type);
            current_offset = (current_offset + alignment - 1) & ~(alignment - 1);
            prop.offset = current_offset;
            current_offset += static_cast<uint32_t>(get_property_type_size(prop.type));
            DEBUG_CLASS_META("Property '" << prop.name << "' offset: " << prop.offset << ", size: " << get_property_type_size(prop.type));
        }
        
        data_size = current_offset;
        instance_size = sizeof(ObjectHeader) + data_size;
        
        DEBUG_CLASS_META("Class '" << class_name << "' layout finalized - instance_size: " << instance_size << ", data_size: " << data_size);
    }
};

// Global class registry
class ClassRegistry {
private:
    std::unordered_map<std::string, std::unique_ptr<ClassMetadata>> class_name_to_metadata;
    std::atomic<uint32_t> next_type_id{static_cast<uint32_t>(ObjectTypeId::USER_CLASS_BASE)};
    std::mutex registry_mutex;
    
public:
    static ClassRegistry& instance() {
        static ClassRegistry registry;
        return registry;
    }
    
    ObjectTypeId register_class(const std::string& class_name) {
        std::lock_guard<std::mutex> lock(registry_mutex);
        
        DEBUG_CLASS_META("Registering class '" << class_name << "'");
        
        auto it = class_name_to_metadata.find(class_name);
        if (it != class_name_to_metadata.end()) {
            DEBUG_CLASS_META("Class '" << class_name << "' already registered with type ID " << static_cast<uint32_t>(it->second->type_id));
            return it->second->type_id;
        }
        
        ObjectTypeId new_id = static_cast<ObjectTypeId>(next_type_id.fetch_add(1));
        auto metadata = std::make_unique<ClassMetadata>(class_name, new_id);
        
        class_name_to_metadata[class_name] = std::move(metadata);
        
        DEBUG_CLASS_META("Class '" << class_name << "' registered with type ID " << static_cast<uint32_t>(new_id));
        
        return new_id;
    }
    
    ClassMetadata* get_class_metadata(const std::string& class_name) {
        auto it = class_name_to_metadata.find(class_name);
        return (it != class_name_to_metadata.end()) ? it->second.get() : nullptr;
    }
    
    void finalize_all_classes() {
        DEBUG_CLASS_META("Finalizing all registered classes");
        
        for (auto& [name, metadata] : class_name_to_metadata) {
            metadata->finalize_layout();
        }
        
        DEBUG_CLASS_META("All classes finalized");
    }
};

} // namespace ultraScript

using namespace ultraScript;

void test_class_system_debug() {
    std::cout << "\n=== UltraScript Class System Debug Test ===" << std::endl;
    std::cout << "This test shows which optimization paths are taken for different property access patterns.\n" << std::endl;
    
    // Register Person class
    ClassRegistry& registry = ClassRegistry::instance();
    ObjectTypeId person_type = registry.register_class("Person");
    
    // Get Person metadata and add properties
    ClassMetadata* person_meta = registry.get_class_metadata("Person");
    if (person_meta) {
        person_meta->add_property("name", PropertyType::STRING);
        person_meta->add_property("age", PropertyType::INT64);
        person_meta->add_property("salary", PropertyType::FLOAT64);
    }
    
    // Finalize all classes
    registry.finalize_all_classes();
    
    std::cout << "\n=== Simulating Different Property Access Patterns ===" << std::endl;
    
    // Simulate: bob.name (compile-time known property)
    std::cout << "\n--- 1. Static Property Access: bob.name ---" << std::endl;
    std::cout << "AST Analysis: Looking up 'name' property at compile time..." << std::endl;
    const PropertyDescriptor* name_prop = person_meta->find_property("name");
    if (name_prop) {
        std::cout << "✓ AST Result: Property found at compile time!" << std::endl;
        std::cout << "  → Will emit: mov rax, [rbx + " << name_prop->offset << "]  ; Direct offset access" << std::endl;
        std::cout << "  → Performance: ULTRA-FAST (zero runtime cost)" << std::endl;
    }
    
    // Simulate: bob[\"age\"] (runtime string lookup)
    std::cout << "\n--- 2. Dynamic Property Access: bob[\"age\"] ---" << std::endl;
    std::cout << "Runtime Analysis: Looking up 'age' property by hash..." << std::endl;
    uint32_t age_hash = hash_property_name("age");
    std::cout << "  Hash calculated: 0x" << std::hex << age_hash << std::dec << std::endl;
    const PropertyDescriptor* age_prop = person_meta->find_property_by_hash(age_hash);
    if (age_prop) {
        std::cout << "✓ Runtime Result: Property found in class!" << std::endl;
        std::cout << "  → Will use: Hash table lookup + offset access" << std::endl;
        std::cout << "  → Performance: FAST (hash lookup + direct access)" << std::endl;
    }
    
    // Simulate: bob.xyz = 123 (property not in class)
    std::cout << "\n--- 3. Dynamic Property Creation: bob.xyz = 123 ---" << std::endl;
    std::cout << "Runtime Analysis: Looking up 'xyz' property..." << std::endl;
    uint32_t xyz_hash = hash_property_name("xyz");
    std::cout << "  Hash calculated: 0x" << std::hex << xyz_hash << std::dec << std::endl;
    const PropertyDescriptor* xyz_prop = person_meta->find_property_by_hash(xyz_hash);
    if (!xyz_prop) {
        std::cout << "✗ Runtime Result: Property not found in class!" << std::endl;
        std::cout << "  → Will use: Dynamic properties hash table (per-object)" << std::endl;
        std::cout << "  → Performance: SLOWER (hash table lookup + storage)" << std::endl;
    }
    
    std::cout << "\n=== Code Generation Paths Summary ===" << std::endl;
    std::cout << "1. ULTRA-FAST (bob.name):" << std::endl;
    std::cout << "   - AST knows property offset at compile time" << std::endl;
    std::cout << "   - Emits direct memory access: [object + offset]" << std::endl;
    std::cout << "   - Zero runtime lookup cost" << std::endl;
    
    std::cout << "\n2. DYNAMIC (bob[propName]):" << std::endl;
    std::cout << "   - Runtime hash-based lookup in class properties" << std::endl;
    std::cout << "   - Falls back to per-object dynamic properties" << std::endl;
    std::cout << "   - Moderate runtime cost" << std::endl;
    
    std::cout << "\n3. DYNAMIC_DICT (bob.xyz = new_prop):" << std::endl;
    std::cout << "   - Property not defined in class" << std::endl;
    std::cout << "   - Stored in per-object hash table" << std::endl;
    std::cout << "   - Highest runtime cost but maximum flexibility" << std::endl;
    
    std::cout << "\n=== Debug Test Complete ===" << std::endl;
}

int main() {
    test_class_system_debug();
    return 0;
}
