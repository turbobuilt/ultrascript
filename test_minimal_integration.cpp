#include "minimal_parser_gc.h"
#include <iostream>
#include <string>
#include <vector>

using namespace ultraScript;

// Test the minimal parser integration without the full compiler system
int main() {
    std::cout << "===============================================" << std::endl;
    std::cout << "Minimal Parser GC Integration Test" << std::endl;
    std::cout << "===============================================" << std::endl;
    
    try {
        MinimalParserGCIntegration gc;
        
        // Test basic scope management
        std::cout << "\n=== Test 1: Basic Scope Management ===" << std::endl;
        
        gc.enter_scope("global", false);
        gc.declare_variable("global_var", DataType::INT32);
        
        gc.enter_scope("function_test", true);
        gc.declare_variable("param1", DataType::STRING);
        gc.declare_variable("param2", DataType::FLOAT64);
        gc.declare_variable("local_var", DataType::ANY);
        
        // Test variable usage and escapes
        std::cout << "\n=== Test 2: Variable Escapes ===" << std::endl;
        
        // Simulate function call with arguments
        std::vector<std::string> call_args = {"param1", "local_var"};
        gc.mark_function_call("some_function", call_args);
        
        // Simulate closure creation
        std::vector<std::string> captured = {"param2"};
        gc.mark_closure_capture(captured);
        
        // Simulate return value
        gc.mark_return_value("local_var");
        
        // Exit scopes
        gc.exit_scope(); // function
        gc.exit_scope(); // global
        
        // Finalize
        std::cout << "\n=== Test 3: Finalization ===" << std::endl;
        gc.finalize_analysis();
        
        std::cout << "\nTest completed successfully!" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    std::cout << "\n===============================================" << std::endl;
    std::cout << "Test Complete - No Segfaults!" << std::endl;
    std::cout << "===============================================" << std::endl;
    
    return 0;
}
