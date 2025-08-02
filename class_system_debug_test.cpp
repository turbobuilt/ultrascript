#include "class_system_performance.h"
#include <iostream>

using namespace ultraScript;

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

// Mock CodeGenerator for testing
class CodeGenerator {
public:
    void emit_mov_reg_imm(int reg, uint64_t value) {
        std::cout << "[ASM] mov r" << reg << ", " << value << std::endl;
    }
    
    void emit_call(const char* function) {
        std::cout << "[ASM] call " << function << std::endl;
    }
    
    void emit_debug_output(const std::string& message) {
        std::cout << "[ASM] ; DEBUG: " << message << std::endl;
        std::cout << "[ASM] push rdi" << std::endl;
        std::cout << "[ASM] lea rdi, [debug_msg_" << message.length() << "]" << std::endl;
        std::cout << "[ASM] call puts" << std::endl;
        std::cout << "[ASM] pop rdi" << std::endl;
    }
};

void test_class_system_debug() {
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
    
    // Register Employee class (inherits from Person)
    ObjectTypeId employee_type = registry.register_class("Employee");
    ClassMetadata* employee_meta = registry.get_class_metadata("Employee");
    if (employee_meta) {
        employee_meta->set_parent_class("Person");
        employee_meta->add_property("department", PropertyType::STRING);
        employee_meta->add_property("employee_id", PropertyType::INT64);
    }
    
    // Finalize all classes
    registry.finalize_all_classes();
    
    std::cout << "\n=== Creating Objects ===" << std::endl;
    
    // Create Person object
    ObjectInstance* bob = ObjectFactory::create_object("Person");
    if (bob) {
        std::cout << "\n=== Testing Property Access Paths ===" << std::endl;
        
        // Test 1: Ultra-fast property access by index (compile-time known)
        std::cout << "\n--- Test 1: ULTRA-FAST property access (bob.name) ---" << std::endl;
        const char* name_value = "Bob Smith";
        bob->set_property_by_index<const char*>(0, name_value);  // name property is index 0
        const char* retrieved_name = *bob->get_property_by_index<const char*>(0);
        std::cout << "Retrieved name: " << (retrieved_name ? retrieved_name : "null") << std::endl;
        
        // Test 2: Dynamic property access by name (runtime lookup)
        std::cout << "\n--- Test 2: DYNAMIC property access (bob[\"age\"]) ---" << std::endl;
        int64_t age_value = 30;
        bool success = bob->set_property_by_hash(hash_property_name("age"), &age_value, sizeof(age_value));
        void* age_ptr = bob->get_property_by_hash(hash_property_name("age"));
        if (age_ptr) {
            int64_t retrieved_age = *static_cast<int64_t*>(age_ptr);
            std::cout << "Retrieved age: " << retrieved_age << std::endl;
        }
        
        // Test 3: Dynamic property that doesn't exist in class (bob.xyz = 123)
        std::cout << "\n--- Test 3: DYNAMIC_DICT property access (bob.xyz = 123) ---" << std::endl;
        int64_t xyz_value = 123;
        bob->set_property_by_hash(hash_property_name("xyz"), &xyz_value, sizeof(xyz_value));
        void* xyz_ptr = bob->get_property_by_hash(hash_property_name("xyz"));
        if (xyz_ptr) {
            std::cout << "Retrieved xyz from dynamic properties" << std::endl;
        }
        
        // Test 4: Property access by name (slowest path)
        std::cout << "\n--- Test 4: NAME property access (bob.get_property_by_name(\"salary\")) ---" << std::endl;
        double salary_value = 75000.50;
        bob->set_property_by_hash(hash_property_name("salary"), &salary_value, sizeof(salary_value));
        void* salary_ptr = bob->get_property_by_name("salary");
        if (salary_ptr) {
            double retrieved_salary = *static_cast<double*>(salary_ptr);
            std::cout << "Retrieved salary: $" << retrieved_salary << std::endl;
        }
        
        std::cout << "\n=== Testing Code Generation ===" << std::endl;
        
        // Test code generation with debug output
        CodeGenerator gen;
        
        std::cout << "\n--- Generating ULTRA-FAST property access assembly ---" << std::endl;
        ClassCodeGenerator::generate_property_access_by_index(gen, 0, PropertyType::STRING);  // name property
        
        std::cout << "\n--- Generating DYNAMIC property access assembly ---" << std::endl;
        ClassCodeGenerator::generate_property_access_by_hash(gen, hash_property_name("age"));
        
        std::cout << "\n--- Generating ULTRA-FAST property assignment assembly ---" << std::endl;
        ClassCodeGenerator::generate_property_assignment_by_index(gen, 1, PropertyType::INT64);  // age property
        
        std::cout << "\n--- Generating object construction assembly ---" << std::endl;
        ClassCodeGenerator::generate_object_construction(gen, person_type);
        
        // Clean up
        ObjectFactory::destroy_object(bob);
    }
    
    std::cout << "\n=== Testing Inheritance ===" << std::endl;
    
    // Create Employee object (inherits from Person)
    ObjectInstance* employee = ObjectFactory::create_object("Employee");
    if (employee) {
        std::cout << "\n--- Testing inherited property access ---" << std::endl;
        
        // Access inherited property "name" from Person
        const char* emp_name = "Alice Johnson";
        employee->set_property_by_hash(hash_property_name("name"), &emp_name, sizeof(emp_name));
        void* name_ptr = employee->get_property_by_hash(hash_property_name("name"));
        if (name_ptr) {
            std::cout << "Employee inherited name access successful" << std::endl;
        }
        
        // Access Employee-specific property
        const char* dept = "Engineering";
        employee->set_property_by_hash(hash_property_name("department"), &dept, sizeof(dept));
        void* dept_ptr = employee->get_property_by_hash(hash_property_name("department"));
        if (dept_ptr) {
            std::cout << "Employee department access successful" << std::endl;
        }
        
        ObjectFactory::destroy_object(employee);
    }
    
    std::cout << "\n=== Debug Test Complete ===" << std::endl;
}

int main() {
    test_class_system_debug();
    return 0;
}
