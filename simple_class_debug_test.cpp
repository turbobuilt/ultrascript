// #include "class_system_performance.h" // Removed - property access system redesigned
#include <iostream>

using namespace ultraScript;

// Simple test that shows the different debug paths without CodeGenerator dependencies
void simple_class_debug_test() {
    std::cout << "\n=== UltraScript Class System Debug Test ===" << std::endl;
    
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
    
    std::cout << "\n=== Testing Property Lookups ===" << std::endl;
    
    // Test compile-time property finding
    const PropertyDescriptor* name_prop = person_meta->find_property("name");
    if (name_prop) {
        std::cout << "Found 'name' property at compile time: index=" << name_prop->index 
                  << ", offset=" << name_prop->offset << std::endl;
    }
    
    // Test runtime hash-based finding
    const PropertyDescriptor* age_prop = person_meta->find_property_by_hash(hash_property_name("age"));
    if (age_prop) {
        std::cout << "Found 'age' property by hash: index=" << age_prop->index 
                  << ", offset=" << age_prop->offset << std::endl;
    }
    
    // Test property that doesn't exist
    const PropertyDescriptor* missing_prop = person_meta->find_property("missing");
    if (!missing_prop) {
        std::cout << "Property 'missing' correctly not found in class" << std::endl;
    }
    
    std::cout << "\n=== Property Access Paths Explanation ===" << std::endl;
    std::cout << "1. ULTRA-FAST: bob.name - AST emits direct offset assembly" << std::endl;
    std::cout << "2. DYNAMIC: bob[propName] - Runtime hash lookup in class properties" << std::endl;
    std::cout << "3. DYNAMIC_DICT: bob.xyz = 123 - Falls back to per-object hash table" << std::endl;
    
    std::cout << "\n=== Debug Test Complete ===" << std::endl;
}

// Mock DynamicValue for testing
struct DynamicValue {
    enum Type { NONE, INT64, FLOAT64, STRING } type = NONE;
    union {
        int64_t int_val;
        double float_val;
        const char* string_val;
    };
    
    DynamicValue() : type(NONE) {}
    DynamicValue(int64_t val) : type(INT64), int_val(val) {}
    DynamicValue(double val) : type(FLOAT64), float_val(val) {}
    DynamicValue(const char* val) : type(STRING), string_val(val) {}
};

int main() {
    simple_class_debug_test();
    return 0;
}
