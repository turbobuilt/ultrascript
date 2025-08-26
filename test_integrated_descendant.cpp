#include <iostream>
#include <memory>
#include <vector>
#include "static_scope_analyzer.h"
#include "compiler.h"

// Create mock AST nodes for testing descendant analysis
class MockASTBuilder {
public:
    // Create a complex nested function structure for testing
    static std::unique_ptr<ASTNode> create_complex_nested_scenario() {
        // This represents:
        // function outer() {
        //   var outer_var = 1;
        //   function middle() {
        //     var middle_var = 2;
        //     function inner() {
        //       var inner_var = 3;
        //       console.log(outer_var);  // Accesses level 0 (skips level 1!)
        //     }
        //   }
        // }
        
        auto outer_body = std::make_unique<std::vector<std::unique_ptr<ASTNode>>>();
        
        // outer_var assignment
        auto outer_assignment = std::make_unique<Assignment>();
        outer_assignment->variable_name = "outer_var";
        outer_assignment->declared_type = DataType::INTEGER;
        outer_assignment->value = nullptr; // Simplified
        outer_body->push_back(std::move(outer_assignment));
        
        // middle function
        auto middle_func = std::make_unique<FunctionExpression>();
        middle_func->parameters = {};
        
        auto middle_body = std::make_unique<std::vector<std::unique_ptr<ASTNode>>>();
        
        // middle_var assignment
        auto middle_assignment = std::make_unique<Assignment>();
        middle_assignment->variable_name = "middle_var";
        middle_assignment->declared_type = DataType::INTEGER;
        middle_body->push_back(std::move(middle_assignment));
        
        // inner function
        auto inner_func = std::make_unique<FunctionExpression>();
        inner_func->parameters = {};
        
        auto inner_body = std::make_unique<std::vector<std::unique_ptr<ASTNode>>>();
        
        // inner_var assignment
        auto inner_assignment = std::make_unique<Assignment>();
        inner_assignment->variable_name = "inner_var";
        inner_assignment->declared_type = DataType::INTEGER;
        inner_body->push_back(std::move(inner_assignment));
        
        // Access outer_var (skips middle level!)
        auto outer_var_access = std::make_unique<Identifier>();
        outer_var_access->name = "outer_var";
        inner_body->push_back(std::move(outer_var_access));
        
        inner_func->body = *inner_body;
        middle_body->push_back(std::move(inner_func));
        
        middle_func->body = *middle_body;
        outer_body->push_back(std::move(middle_func));
        
        auto outer_func = std::make_unique<FunctionExpression>();
        outer_func->parameters = {};
        outer_func->body = *outer_body;
        
        return std::move(outer_func);
    }
};

// Test the descendant analysis with mock AST
class IntegratedDescendantTest {
public:
    void run_test() {
        std::cout << "🔬 INTEGRATED DESCENDANT ANALYSIS TEST" << std::endl;
        std::cout << "Testing real StaticScopeAnalyzer with mock AST nodes" << std::endl;
        std::cout << std::string(60, '=') << std::endl;
        
        // Create the analyzer
        StaticScopeAnalyzer analyzer;
        
        // Create complex nested function AST
        auto ast = MockASTBuilder::create_complex_nested_scenario();
        
        std::cout << "\nTesting scenario:" << std::endl;
        std::cout << "- outer function (level 0): declares outer_var" << std::endl;
        std::cout << "- middle function (level 1): declares middle_var" << std::endl; 
        std::cout << "- inner function (level 2): declares inner_var, accesses outer_var" << std::endl;
        std::cout << "- Expected: middle function must provide outer_var access (level 0)" << std::endl;
        std::cout << "            even though middle doesn't use outer_var directly!" << std::endl;
        
        try {
            // Analyze the outer function
            std::cout << "\n🔍 Running analysis..." << std::endl;
            analyzer.analyze_function("outer_function", ast.get());
            
            std::cout << "\n✅ Analysis completed successfully!" << std::endl;
            std::cout << "Check the debug output above to see descendant propagation in action." << std::endl;
            
        } catch (const std::exception& e) {
            std::cout << "\n❌ Analysis failed with exception: " << e.what() << std::endl;
        } catch (...) {
            std::cout << "\n❌ Analysis failed with unknown exception" << std::endl;
        }
    }
};

int main() {
    IntegratedDescendantTest test;
    test.run_test();
    return 0;
}
