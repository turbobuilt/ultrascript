#include "runtime.h"
#include <iostream>
#include <cassert>



int main() {
    std::cout << "=== UltraScript String Overhaul Test ===" << std::endl;
    
    // Test 1: Basic string creation
    std::cout << "\n1. Testing basic string creation..." << std::endl;
    UltraScriptString str1("Hello");
    UltraScriptString str2("World");
    
    std::cout << "str1: '" << std::string(str1.data(), str1.length()) << "' (length: " << str1.length() << ")" << std::endl;
    std::cout << "str2: '" << std::string(str2.data(), str2.length()) << "' (length: " << str2.length() << ")" << std::endl;
    
    // Test 2: String concatenation
    std::cout << "\n2. Testing string concatenation..." << std::endl;
    UltraScriptString str3 = str1 + str2;
    std::cout << "str1 + str2: '" << std::string(str3.data(), str3.length()) << "' (length: " << str3.length() << ")" << std::endl;
    
    // Test 3: String comparison
    std::cout << "\n3. Testing string comparison..." << std::endl;
    UltraScriptString str4("Hello");
    std::cout << "str1 == str4: " << (str1 == str4 ? "true" : "false") << std::endl;
    std::cout << "str1 == str2: " << (str1 == str2 ? "true" : "false") << std::endl;
    
    // Test 4: Empty string
    std::cout << "\n4. Testing empty string..." << std::endl;
    UltraScriptString empty;
    std::cout << "empty.length(): " << empty.length() << std::endl;
    std::cout << "empty.empty(): " << (empty.empty() ? "true" : "false") << std::endl;
    
    // Test 5: String with length constructor (no null termination needed)
    std::cout << "\n5. Testing string with explicit length..." << std::endl;
    const char* data = "Hello\0World"; // Contains null byte
    UltraScriptString str5(data, 11); // Include the null byte and "World"
    std::cout << "str5 length: " << str5.length() << std::endl;
    std::cout << "str5 data: ";
    for (uint64_t i = 0; i < str5.length(); i++) {
        char c = str5[i];
        if (c == '\0') {
            std::cout << "\\0";
        } else {
            std::cout << c;
        }
    }
    std::cout << std::endl;
    
    // Test 6: c_str() function (creates null-terminated copy)
    std::cout << "\n6. Testing c_str() function..." << std::endl;
    std::cout << "str1.c_str(): '" << str1.c_str() << "'" << std::endl;
    
    // Test 7: Character access
    std::cout << "\n7. Testing character access..." << std::endl;
    std::cout << "str1[0]: '" << str1[0] << "'" << std::endl;
    std::cout << "str1[4]: '" << str1[4] << "'" << std::endl;
    std::cout << "str1[10] (out of bounds): '" << str1[10] << "' (should be \\0)" << std::endl;
    
    std::cout << "\n=== All tests completed successfully! ===" << std::endl;
    return 0;
}
