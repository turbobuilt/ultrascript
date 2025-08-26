#include <iostream>
#include <string>
#include <cassert>
#include "static_scope_analyzer.h"

/**
 * Minimal ES6 Block Scoping Test
 * 
 * This test focuses on the core ES6 block scoping functionality
 * without relying on complex methods that aren't properly declared.
 */

int main() {
    std::cout << "=== Minimal ES6 Block Scoping Test ===" << std::endl;
    
    try {
        StaticScopeAnalyzer analyzer;
        
        // Test basic functionality
        analyzer.begin_function_analysis("test_function");
        
        // Add variables with different declaration types
        analyzer.add_variable_with_declaration_kind("varExample", VAR, 0, 1);
        analyzer.add_variable_with_declaration_kind("letExample", LET, 1, 2);
        analyzer.add_variable_with_declaration_kind("constExample", CONST, 1, 3);
        
        analyzer.end_function_analysis();
        
        // Test variable retrieval
        auto varInfo = analyzer.get_variable_info("varExample");
        auto letInfo = analyzer.get_variable_info("letExample");
        auto constInfo = analyzer.get_variable_info("constExample");
        
        // Validate scoping rules
        assert(varInfo.declaration_kind == VAR);
        assert(!varInfo.is_block_scoped);
        
        assert(letInfo.declaration_kind == LET);
        assert(letInfo.is_block_scoped);
        
        assert(constInfo.declaration_kind == CONST);
        assert(constInfo.is_block_scoped);
        
        std::cout << "✅ Variable declaration kinds work correctly" << std::endl;
        std::cout << "✅ Block scoping flags work correctly" << std::endl;
        std::cout << "✅ Basic ES6 block scoping functionality verified!" << std::endl;
        
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "❌ Test failed with exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "❌ Test failed with unknown exception" << std::endl;
        return 1;
    }
}
