#include "static_scope_analyzer.h"
#include "compiler.h"
#include <iostream>

int main() {
    std::cout << "=== TESTING STATIC SCOPE ANALYZER WITH test_scope.gts ===" << std::endl;
    
    // Create a static scope analyzer
    StaticScopeAnalyzer analyzer;
    
    // Test analyzing a simple function structure that mimics test_scope.gts:
    // var x = 5;  // Global scope 0
    // let result = go function() {  // Function scope 1
    //     var y = 0;  // Local variable in scope 1
    //     console.log(y);  // Uses local y
    //     console.log("X is", x);  // Uses parent x from scope 0
    // }
    
    std::cout << "\nSimulating analysis of test_scope.gts structure:" << std::endl;
    std::cout << "var x = 5;                    // Global scope level 0" << std::endl;
    std::cout << "let result = go function() {  // Goroutine function scope level 1" << std::endl;
    std::cout << "    var y = 0;               // Local to goroutine function" << std::endl;
    std::cout << "    console.log(y);          // Uses local variable y" << std::endl;
    std::cout << "    console.log('X is', x);  // Uses parent scope variable x" << std::endl;
    std::cout << "}" << std::endl;
    std::cout << std::endl;
    
    // Simulate the analysis manually since we can't parse the actual file yet
    // We'll directly test the analyzer logic
    
    // Create a simple Assignment node to represent "var x = 5" at global level
    auto global_assignment = std::make_unique<Assignment>("x", nullptr);
    global_assignment->declared_type = DataType::INT32;
    
    // Analyze the "global function" (main scope)
    analyzer.analyze_function("global", global_assignment.get());
    
    // Now create another Assignment to represent the goroutine function
    auto goroutine_assignment = std::make_unique<Assignment>("y", nullptr);
    goroutine_assignment->declared_type = DataType::INT32;
    
    // Create an Identifier node to represent accessing "x" from parent scope
    auto parent_access = std::make_unique<Identifier>("x");
    
    // Analyze the goroutine function
    analyzer.analyze_function("goroutine_function", goroutine_assignment.get());
    
    // Get and print the analysis results
    auto global_analysis = analyzer.get_function_analysis("global");
    auto goroutine_analysis = analyzer.get_function_analysis("goroutine_function");
    
    std::cout << "\n=== ANALYSIS RESULTS ===" << std::endl;
    
    std::cout << "\nGlobal scope analysis:" << std::endl;
    std::cout << "  Variables: " << global_analysis.variables.size() << std::endl;
    std::cout << "  Required parent scopes: " << global_analysis.required_parent_scopes.size() << std::endl;
    std::cout << "  Has escaping variables: " << (global_analysis.has_escaping_variables ? "yes" : "no") << std::endl;
    
    std::cout << "\nGoroutine function analysis:" << std::endl;
    std::cout << "  Variables: " << goroutine_analysis.variables.size() << std::endl;
    std::cout << "  Required parent scopes: " << goroutine_analysis.required_parent_scopes.size() << std::endl;
    std::cout << "  Has escaping variables: " << (goroutine_analysis.has_escaping_variables ? "yes" : "no") << std::endl;
    
    if (!goroutine_analysis.required_parent_scopes.empty()) {
        std::cout << "  Parent scope levels needed: ";
        for (int level : goroutine_analysis.required_parent_scopes) {
            std::cout << level << " ";
        }
        std::cout << std::endl;
        
        std::cout << "  Register allocation:" << std::endl;
        for (const auto& [scope_level, register_id] : goroutine_analysis.scope_level_to_register) {
            std::cout << "    Parent scope level " << scope_level << " -> r" << register_id << std::endl;
        }
    }
    
    std::cout << "\n=== EXPECTED BEHAVIOR ===" << std::endl;
    std::cout << "For the test_scope.gts structure, we expect:" << std::endl;
    std::cout << "1. Global scope has variables but needs no parent scopes" << std::endl;
    std::cout << "2. Goroutine function should need parent scope level 0 (for variable 'x')" << std::endl;
    std::cout << "3. Parent scope level 0 should be assigned to register r12" << std::endl;
    std::cout << "4. This enables fast access to parent scope variables via [r12+offset]" << std::endl;
    
    std::cout << "\nNext steps:" << std::endl;
    std::cout << "- Integrate with actual parser to analyze real test_scope.gts" << std::endl;
    std::cout << "- Add proper AST walking to detect parent scope variable access" << std::endl;
    std::cout << "- Test with more complex scope hierarchies" << std::endl;
    
    return 0;
}
