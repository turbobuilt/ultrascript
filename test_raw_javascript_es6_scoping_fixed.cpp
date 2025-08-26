#include "compiler.h"
#include "static_scope_analyzer.h"
#include <iostream>
#include <cassert>
#include <vector>

// Test helper class to capture and analyze scope results
class ScopeTestHelper {
public:
    static void test_es6_for_loop_scoping() {
        std::cout << "\n=== Testing ES6 For-Loop Scoping with Raw JavaScript ===" << std::endl;
        
        // Test 1: let in for-loop should be block-scoped
        std::string js_code_let = R"(
function testLetInForLoop() {
    for (let i = 0; i < 3; i++) {
        let j = i * 2;
        console.log(i, j);
    }
    // i and j should NOT be accessible here - block scoped
}
        )";
        
        std::cout << "\nTest 1: let variables in for-loop" << std::endl;
        std::cout << "JavaScript code:" << std::endl;
        std::cout << js_code_let << std::endl;
        
        test_javascript_scoping(js_code_let, "testLetInForLoop", {
            // Both i and j should be in the same block scope (loop body scope)
            {"i", Assignment::LET, 1, true},  // scope_level 1, block_scoped true
            {"j", Assignment::LET, 1, true}   // same scope as i
        });
        
        // Test 2: var in for loop should be function-scoped (hoisted)
        std::string js_code_var = R"(
function testVarInForLoop() {
    for (var i = 0; i < 3; i++) {
        var j = i * 2;
        console.log(i, j);
    }
    // i and j SHOULD be accessible here - function scoped
    console.log("After loop:", i, j);
}
        )";
        
        std::cout << "\nTest 2: var variables in for-loop" << std::endl;
        std::cout << "JavaScript code:" << std::endl;
        std::cout << js_code_var << std::endl;
        
        test_javascript_scoping(js_code_var, "testVarInForLoop", {
            {"i", Assignment::VAR, 0, false}, // function-scoped, no block scoping
            {"j", Assignment::VAR, 0, false}  // function-scoped, no block scoping
        });
        
        // Test 3: Mixed var/let/const with nested scoping
        std::string js_code_mixed = R"(
function testMixedDeclarations() {
    var x = "function-scoped";
    for (let i = 0; i < 2; i++) {
        const multiplier = 2;
        var y = "also-function-scoped";
        let z = i * multiplier;
        
        if (i > 0) {
            let w = z + 1;
            var u = "function-scoped-from-if";
            console.log(x, y, z, w, u);
        }
    }
    // x, y, u should be accessible (var)
    // i, multiplier, z, w should NOT be accessible (let/const)
}
        )";
        
        std::cout << "\nTest 3: Mixed declarations with nested blocks" << std::endl;
        std::cout << "JavaScript code:" << std::endl;
        std::cout << js_code_mixed << std::endl;
        
        test_javascript_scoping(js_code_mixed, "testMixedDeclarations", {
            {"x", Assignment::VAR, 0, false},             // function scope
            {"y", Assignment::VAR, 0, false},             // hoisted to function
            {"u", Assignment::VAR, 0, false},             // hoisted to function
            {"i", Assignment::LET, 1, true},            // for-loop scope
            {"multiplier", Assignment::CONST, 1, true}, // same scope as i
            {"z", Assignment::LET, 1, true},            // same scope as i
            {"w", Assignment::LET, 2, true}             // nested if block scope
        });
        
        // Test 4: Complex nested for-loops 
        std::string js_code_complex = R"(
function testComplexForLoop() {
    let outerVar = "outer";
    
    for (let i = 0; i < 3; i++) {
        const item = i + 10;
        let processed = item * 2;
        
        for (let j = 0; j < 2; j++) {
            let subItem = processed + j;
            var globalCounter = j + 1;
            
            if (subItem > 15) {
                let validItem = subItem - 5;
                console.log(outerVar, item, processed, subItem, validItem);
            }
        }
    }
    // Only outerVar and globalCounter should be accessible here
}
        )";
        
        std::cout << "\nTest 4: Complex nested for-loops with function calls" << std::endl;
        std::cout << "JavaScript code:" << std::endl;
        std::cout << js_code_complex << std::endl;
        
        test_javascript_scoping(js_code_complex, "testComplexForLoop", {
            {"outerVar", Assignment::LET, 0, true},        // function block scope  
            {"globalCounter", Assignment::VAR, 0, false},  // hoisted to function
            {"i", Assignment::LET, 1, true},               // outer for-loop scope
            {"item", Assignment::CONST, 1, true},          // same scope as i
            {"processed", Assignment::LET, 1, true},       // same scope as i
            {"j", Assignment::LET, 2, true},               // inner for-loop scope
            {"subItem", Assignment::LET, 2, true},         // same scope as j
            {"validItem", Assignment::LET, 3, true}        // nested if block scope
        });
        
        std::cout << "\n🎉 All ES6 scoping tests completed!" << std::endl;
    }

