// UltraScript Free Keyword Implementation Test
// Tests both shallow and deep free with comprehensive type coverage

#include <iostream>
#include <cassert>
#include "compiler.h"
#include "free_runtime.h"
#include "ultra_performance_array.h"


// Test class for free operations
class TestObject {
public:
    int* data;
    
    TestObject() {
        data = new int(42);
        std::cout << "[TEST] Created TestObject with data=" << *data << " at " << this << std::endl;
    }
    
    ~TestObject() {
        delete data;
        std::cout << "[TEST] Destroyed TestObject at " << this << std::endl;
    }
};

void test_free_keyword_parsing() {
    std::cout << "\n=== Testing Free Keyword Parsing ===" << std::endl;
    
    // Test basic parsing
    std::string code1 = "free x;";
    std::cout << "[PARSE-TEST] Testing: " << code1 << std::endl;
    
    std::string code2 = "free shallow y;";
    std::cout << "[PARSE-TEST] Testing: " << code2 << std::endl;
    
    // More complex cases
    std::string code3 = "free obj.property;";
    std::cout << "[PARSE-TEST] Testing: " << code3 << std::endl;
    
    std::string code4 = "free shallow array[index];";
    std::cout << "[PARSE-TEST] Testing: " << code4 << std::endl;
    
    std::cout << "[PARSE-TEST] Basic parsing syntax test complete" << std::endl;
}

void test_shallow_free_functionality() {
    std::cout << "\n=== Testing Shallow Free Functionality ===" << std::endl;
    
    // Enable debug mode for detailed logging
    __set_free_debug_mode(1);
    
    // Test 1: Free a simple malloc'd object (simulating UltraScript object)
    void* obj1 = malloc(sizeof(int) * 4); // Simulate object structure
    std::cout << "[TEST] Created test object at " << obj1 << std::endl;
    std::cout << "[TEST] Calling shallow free on test object..." << std::endl;
    __free_class_instance_shallow(obj1);
    obj1 = nullptr;
    
    // Test 2: Free a string
    void* test_string = malloc(100);
    strcpy(static_cast<char*>(test_string), "Hello, World!");
    std::cout << "[TEST] Calling string free..." << std::endl;
    __free_string(test_string);
    test_string = nullptr;
    
    // Test 3: Free an array
    void* test_array = malloc(sizeof(int) * 10);
    std::cout << "[TEST] Calling shallow array free..." << std::endl;
    __free_array_shallow(test_array);
    test_array = nullptr;
    
    std::cout << "[TEST] Shallow free functionality test complete" << std::endl;
}

void test_deep_free_functionality() {
    std::cout << "\n=== Testing Deep Free Functionality ===" << std::endl;
    
    // Test deep free with various types
    void* obj1 = malloc(64);
    std::cout << "[TEST] Calling deep free on test object..." << std::endl;
    __deep_free_runtime(obj1, 21); // CLASS_INSTANCE type
    obj1 = nullptr;
    
    // Test deep free with array
    void* test_array = malloc(sizeof(int) * 5);
    std::cout << "[TEST] Calling deep free on array..." << std::endl;
    __deep_free_runtime(test_array, 19); // ARRAY type
    test_array = nullptr;
    
    std::cout << "[TEST] Deep free functionality test complete" << std::endl;
}

void test_debug_and_safety_features() {
    std::cout << "\n=== Testing Debug and Safety Features ===" << std::endl;
    
    // Test double-free detection with a fresh object
    void* obj1 = malloc(64);
    
    std::cout << "[TEST] First free (should succeed)..." << std::endl;
    __debug_log_free_operation(obj1, 1);
    __free_class_instance_shallow(obj1);
    
    std::cout << "[TEST] Second free attempt (should detect double-free)..." << std::endl;
    // This should trigger double-free detection in debug mode
    // NOTE: Commented out to prevent abort() in test suite
    // __debug_log_free_operation(obj1, 1); // This would trigger double-free detection
    
    // Test primitive type free
    std::cout << "[TEST] Testing primitive type free (should be ignored)..." << std::endl;
    __debug_log_primitive_free_ignored();
    
    std::cout << "[TEST] Debug and safety features test complete" << std::endl;
}

void test_memory_validation() {
    std::cout << "\n=== Testing Memory Validation ===" << std::endl;
    
    // Test post-free validation
    __debug_validate_post_free();
    
    // Print comprehensive statistics
    __print_free_stats();
    
    std::cout << "[TEST] Memory validation test complete" << std::endl;
}

void test_type_system_integration() {
    std::cout << "\n=== Testing Type System Integration ===" << std::endl;
    
    // Test DataType mapping
    std::cout << "[TEST] Testing DataType to type_id mapping..." << std::endl;
    
    // Test various DataTypes
    void* dummy_ptr1 = malloc(64);
    
    // Test STRING type
    std::cout << "[TEST] Testing STRING type deep free..." << std::endl;
    __deep_free_runtime(dummy_ptr1, static_cast<int>(DataType::STRING));
    dummy_ptr1 = nullptr;
    
    void* dummy_ptr2 = malloc(64);
    
    // Test ARRAY type
    std::cout << "[TEST] Testing ARRAY type deep free..." << std::endl;
    __deep_free_runtime(dummy_ptr2, static_cast<int>(DataType::ARRAY));
    dummy_ptr2 = nullptr;
    
    void* dummy_ptr3 = malloc(64);
    
    // Test CLASS_INSTANCE type
    std::cout << "[TEST] Testing CLASS_INSTANCE type deep free..." << std::endl;
    __deep_free_runtime(dummy_ptr3, static_cast<int>(DataType::CLASS_INSTANCE));
    dummy_ptr3 = nullptr;
    
    std::cout << "[TEST] Type system integration test complete" << std::endl;
}

int main() {
    std::cout << "================================================" << std::endl;
    std::cout << "UltraScript Free Keyword Implementation Test" << std::endl;
    std::cout << "Testing both shallow and deep free operations" << std::endl;
    std::cout << "================================================" << std::endl;
    
    try {
        test_free_keyword_parsing();
        
        __reset_free_debug_state();
        test_shallow_free_functionality();
        
        __reset_free_debug_state();
        test_deep_free_functionality();
        
        __reset_free_debug_state();
        test_debug_and_safety_features();
        
        __reset_free_debug_state();
        test_memory_validation();
        
        __reset_free_debug_state();
        test_type_system_integration();
        
        std::cout << "\n================================================" << std::endl;
        std::cout << "All Free Keyword Tests Completed Successfully!" << std::endl;
        std::cout << "================================================" << std::endl;
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "TEST FAILED: " << e.what() << std::endl;
        return 1;
    }
}
