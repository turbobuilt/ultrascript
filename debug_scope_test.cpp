#include <iostream>
#include <map>

int main() {
    std::map<int, bool> expected_scope_allocation = {{1, false}, {2, true}};
    
    std::cout << "Expected scopes in test case:" << std::endl;
    for (const auto& [scope_level, should_need] : expected_scope_allocation) {
        std::cout << "  Scope " << scope_level << ": " << should_need << std::endl;
    }
    
    // Simulate the validation loop
    for (const auto& [scope_level, should_need] : expected_scope_allocation) {
        std::cout << "Testing scope " << scope_level << std::endl;
        std::cout << "✓ Scope " << scope_level << ": " 
                  << (should_need ? "requires allocation" : "can be optimized") << std::endl;
    }
    
    return 0;
}
