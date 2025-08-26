#include "static_scope_analyzer.h"
#include "compiler.h"
#include <iostream>

int main() {
    std::cout << "Testing Static Scope Analyzer..." << std::endl;
    
    // Create a simple AST for testing
    auto assignment = std::make_unique<Assignment>("x", nullptr);
    assignment->declared_type = DataType::INT32;
    
    // Create the analyzer
    StaticScopeAnalyzer analyzer;
    
    // Test the analyzer
    analyzer.analyze_function("test_function", assignment.get());
    
    std::cout << "Static Scope Analyzer test completed." << std::endl;
    return 0;
}
