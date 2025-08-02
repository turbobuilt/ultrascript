#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include <cstdint>

namespace ultraScript {

// Compile-time class layout information for JIT optimization
struct JITPropertyInfo {
    std::string name;
    uint32_t offset;        // Byte offset from object start
    uint32_t size;          // Size in bytes
    uint8_t type_id;        // DataType enum value
    
    JITPropertyInfo(const std::string& n, uint32_t off, uint32_t sz, uint8_t tid)
        : name(n), offset(off), size(sz), type_id(tid) {}
};

struct JITClassInfo {
    std::string class_name;
    uint32_t instance_size;                                    // Total object size in bytes
    std::vector<JITPropertyInfo> properties;                  // All properties in order
    std::unordered_map<std::string, uint32_t> property_offsets; // Fast name -> offset lookup
    
    JITClassInfo(const std::string& name) : class_name(name), instance_size(8) {} // Start with 8-byte header
    
    // Add a property to this class (called during compilation)
    void add_property(const std::string& prop_name, uint8_t type_id, uint32_t prop_size) {
        // Align to 8-byte boundaries for performance
        uint32_t aligned_offset = (instance_size + 7) & ~7;
        
        properties.emplace_back(prop_name, aligned_offset, prop_size, type_id);
        property_offsets[prop_name] = aligned_offset;
        
        instance_size = aligned_offset + prop_size;
    }
    
    // Get property offset by name (for JIT code generation)
    uint32_t get_property_offset(const std::string& prop_name) const {
        auto it = property_offsets.find(prop_name);
        return (it != property_offsets.end()) ? it->second : 0;
    }
};

// Global JIT class registry - populated during compilation
class JITClassRegistry {
private:
    std::unordered_map<std::string, JITClassInfo> classes;
    
public:
    static JITClassRegistry& instance() {
        static JITClassRegistry registry;
        return registry;
    }
    
    // Register a class during compilation
    void register_class(const std::string& class_name) {
        if (classes.find(class_name) == classes.end()) {
            classes.emplace(class_name, JITClassInfo(class_name));
        }
    }
    
    // Add a property to a class during compilation
    void add_property(const std::string& class_name, const std::string& prop_name, 
                     uint8_t type_id, uint32_t prop_size) {
        auto it = classes.find(class_name);
        if (it != classes.end()) {
            it->second.add_property(prop_name, type_id, prop_size);
        }
    }
    
    // Get class info for JIT code generation
    const JITClassInfo* get_class_info(const std::string& class_name) const {
        auto it = classes.find(class_name);
        return (it != classes.end()) ? &it->second : nullptr;
    }
    
    // Get property offset for JIT code generation
    uint32_t get_property_offset(const std::string& class_name, const std::string& prop_name) const {
        auto it = classes.find(class_name);
        if (it != classes.end()) {
            return it->second.get_property_offset(prop_name);
        }
        return 0;
    }
    
    // Get instance size for object allocation
    uint32_t get_instance_size(const std::string& class_name) const {
        auto it = classes.find(class_name);
        return (it != classes.end()) ? it->second.instance_size : 8; // Default to header size
    }
};

} // namespace ultraScript
