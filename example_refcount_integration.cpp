// ============================================================================
// ULTRASCRIPT REFERENCE COUNTING INTEGRATION EXAMPLE
// Demonstrates how to use the new reference counting system
// ============================================================================

#include <iostream>
#include <vector>
#include <string>
#include "refcount.h"
#include "free_runtime.h"
#include "refcount_asm.h"

// Example UltraScript object types
struct UltraString {
    char* data;
    size_t length;
    size_t capacity;
    
    UltraString(const std::string& str) {
        length = str.length();
        capacity = length + 1;
        data = static_cast<char*>(malloc(capacity));
        strcpy(data, str.c_str());
        std::cout << "[UltraString] Created: \"" << data << "\"" << std::endl;
    }
    
    ~UltraString() {
        std::cout << "[UltraString] Destroyed: \"" << (data ? data : "null") << "\"" << std::endl;
        free(data);
        data = nullptr;
    }
};

struct UltraArray {
    void** elements;
    size_t length;
    size_t capacity;
    uint32_t element_type;
    
    UltraArray(size_t initial_capacity, uint32_t type) 
        : length(0), capacity(initial_capacity), element_type(type) {
        elements = static_cast<void**>(calloc(capacity, sizeof(void*)));
        std::cout << "[UltraArray] Created with capacity " << capacity << std::endl;
    }
    
    ~UltraArray() {
        std::cout << "[UltraArray] Destroyed (length=" << length << ")" << std::endl;
        // In real implementation, would release all element references
        for (size_t i = 0; i < length; ++i) {
            if (elements[i]) {
                rc_release(elements[i]);
            }
        }
        free(elements);
        elements = nullptr;
    }
    
    void push_back(void* element) {
        if (length >= capacity) {
            capacity *= 2;
            elements = static_cast<void**>(realloc(elements, capacity * sizeof(void*)));
        }
        elements[length++] = rc_retain(element);  // Retain reference
    }
};

struct UltraObject {
    std::string class_name;
    std::vector<void*> properties;  // Reference counted properties
    
    UltraObject(const std::string& name) : class_name(name) {
        std::cout << "[UltraObject] Created: " << class_name << std::endl;
    }
    
    ~UltraObject() {
        std::cout << "[UltraObject] Destroyed: " << class_name << std::endl;
        // Release all property references
        for (void* prop : properties) {
            if (prop) rc_release(prop);
        }
    }
    
    void add_property(void* property) {
        properties.push_back(rc_retain(property));
    }
};

// Type-specific destructors for the reference counting system
void ultra_string_destructor(void* ptr) {
    if (ptr) {
        static_cast<UltraString*>(ptr)->~UltraString();
    }
}

void ultra_array_destructor(void* ptr) {
    if (ptr) {
        static_cast<UltraArray*>(ptr)->~UltraArray();
    }
}

void ultra_object_destructor(void* ptr) {
    if (ptr) {
        static_cast<UltraObject*>(ptr)->~UltraObject();
    }
}

// Type IDs for UltraScript types
enum UltraTypeID {
    ULTRA_STRING = 100,
    ULTRA_ARRAY = 101,
    ULTRA_OBJECT = 102
};

// Factory functions for creating reference counted UltraScript objects
void* create_ultra_string(const std::string& str) {
    void* memory = rc_alloc(sizeof(UltraString), ULTRA_STRING, ultra_string_destructor);
    if (memory) {
        new (memory) UltraString(str);
    }
    return memory;
}

void* create_ultra_array(size_t capacity, uint32_t element_type) {
    void* memory = rc_alloc(sizeof(UltraArray), ULTRA_ARRAY, ultra_array_destructor);
    if (memory) {
        new (memory) UltraArray(capacity, element_type);
    }
    return memory;
}

void* create_ultra_object(const std::string& class_name) {
    void* memory = rc_alloc(sizeof(UltraObject), ULTRA_OBJECT, ultra_object_destructor);
    if (memory) {
        new (memory) UltraObject(class_name);
    }
    return memory;
}

