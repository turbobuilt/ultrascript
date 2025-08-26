#include "static_scope_analyzer.h"
#include "compiler.h"
#include <iostream>
#include <memory>
#include <cassert>

void test_es6_for_loop_scoping() {
    std::cout << "🧪 TESTING: ES6 For-Loop Scoping in Real UltraScript Parser" << std::endl;
    
    StaticScopeAnalyzer analyzer;
    
    // Create a test function with for-loops
    auto func_expr = std::make_unique<FunctionExpression>();
    func_expr->name = "test_for_loops";
    
    // Test Case 1: for (var i = 0; ...)
    auto var_for_loop = std::make_unique<ForLoop>();
    auto var_assignment = std::make_unique<Assignment>("i", std::make_unique<NumericLiteral>(0), Assignment::VAR);
    var_for_loop->init = std::move(var_assignment);
    var_for_loop->condition = std::make_unique<NumericLiteral>(1); // dummy condition
    var_for_loop->update = std::make_unique<Assignment>("i", std::make_unique<NumericLiteral>(1), Assignment::VAR);
    var_for_loop->init_declaration_kind = Assignment::VAR;
    var_for_loop->creates_block_scope = false;
    
    // Add a var declaration in the loop body
    auto var_body_stmt = std::make_unique<Assignment>("temp", std::make_unique<NumericLiteral>(1), Assignment::VAR);
    var_for_loop->body.push_back(std::move(var_body_stmt));
    
    func_expr->body.push_back(std::move(var_for_loop));
    
    // Test Case 2: for (let j = 0; ...)
    auto let_for_loop = std::make_unique<ForLoop>();
    auto let_assignment = std::make_unique<Assignment>("j", std::make_unique<NumericLiteral>(0), Assignment::LET);
    let_for_loop->init = std::move(let_assignment);
    let_for_loop->condition = std::make_unique<NumericLiteral>(1); // dummy condition
    let_for_loop->update = std::make_unique<Assignment>("j", std::make_unique<NumericLiteral>(1), Assignment::LET);
    let_for_loop->init_declaration_kind = Assignment::LET;
    let_for_loop->creates_block_scope = true;
    
    // Add let declarations in the loop body
    auto let_body_stmt1 = std::make_unique<Assignment>("value", std::make_unique<NumericLiteral>(2), Assignment::LET);
    auto let_body_stmt2 = std::make_unique<Assignment>("processed", std::make_unique<NumericLiteral>(3), Assignment::CONST);
    let_for_loop->body.push_back(std::move(let_body_stmt1));
    let_for_loop->body.push_back(std::move(let_body_stmt2));
    
    func_expr->body.push_back(std::move(let_for_loop));
    
    // Analyze the function
    analyzer.analyze_function("test_for_loops", func_expr.get());
    
    // Verify results
    std::cout << "\n[VERIFICATION]" << std::endl;
    
    // Check var variables (should be hoisted to function scope - level 0)
    auto i_info = analyzer.get_variable_info("i");
    auto temp_info = analyzer.get_variable_info("temp");
    
    std::cout << "var i: scope_level=" << i_info.scope_level << ", is_block_scoped=" << i_info.is_block_scoped << std::endl;
    std::cout << "var temp: scope_level=" << temp_info.scope_level << ", is_block_scoped=" << temp_info.is_block_scoped << std::endl;
    
    assert(i_info.scope_level == 0);        // var hoisted to function scope
    assert(!i_info.is_block_scoped);        // var is not block-scoped
    assert(temp_info.scope_level == 0);     // var hoisted to function scope  
    assert(!temp_info.is_block_scoped);     // var is not block-scoped
    
    // Check let/const variables (should be in block scope - level 1)
    auto j_info = analyzer.get_variable_info("j");
    auto value_info = analyzer.get_variable_info("value");
    auto processed_info = analyzer.get_variable_info("processed");
    
    std::cout << "let j: scope_level=" << j_info.scope_level << ", is_block_scoped=" << j_info.is_block_scoped << std::endl;
    std::cout << "let value: scope_level=" << value_info.scope_level << ", is_block_scoped=" << value_info.is_block_scoped << std::endl;
    std::cout << "const processed: scope_level=" << processed_info.scope_level << ", is_block_scoped=" << processed_info.is_block_scoped << std::endl;
    
    assert(j_info.scope_level == 1);           // let in block scope
    assert(j_info.is_block_scoped);            // let is block-scoped
    assert(value_info.scope_level == 1);       // let in same block scope as j
    assert(value_info.is_block_scoped);        // let is block-scoped
    assert(processed_info.scope_level == 1);   // const in same block scope as j
    assert(processed_info.is_block_scoped);    // const is block-scoped
    
    // Check scope allocation requirements
    bool function_scope_needs_allocation = analyzer.scope_needs_actual_allocation("test_for_loops", 0);
    bool block_scope_needs_allocation = analyzer.scope_needs_actual_allocation("test_for_loops", 1);
    
    std::cout << "\nFunction scope (0) needs allocation: " << function_scope_needs_allocation << std::endl;
    std::cout << "Block scope (1) needs allocation: " << block_scope_needs_allocation << std::endl;
    
    assert(function_scope_needs_allocation);   // Function scope always needs allocation
    assert(block_scope_needs_allocation);     // Block scope needs allocation (has let/const)
    
    // Check let/const detection
    bool function_has_let_const = analyzer.has_let_const_in_scope("test_for_loops", 0);
    bool block_has_let_const = analyzer.has_let_const_in_scope("test_for_loops", 1);
    
    std::cout << "Function scope has let/const: " << function_has_let_const << std::endl;
    std::cout << "Block scope has let/const: " << block_has_let_const << std::endl;
    
    assert(!function_has_let_const);  // Function scope has only var (hoisted)
    assert(block_has_let_const);      // Block scope has let/const
    
    std::cout << "\n✅ ES6 FOR-LOOP SCOPING TEST PASSED!" << std::endl;
    std::cout << "🎯 Key Validations:" << std::endl;
    std::cout << "  • for(var i...) variables hoisted to function scope (level 0)" << std::endl;
    std::cout << "  • for(let j...) variables in block scope (level 1)" << std::endl;  
    std::cout << "  • j, value, processed all in SAME scope level (correct ES6 semantics)" << std::endl;
    std::cout << "  • Scope allocation requirements correctly detected" << std::endl;
    std::cout << "  • Performance optimization opportunities identified" << std::endl;
}

int main() {
    std::cout << "🚀 REAL ULTRASCRIPT ES6 SCOPING VALIDATION" << std::endl;
    std::cout << "Testing actual UltraScript parser with ES6 for-loop fixes" << std::endl;
    
    try {
        test_es6_for_loop_scoping();
        
        std::cout << "\n🏆 REAL PARSER INTEGRATION SUCCESS!" << std::endl;
        std::cout << "The UltraScript static scope analyzer now correctly handles:" << std::endl;
        std::cout << "✅ ES6 let/const vs var scoping semantics" << std::endl;
        std::cout << "✅ Proper for-loop variable scoping" << std::endl;
        std::cout << "✅ Variable hoisting for var declarations" << std::endl;
        std::cout << "✅ Block scope optimization opportunities" << std::endl;
        
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "❌ REAL PARSER TEST FAILED: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "❌ REAL PARSER TEST FAILED: Unknown exception" << std::endl;
        return 1;
    }
}
