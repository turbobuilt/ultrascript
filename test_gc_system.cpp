#include "gc_system.h"
// #include "lexical_scope.h"  // Skip lexical scope for now to avoid compiler.h dependency
#include <iostream>
#include <string>



int main() {
    std::cout << "===============================================" << std::endl;
    std::cout << "UltraScript Garbage Collection System Test" << std::endl;
    std::cout << "===============================================" << std::endl;
    
    try {
        // Test 1: Basic variable tracking and escape analysis
        std::cout << "\n=== TEST 1: Variable Tracking and Escape Analysis ===" << std::endl;
        
        auto& tracker = VariableTracker::instance();
        
        // Simulate parsing a function with various variable patterns
        size_t global_scope = tracker.enter_scope("global");
        
        // Global variables
        tracker.register_variable("global_var", DataType::INT64);
        tracker.register_variable("global_obj", DataType::ANY);
        
        // Function scope
        size_t func_scope = tracker.enter_scope("test_function", true);
        
        // Function parameters (automatically escape)
        tracker.register_variable("param1", DataType::INT64);
        tracker.register_variable("param2", DataType::STRING);
        tracker.mark_variable_escape("param1", EscapeType::FUNCTION_ARG);
        tracker.mark_variable_escape("param2", EscapeType::FUNCTION_ARG);
        
        // Local variables
        tracker.register_variable("local_var", DataType::FLOAT64);
        tracker.register_variable("callback_var", DataType::ANY);
        
        // Simulate callback creation (variables escape)
        tracker.mark_variable_escape("callback_var", EscapeType::CALLBACK);
        tracker.mark_variable_escape("global_obj", EscapeType::CALLBACK);
        
        // Simulate return statement
        tracker.register_variable("return_value", DataType::STRING);
        tracker.mark_variable_escape("return_value", EscapeType::RETURN_VALUE);
        
        // Nested scope (loop)
        size_t loop_scope = tracker.enter_scope("for_loop", false, true);
        tracker.register_variable("loop_var", DataType::INT32);
        tracker.register_variable("temp_obj", DataType::ANY);
        tracker.mark_variable_escape("temp_obj", EscapeType::OBJECT_ASSIGN);
        tracker.exit_scope(); // exit loop
        
        tracker.exit_scope(); // exit function
        
        // Goroutine scope
        size_t goroutine_scope = tracker.enter_scope("goroutine", true);
        tracker.register_variable("goroutine_local", DataType::BOOLEAN);
        tracker.mark_variable_escape("global_var", EscapeType::GOROUTINE);
        tracker.exit_scope(); // exit goroutine
        
        tracker.exit_scope(); // exit global
        
        // Display analysis results
        tracker.dump_scope_tree();
        tracker.dump_variables();
        
        // Test 2: Garbage Collector memory management
        std::cout << "\n=== TEST 2: Garbage Collector Memory Management ===" << std::endl;
        
        auto& gc = GarbageCollector::instance();
        
        std::cout << "Initial heap state:" << std::endl;
        auto stats = gc.get_stats();
        std::cout << "  Live objects: " << stats.live_objects << std::endl;
        std::cout << "  Heap used: " << gc.get_heap_used() << " bytes" << std::endl;
        
        // Allocate some test objects
        std::cout << "\nAllocating test objects..." << std::endl;
        void* obj1 = gc.gc_alloc(64, 1);  // Small object
        void* obj2 = gc.gc_alloc(128, 2); // Medium object
        void* obj3 = gc.gc_alloc_array(sizeof(int), 100, 3); // Array object
        
        std::cout << "Objects allocated:" << std::endl;
        std::cout << "  obj1: " << obj1 << std::endl;
        std::cout << "  obj2: " << obj2 << std::endl;
        std::cout << "  obj3: " << obj3 << std::endl;
        
        stats = gc.get_stats();
        std::cout << "After allocation:" << std::endl;
        std::cout << "  Live objects: " << stats.live_objects << std::endl;
        std::cout << "  Heap used: " << gc.get_heap_used() << " bytes" << std::endl;
        std::cout << "  Total allocated: " << stats.total_allocated << " bytes" << std::endl;
        
        // Add objects as roots (simulate being referenced by variables)
        gc.add_root(reinterpret_cast<void**>(&obj1));
        gc.add_root(reinterpret_cast<void**>(&obj2));
        // obj3 is not added as root - should be collected
        
        // Force garbage collection
        std::cout << "\nForcing garbage collection..." << std::endl;
        gc.collect();
        
        stats = gc.get_stats();
        std::cout << "After collection:" << std::endl;
        std::cout << "  Live objects: " << stats.live_objects << std::endl;
        std::cout << "  Heap used: " << gc.get_heap_used() << " bytes" << std::endl;
        std::cout << "  Total freed: " << stats.total_freed << " bytes" << std::endl;
        std::cout << "  Collections: " << stats.collections << std::endl;
        
        // Test 3: Integration with lexical scopes
        std::cout << "\n=== TEST 3: Integration with Lexical Scopes (SKIPPED) ===" << std::endl;
        std::cout << "Skipping lexical scope test to avoid circular dependencies" << std::endl;
        
        /*
        // Create some lexical scopes
        auto global_lexical_scope = std::make_shared<LexicalScope>();
        global_lexical_scope->declare_variable("scope_var1", DataType::INT64, true);
        global_lexical_scope->set_variable("scope_var1", 42L);
        
        auto child_lexical_scope = global_lexical_scope->create_child_scope();
        child_lexical_scope->declare_variable("scope_var2", DataType::STRING, true);
        child_lexical_scope->set_variable("scope_var2", std::string("Hello GC"));
        
        // Add scopes to GC root set
        gc.add_scope_roots(global_lexical_scope);
        gc.add_scope_roots(child_lexical_scope);
        
        std::cout << "Added lexical scopes to GC root set" << std::endl;
        std::cout << "Scope var1: " << global_lexical_scope->get_variable<int64_t>("scope_var1") << std::endl;
        std::cout << "Scope var2: " << child_lexical_scope->get_variable<std::string>("scope_var2") << std::endl;
        */
        
        // Test 4: Parser integration simulation
        std::cout << "\n=== TEST 4: Parser Integration Simulation ===" << std::endl;
        
        // Clear previous analysis
        tracker.clear();
        
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
        
        // Callback creation (captures 'local')
        GCParserIntegration::on_callback_creation({"local"});
        
        // Goroutine creation (captures 'x')
        GCParserIntegration::on_goroutine_creation({"x"});
        
        // Return statement
        GCParserIntegration::on_return_statement("local");
        
        GCParserIntegration::on_exit_scope(); // exit function
        GCParserIntegration::on_exit_scope(); // exit global
        
        // Finalize and show results
        GCParserIntegration::finalize_escape_analysis();
        
        // Cleanup
        std::cout << "\n=== CLEANUP ===" << std::endl;
        gc.remove_root(reinterpret_cast<void**>(&obj1));
        gc.remove_root(reinterpret_cast<void**>(&obj2));
        // gc.remove_scope_roots(global_lexical_scope);  // Skipped lexical scope test
        // gc.remove_scope_roots(child_lexical_scope);   // Skipped lexical scope test
        
        // Final collection to clean everything up
        gc.collect();
        
        stats = gc.get_stats();
        std::cout << "Final state:" << std::endl;
        std::cout << "  Live objects: " << stats.live_objects << std::endl;
        std::cout << "  Total collections: " << stats.collections << std::endl;
        std::cout << "  Total allocated: " << stats.total_allocated << " bytes" << std::endl;
        std::cout << "  Total freed: " << stats.total_freed << " bytes" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    std::cout << "\n===============================================" << std::endl;
    std::cout << "Garbage Collection System Test Complete!" << std::endl;
    std::cout << "===============================================" << std::endl;
    
    return 0;
}