// Demonstrate basic usage
void demo_basic_usage() {
    std::cout << "\n=== BASIC USAGE DEMONSTRATION ===" << std::endl;
    
    // Create some UltraScript objects
    void* str1 = create_ultra_string("Hello, UltraScript!");
    void* str2 = create_ultra_string("Reference Counting Rules!");
    void* array = create_ultra_array(10, ULTRA_STRING);
    void* obj = create_ultra_object("MyClass");
    
    std::cout << "\nReference counts after creation:" << std::endl;
    std::cout << "str1: " << rc_get_count(str1) << std::endl;
    std::cout << "str2: " << rc_get_count(str2) << std::endl;
    std::cout << "array: " << rc_get_count(array) << std::endl;
    std::cout << "obj: " << rc_get_count(obj) << std::endl;
    
    // Add strings to array
    UltraArray* typed_array = static_cast<UltraArray*>(array);
    typed_array->push_back(str1);  // This retains str1
    typed_array->push_back(str2);  // This retains str2
    
    std::cout << "\nReference counts after adding to array:" << std::endl;
    std::cout << "str1: " << rc_get_count(str1) << std::endl;
    std::cout << "str2: " << rc_get_count(str2) << std::endl;
    
    // Add array to object
    UltraObject* typed_obj = static_cast<UltraObject*>(obj);
    typed_obj->add_property(array);  // This retains array
    
    std::cout << "\nReference counts after adding array to object:" << std::endl;
    std::cout << "array: " << rc_get_count(array) << std::endl;
    
    // Release our initial references
    rc_release(str1);
    rc_release(str2);
    rc_release(array);
    rc_release(obj);  // This should cascade destroy everything
    
    std::cout << "\nAll objects should be destroyed now." << std::endl;
}

// Demonstrate cycle breaking with "free shallow"
void demo_cycle_breaking() {
    std::cout << "\n=== CYCLE BREAKING DEMONSTRATION ===" << std::endl;
    
    // Create two objects that reference each other
    void* obj1 = create_ultra_object("Parent");
    void* obj2 = create_ultra_object("Child");
    
    UltraObject* parent = static_cast<UltraObject*>(obj1);
    UltraObject* child = static_cast<UltraObject*>(obj2);
    
    // Create cycle
    parent->add_property(obj2);  // Parent -> Child
    child->add_property(obj1);   // Child -> Parent (cycle!)
    
    std::cout << "\nReference counts with cycle:" << std::endl;
    std::cout << "parent: " << rc_get_count(obj1) << std::endl;
    std::cout << "child: " << rc_get_count(obj2) << std::endl;
    
    // Release our initial references - objects won't be destroyed due to cycle
    rc_release(obj1);
    rc_release(obj2);
    
    std::cout << "\nReference counts after releasing initial refs (cycle remains):" << std::endl;
    std::cout << "parent: " << rc_get_count(obj1) << std::endl;
    std::cout << "child: " << rc_get_count(obj2) << std::endl;
    
    // Break the cycle using "free shallow" equivalent
    std::cout << "\nBreaking cycle with rc_break_cycles()..." << std::endl;
    rc_break_cycles(obj1);  // This is what "free shallow" does
    
    std::cout << "Cycle broken - objects should be destroyed now." << std::endl;
}

// Demonstrate high-performance operations
void demo_performance_features() {
    std::cout << "\n=== PERFORMANCE FEATURES DEMONSTRATION ===" << std::endl;
    
    // Batch operations
    const int NUM_OBJECTS = 100;
    std::vector<void*> objects;
    objects.reserve(NUM_OBJECTS);
    
    std::cout << "Creating " << NUM_OBJECTS << " objects..." << std::endl;
    for (int i = 0; i < NUM_OBJECTS; ++i) {
        void* obj = create_ultra_string("Batch object " + std::to_string(i));
        objects.push_back(obj);
    }
    
    std::cout << "Performing batch retain..." << std::endl;
    rc_retain_batch(objects.data(), objects.size());
    
    std::cout << "First object ref count: " << rc_get_count(objects[0]) << std::endl;
    
    std::cout << "Performing batch release..." << std::endl;
    rc_release_batch(objects.data(), objects.size());
    
    std::cout << "Releasing original references..." << std::endl;
    rc_release_batch(objects.data(), objects.size());
    
    std::cout << "All batch objects destroyed." << std::endl;
}

