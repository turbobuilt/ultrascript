#include "simple_lexical_scope.h"
#include <iostream>

int main() {
    SimpleLexicalScopeAnalyzer analyzer;
    
    std::cout << "=== Testing Access Count Accumulation ===" << std::endl;
    
    // Global scope (depth 0)
    analyzer.declare_variable("global", "let");
    
    // Function scope (depth 1) 
    analyzer.enter_scope();
    analyzer.declare_variable("func", "let");
    
        // Block scope (depth 2)
        analyzer.enter_scope();
        // Access global 3 times, func 2 times
        analyzer.access_variable("global");
        analyzer.access_variable("global"); 
        analyzer.access_variable("global");
        analyzer.access_variable("func");
        analyzer.access_variable("func");
        
            // Inner scope (depth 3)
            analyzer.enter_scope();
            // Access global 1 time, func 1 time
            analyzer.access_variable("global");
            analyzer.access_variable("func");
            
            std::cout << "\n=== Depth 3 scope closing ===" << std::endl;
            analyzer.exit_scope(); // Should propagate: global(1), func(1)
        
        std::cout << "\n=== Depth 2 scope closing ===" << std::endl;
        analyzer.exit_scope(); // Should have: self(global:3, func:2) + descendant(global:1, func:1) = global:4, func:3
    
    std::cout << "\n=== Depth 1 scope closing ===" << std::endl;
    analyzer.exit_scope(); // Should receive all accumulated dependencies
    
    return 0;
}