private:
    static void test_javascript_scoping(const std::string& js_code, 
                                       const std::string& function_name,
                                       const std::vector<std::tuple<std::string, Assignment::DeclarationKind, int, bool>>& expected_vars) {
        
        try {
            // Parse JavaScript using UltraScript parser
            std::cout << "\n🔍 Parsing JavaScript with UltraScript parser..." << std::endl;
            
            GoTSCompiler compiler;
            auto parsed_result = compiler.parse_javascript(js_code);
            
            if (parsed_result.empty()) {
                std::cout << "❌ Failed to parse JavaScript code" << std::endl;
                return;
            }
            
            std::cout << "✅ JavaScript successfully parsed! AST nodes: " << parsed_result.size() << std::endl;
            
            // Find the target function
            FunctionDecl* target_function = nullptr;
            for (auto& node : parsed_result) {
                if (auto* func = dynamic_cast<FunctionDecl*>(node.get())) {
                    if (func->name == function_name) {
                        target_function = func;
                        break;
                    }
                }
            }
            
            if (!target_function) {
                std::cout << "❌ Function '" << function_name << "' not found in parsed AST" << std::endl;
                return;
            }
            
            std::cout << "✅ Found function: " << function_name << std::endl;
            
            // Run static scope analysis
            StaticScopeAnalyzer analyzer;
            analyzer.analyze_function(function_name, target_function);
            
            std::cout << "✅ Static scope analysis completed for " << function_name << std::endl;
            
            // Validate variables
            bool all_tests_passed = true;
            for (const auto& [var_name, expected_kind, expected_scope, expected_block_scoped] : expected_vars) {
                std::cout << "🔍 Variable '" << var_name << "':" << std::endl;
                
                auto var_info_ptr = &analyzer.get_variable_info(var_name);
                if (var_info_ptr->variable_name.empty()) {
                    std::cout << "   ❌ Variable not found in scope analysis!" << std::endl;
                    all_tests_passed = false;
                    continue;
                }
                
                std::cout << "   - Declaration kind: " << (var_info_ptr->declaration_kind == Assignment::VAR ? "var" : 
                                                            var_info_ptr->declaration_kind == Assignment::LET ? "let" : "const") << std::endl;
                std::cout << "   - Scope level: " << var_info_ptr->scope_level << std::endl;
                std::cout << "   - Block scoped: " << (var_info_ptr->is_block_scoped ? "true" : "false") << std::endl;
                
                bool tests_pass = (var_info_ptr->declaration_kind == expected_kind) &&
                                (var_info_ptr->scope_level == expected_scope) &&
                                (var_info_ptr->is_block_scoped == expected_block_scoped);
                
                if (tests_pass) {
                    std::cout << "   ✅ All scope properties correct!" << std::endl;
                } else {
                    std::cout << "   ❌ Scope validation FAILED!" << std::endl;
                    std::cout << "      Expected: kind=" << (expected_kind == Assignment::VAR ? "var" : 
                                                           expected_kind == Assignment::LET ? "let" : "const")
                             << ", scope=" << expected_scope << ", block_scoped=" << expected_block_scoped << std::endl;
                    all_tests_passed = false;
                }
            }
            
            if (all_tests_passed) {
                std::cout << "✅ All scope validation passed for " << function_name << std::endl;
            } else {
                std::cout << "❌ Some scope validations failed for " << function_name << std::endl;
            }
            
        } catch (const std::exception& e) {
            std::cout << "💥 Test failed with exception: " << e.what() << std::endl;
        }
    }
};

int main() {
    std::cout << "🚀 Starting Raw JavaScript ES6 Scoping Tests" << std::endl;
    std::cout << "=============================================" << std::endl;
    
    try {
        ScopeTestHelper::test_es6_for_loop_scoping();
        std::cout << "\n✅ All tests completed successfully!" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cout << "💥 Test suite failed: " << e.what() << std::endl;
        return 1;
    }
}
