#include <iostream>
#include <memory>
#include <string>

// Simple test to isolate the memory issue without the complex lexical scope system
int main() {
    std::cout << "Testing basic memory management..." << std::endl;
    
    // Test 1: Simple unique_ptr creation and destruction
    {
        auto ptr = std::make_unique<std::string>("test");
        std::cout << "Created string: " << *ptr << std::endl;
    }
    std::cout << "unique_ptr test completed" << std::endl;
    
    // Test 2: shared_ptr creation and destruction 
    {
        auto shared = std::make_shared<std::string>("shared_test");
        std::cout << "Created shared string: " << *shared << std::endl;
    }
    std::cout << "shared_ptr test completed" << std::endl;
    
    std::cout << "All tests completed successfully" << std::endl;
    return 0;
}
