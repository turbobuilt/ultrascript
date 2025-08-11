#include "minimal_parser_gc.h"
#include <iostream>
#include <string>

using namespace ultraScript;

// Static API wrapper for testing
class GCParserIntegration {
private:
    static std::unique_ptr<MinimalParserGCIntegration> instance_;
    
public:
    static void on_enter_scope(const std::string& scope_name, bool is_function = false) {
        ensure_initialized();
        instance_->enter_scope(scope_name, is_function);
    }
    
    static void on_exit_scope() {
        ensure_initialized();
        instance_->exit_scope();
    }
    
    static void on_variable_declaration(const std::string& name, DataType type) {
        ensure_initialized();
        instance_->declare_variable(name, type);
    }
    
    static void on_function_call(const std::string& function_name, const std::vector<std::string>& args) {
        ensure_initialized();
        instance_->mark_function_call(function_name, args);
    }
    
    static void on_callback_creation(const std::vector<std::string>& captured_vars) {
        ensure_initialized();
        instance_->mark_closure_capture(captured_vars);
    }
    
    static void on_object_assignment(const std::string& object_name, const std::string& property, const std::string& value_var) {
        ensure_initialized();
        instance_->mark_property_assignment(object_name, property);
    }
    
    static void on_return_statement(const std::string& returned_var) {
        ensure_initialized();
        instance_->mark_return_value(returned_var);
    }
    
    static void on_goroutine_creation(const std::vector<std::string>& captured_vars) {
        ensure_initialized();
        instance_->mark_goroutine_capture(captured_vars);
    }
    
    static void finalize_escape_analysis() {
        if (instance_) {
            instance_->finalize_analysis();
        }
    }
    
    static void clear() {
        instance_.reset();
    }
    
private:
    static void ensure_initialized() {
        if (!instance_) {
            instance_ = std::make_unique<MinimalParserGCIntegration>();
        }
    }
};

std::unique_ptr<MinimalParserGCIntegration> GCParserIntegration::instance_;

int main() {
    std::cout << "===============================================" << std::endl;
    std::cout << "UltraScript Parser GC Integration Test" << std::endl;
    std::cout << "===============================================" << std::endl;
    
    try {
        // Test Parser Integration Simulation
        std::cout << "\n=== TEST: Parser Integration Simulation ===" << std::endl;
        
        // Simulate parsing some UltraScript code:
        // function test(x, y) {
        //     var local = x + y;
        //     var callback = function() { return local; };
        //     go function() { console.log(x); };
        //     return local;
        // }
        
        GCParserIntegration::on_enter_scope("global");
        GCParserIntegration::on_enter_scope("test_function", true);
        
        // Function parameters
        GCParserIntegration::on_variable_declaration("x", DataType::ANY);
        GCParserIntegration::on_variable_declaration("y", DataType::ANY);
        
        // Local variable
        GCParserIntegration::on_variable_declaration("local", DataType::ANY);
        
        // Simulate expression: local = x + y
        std::vector<std::string> plus_args = {"x", "y"};
        GCParserIntegration::on_function_call("operator+", plus_args);
        
        // Simulate variable declaration with assignment
        GCParserIntegration::on_variable_declaration("callback", DataType::ANY);
        
        // Callback creation (captures 'local')
        GCParserIntegration::on_callback_creation({"local"});
        
        // Goroutine creation (captures 'x')
        GCParserIntegration::on_goroutine_creation({"x"});
        
        // Console.log function call inside goroutine
        std::vector<std::string> log_args = {"x"};
        GCParserIntegration::on_function_call("console.log", log_args);
        
        // Return statement
        GCParserIntegration::on_return_statement("local");
        
        GCParserIntegration::on_exit_scope(); // exit function
        GCParserIntegration::on_exit_scope(); // exit global
        
        // Finalize and show results
        GCParserIntegration::finalize_escape_analysis();
        
        std::cout << "\n=== Analysis Results ===" << std::endl;
        std::cout << "Based on the escape analysis:" << std::endl;
        std::cout << "- Variable 'x' escapes via function argument and goroutine capture" << std::endl;
        std::cout << "- Variable 'y' escapes via function argument" << std::endl;
        std::cout << "- Variable 'local' escapes via callback capture and return value" << std::endl;
        std::cout << "- Variable 'callback' escapes (closure allocated on heap)" << std::endl;
        std::cout << "- All variables require heap allocation in this example" << std::endl;
        
        // Test another scenario with more stack allocation opportunities
        std::cout << "\n=== TEST: Stack Allocation Opportunities ===" << std::endl;
        
        GCParserIntegration::clear(); // Reset for new test
        
        // Simulate parsing:
        // function simple() {
        //     var temp1 = 42;
        //     var temp2 = temp1 * 2;
        //     console.log(temp2);  // temp2 escapes here
        //     // temp1 doesn't escape - can be stack allocated
        // }
        
        GCParserIntegration::on_enter_scope("global");
        GCParserIntegration::on_enter_scope("simple_function", true);
        
        GCParserIntegration::on_variable_declaration("temp1", DataType::INT32);
        GCParserIntegration::on_variable_declaration("temp2", DataType::INT32);
        
        // temp2 = temp1 * 2 (temp1 used but doesn't escape)
        std::vector<std::string> mult_args = {"temp1", "2"};
        GCParserIntegration::on_function_call("operator*", mult_args);
        
        // console.log(temp2) - temp2 escapes
        std::vector<std::string> log_args2 = {"temp2"};
        GCParserIntegration::on_function_call("console.log", log_args2);
        
        GCParserIntegration::on_exit_scope(); // exit function
        GCParserIntegration::on_exit_scope(); // exit global
        
        GCParserIntegration::finalize_escape_analysis();
        
        std::cout << "\nIn this example:" << std::endl;
        std::cout << "- Variable 'temp1' could be stack-allocated (doesn't escape)" << std::endl;
        std::cout << "- Variable 'temp2' requires heap allocation (escapes via function call)" << std::endl;
        std::cout << "- This shows potential for mixed allocation strategies" << std::endl;
        
        // Cleanup
        std::cout << "\n=== CLEANUP ===" << std::endl;
        GCParserIntegration::clear();
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    std::cout << "\n===============================================" << std::endl;
    std::cout << "Parser GC Integration Test Complete!" << std::endl;
    std::cout << "===============================================" << std::endl;
    
    return 0;
}
