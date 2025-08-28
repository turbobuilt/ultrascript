#include "simple_lexical_scope.h"
#include <iostream>

int main() {
    SimpleLexicalScopeAnalyzer analyzer;
    
    std::cout << "=== Testing Descendant Dependencies ===" << std::endl;
    
    // Global scope (depth 0)
    analyzer.declare_variable("globalVar", "let");
    
    // Function scope (depth 1)
    analyzer.enter_scope();
    analyzer.declare_variable("funcVar", "let");
    
        // Inner block (depth 2)
        analyzer.enter_scope();
        analyzer.declare_variable("blockVar", "let");
        analyzer.access_variable("globalVar");  // Access from depth 2 -> 0
        analyzer.access_variable("funcVar");    // Access from depth 2 -> 1
        analyzer.access_variable("globalVar");  // Access again for testing count
        
            // Nested block (depth 3)
            analyzer.enter_scope();
            analyzer.access_variable("globalVar");  // Access from depth 3 -> 0
            analyzer.access_variable("funcVar");    // Access from depth 3 -> 1
            analyzer.access_variable("blockVar");   // Access from depth 3 -> 2
            
            std::cout << "\n--- Exiting nested block (depth 3) ---" << std::endl;
            analyzer.exit_scope(); // Exit depth 3
        
        std::cout << "\n--- Exiting inner block (depth 2) ---" << std::endl;
        analyzer.exit_scope(); // Exit depth 2
    
    std::cout << "\n--- Exiting function scope (depth 1) ---" << std::endl;
    analyzer.exit_scope(); // Exit depth 1
    
    std::cout << "\n=== Final Debug Info ===" << std::endl;
    analyzer.print_debug_info();
    
    return 0;
}
