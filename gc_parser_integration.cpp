#include "gc_system.h"
#include <algorithm>
#include <stdexcept>
#include <iostream>

namespace ultraScript {

// Simple GC-integrated parser demonstration
// This would normally integrate with the actual Parser class
class GCDemoParser {
public:
    static void demo_parsing() {
        std::cout << "[GC-Parser] Demo parsing with GC integration..." << std::endl;
        
        // Simulate parsing UltraScript code with GC tracking
        
        // Enter global scope
        GCParserIntegration::on_enter_scope("global", false);
        
        // Global variable
        GCParserIntegration::on_variable_declaration("globalVar", DataType::INT64);
        
        // Function definition
        GCParserIntegration::on_enter_scope("myFunction", true);
        
        // Function parameters
        GCParserIntegration::on_variable_declaration("param1", DataType::STRING);
        GCParserIntegration::on_variable_declaration("param2", DataType::INT32);
        
        // Local variables
        GCParserIntegration::on_variable_declaration("localVar", DataType::FLOAT64);
        GCParserIntegration::on_variable_declaration("objVar", DataType::ANY);
        
        // Function call with arguments (causes escape)
        GCParserIntegration::on_function_call("someFunction", {"param1", "localVar"});
        
        // Object property assignment (causes escape)
        GCParserIntegration::on_object_assignment("objVar", "property", "param2");
        
        // Callback creation (captures variables)
        GCParserIntegration::on_callback_creation({"localVar", "globalVar"});
        
        // Return statement
        GCParserIntegration::on_return_statement("objVar");
        
        // Exit function scope
        GCParserIntegration::on_exit_scope();
        
        // Goroutine creation
        GCParserIntegration::on_enter_scope("goroutine", true);
        GCParserIntegration::on_variable_declaration("goroutineLocal", DataType::BOOLEAN);
        GCParserIntegration::on_goroutine_creation({"globalVar", "goroutineLocal"});
        GCParserIntegration::on_exit_scope();
        
        // Exit global scope
        GCParserIntegration::on_exit_scope();
        
        // Finalize analysis
        GCParserIntegration::finalize_escape_analysis();
    }
};

} // namespace ultraScript
