#include "runtime.h"
#include <iostream>
#include <cstring>

using namespace ultraScript;

int main() {
    std::cout << "Testing UltraScript String System with Null Bytes..." << std::endl;
    
    // Test 1: Create a string with embedded null bytes using the new constructor
    const char test_data[] = {'H', 'e', 'l', 'l', 'o', '\0', 'W', 'o', 'r', 'l', 'd', '\0'};
    size_t test_length = 11; // 11 bytes including the embedded null
    
    GoTSString test_str(test_data, test_length);
    
    std::cout << "Original data length: " << test_length << std::endl;
    std::cout << "GoTSString reported length: " << test_str.size() << std::endl;
    std::cout << "Length matches: " << (test_str.size() == test_length ? "YES" : "NO") << std::endl;
    
    // Test 2: Verify the data content byte by byte
    bool content_matches = true;
    for (size_t i = 0; i < test_length; i++) {
        if (test_str.data()[i] != test_data[i]) {
            content_matches = false;
            std::cout << "Mismatch at position " << i << ": expected " << (int)test_data[i] 
                      << ", got " << (int)test_str.data()[i] << std::endl;
        }
    }
    std::cout << "Content matches: " << (content_matches ? "YES" : "NO") << std::endl;
    
    // Test 3: Test string functions with null bytes
    void* str_ptr = __string_create_with_length(test_data, test_length);
    size_t runtime_length = __string_length(str_ptr);
    const char* runtime_data = __string_data(str_ptr);
    
    std::cout << "Runtime function length: " << runtime_length << std::endl;
    std::cout << "Runtime length matches: " << (runtime_length == test_length ? "YES" : "NO") << std::endl;
    
    // Test 4: Test concatenation with null bytes
    const char test_data2[] = {'!', '\0', 'E', 'n', 'd'};
    size_t test_length2 = 5;
    
    void* str_ptr2 = __string_create_with_length(test_data2, test_length2);
    void* concat_result = __string_concat(str_ptr, str_ptr2);
    
    size_t concat_length = __string_length(concat_result);
    std::cout << "Concatenated length: " << concat_length << std::endl;
    std::cout << "Expected concat length: " << (test_length + test_length2) << std::endl;
    std::cout << "Concat length matches: " << (concat_length == test_length + test_length2 ? "YES" : "NO") << std::endl;
    
    // Test 5: Console output test (should handle null bytes properly)
    std::cout << "Console output test (with null bytes):" << std::endl;
    __console_log_string(str_ptr);
    std::cout << " [END]" << std::endl;
    
    // Clean up
    delete static_cast<GoTSString*>(str_ptr);
    delete static_cast<GoTSString*>(str_ptr2);
    delete static_cast<GoTSString*>(concat_result);
    
    std::cout << "All tests completed!" << std::endl;
    
    return 0;
}