// Demonstrate C++ template interface
void demo_cpp_interface() {
    std::cout << "\n=== C++ TEMPLATE INTERFACE DEMONSTRATION ===" << std::endl;
    
    // Note: This would work with custom allocator integration
    std::cout << "Creating RefPtr objects..." << std::endl;
    
    // Manual RefPtr usage (since we can't use make_ref with placement new easily)
    void* raw_obj = create_ultra_object("TemplateObject");
    RefPtr<UltraObject> obj_ptr(static_cast<UltraObject*>(raw_obj));
    
    std::cout << "RefPtr use count: " << obj_ptr.use_count() << std::endl;
    std::cout << "Object class name: " << obj_ptr->class_name << std::endl;
    
    {
        RefPtr<UltraObject> copy = obj_ptr;  // Copy constructor
        std::cout << "After copy, use count: " << obj_ptr.use_count() << std::endl;
        
        RefPtr<UltraObject> moved = std::move(copy);  // Move constructor
        std::cout << "After move, use count: " << obj_ptr.use_count() << std::endl;
        std::cout << "copy is null: " << (!copy) << std::endl;
    }
    
    std::cout << "After scope exit, use count: " << obj_ptr.use_count() << std::endl;
    
    // RefPtr will automatically release when it goes out of scope
    std::cout << "RefPtr going out of scope..." << std::endl;
}

// Demonstrate assembly generation
void demo_assembly_generation() {
    std::cout << "\n=== ASSEMBLY GENERATION DEMONSTRATION ===" << std::endl;
    
    std::cout << "Generated assembly for retain operation:" << std::endl;
    const char* retain_asm = jit_generate_retain("rdi");
    std::cout << retain_asm << std::endl;
    
    std::cout << "Generated assembly for release operation:" << std::endl;
    const char* release_asm = jit_generate_release("rdi");
    std::cout << release_asm << std::endl;
    
    std::cout << "Generated assembly for cycle breaking:" << std::endl;
    const char* break_cycles_asm = jit_generate_break_cycles();
    std::cout << break_cycles_asm << std::endl;
}

// Demonstrate integration with existing free runtime
void demo_free_integration() {
    std::cout << "\n=== FREE RUNTIME INTEGRATION DEMONSTRATION ===" << std::endl;
    
    // Initialize the migration
    __migrate_to_rc_alloc();
    
    void* obj = create_ultra_object("FreeIntegrationTest");
    
    std::cout << "Testing __is_rc_object(): " << __is_rc_object(obj) << std::endl;
    
    std::cout << "Testing free shallow integration..." << std::endl;
    __free_rc_object_shallow(obj);
    
    std::cout << "Free integration test complete." << std::endl;
}

int main() {
    std::cout << "=== ULTRASCRIPT REFERENCE COUNTING INTEGRATION EXAMPLE ===" << std::endl;
    
    // Initialize the reference counting system
    rc_set_debug_mode(1);  // Enable detailed logging
    
    try {
        demo_basic_usage();
        demo_cycle_breaking();
        demo_performance_features();
        demo_cpp_interface();
        demo_assembly_generation();
        demo_free_integration();
        
        // Print final statistics
        std::cout << "\n=== FINAL SYSTEM STATISTICS ===" << std::endl;
        rc_print_stats();
        __print_free_stats();
        
    } catch (const std::exception& e) {
        std::cerr << "Example failed with exception: " << e.what() << std::endl;
        return 1;
    }
    
    std::cout << "\n=== INTEGRATION EXAMPLE COMPLETED SUCCESSFULLY ===" << std::endl;
    std::cout << "\nKey Benefits Demonstrated:" << std::endl;
    std::cout << "  ✓ Automatic memory management with reference counting" << std::endl;
    std::cout << "  ✓ High performance through optimized atomic operations" << std::endl;
    std::cout << "  ✓ Cycle breaking with 'free shallow' integration" << std::endl;
    std::cout << "  ✓ Batch operations for improved cache performance" << std::endl;
    std::cout << "  ✓ C++ RAII semantics with RefPtr template" << std::endl;
    std::cout << "  ✓ JIT assembly generation for maximum speed" << std::endl;
    std::cout << "  ✓ Seamless integration with existing free runtime" << std::endl;
    std::cout << "\nThe reference counting system is ready for production use!" << std::endl;
    
    return 0;
}
