#include "compiler.h"
#include "static_scope_analyzer.h"
#include <iostream>
#include <cassert>
#include <vector>
#in        test_javascript_scoping(js_code_complex, "testComplexForLoop", {
            {"outerVar", Assignment::LET, 0, true},        // function block scope  
            {"globalCounter", Assignment::VAR, 0, false},  // hoisted to function
            {"i", Assignment::LET, 1, true},               // outer for-loop scope
            {"item", Assignment::CONST, 1, true},          // same scope as i
            {"processed", Assignment::LET, 1, true},       // same scope as i
            {"j", Assignment::LET, 2, true},               // inner for-loop scope
            {"subItem", Assignment::LET, 2, true},         // same scope as j
            {"validItem", Assignment::LET, 3, true}        // nested if block scopeing>

// Test helper class to capture and analyze scope results
class ScopeTestHelper {
public:
    static void test_es6_for_loop_scoping() {
        std::cout << "\n=== Testing ES6 For-Loop Scoping with Raw JavaScript ===" << std::endl;
        
        // Test 1: let in for loop should be block-scoped with loop body
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
            // Both i and j should be hoisted to function scope (level 0)
            {"i", Assignment::VAR, 0, false}, // scope_level 0, block_scoped false
            {"j", Assignment::VAR, 0, false}  // hoisted to function scope
        });
        
        // Test 3: Mixed let/const/var with nested blocks
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
            {"x", Assignment::VAR, 0, false},           // function scope
            {"y", Assignment::VAR, 0, false},           // hoisted to function
            {"u", Assignment::VAR, 0, false},           // hoisted from if block
            {"i", Assignment::LET, 1, true},            // for-loop scope
            {"multiplier", Assignment::CONST, 1, true}, // same scope as i
            {"z", Assignment::LET, 1, true},            // same scope as i
            {"w", Assignment::LET, 2, true}             // nested if block scope
        });
        
        // Test 4: For-loop with complex expressions and function calls
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
            {"len", Assignment::LET, 1, true},             // same scope as i
            {"item", Assignment::CONST, 1, true},          // same scope as i
            {"processed", Assignment::LET, 1, true},       // same scope as i
            {"j", Assignment::LET, 2, true},               // inner for-loop scope  
            {"subItem", Assignment::LET, 2, true},         // same scope as j
            {"validItem", Assignment::LET, 3, true}        // if block scope
        });
        
        std::cout << "\n✅ All ES6 scoping tests passed!" << std::endl;
    }
    
private:
    struct ExpectedVariable {
        std::string name;
        Assignment::DeclarationKind kind;
        int expected_scope_level;
        bool expected_block_scoped;
    };
    
    static void test_javascript_scoping(const std::string& js_code, 
                                      const std::string& function_name,
                                      const std::vector<ExpectedVariable>& expected_vars) {
        try {
            // Create UltraScript compiler
            GoTSCompiler compiler(Backend::X86_64);
            compiler.set_current_file("test_es6_scoping.js");
            
            // Parse the raw JavaScript code
            std::cout << "\n🔍 Parsing JavaScript with UltraScript parser..." << std::endl;
            auto ast = compiler.parse_javascript(js_code);
            std::cout << "✅ JavaScript successfully parsed! AST nodes: " << ast.size() << std::endl;
            
            // Find the function node in the AST
            FunctionDecl* function_node = nullptr;
            for (const auto& node : ast) {
                if (auto func = dynamic_cast<FunctionDecl*>(node.get())) {
                    if (func->name == function_name) {
                        function_node = func;
                        break;
                    }
                }
            }
            
            if (!function_node) {
                throw std::runtime_error("Function " + function_name + " not found in parsed AST");
            }
            
            std::cout << "✅ Found function: " << function_name << std::endl;
            
            // Create static scope analyzer and analyze the function
            StaticScopeAnalyzer scope_analyzer;
            scope_analyzer.analyze_function(function_name, function_node);
            
            std::cout << "✅ Static scope analysis completed for " << function_name << std::endl;
            
            // Verify expected variables are correctly scoped
            for (const auto& expected : expected_vars) {
                try {
                    auto var_info = scope_analyzer.get_variable_info(expected.name);
                    
                    std::cout << "🔍 Variable '" << expected.name << "':" << std::endl;
                    std::cout << "   - Declaration kind: " << (var_info.declaration_kind == Assignment::VAR ? "var" : 
                                                              var_info.declaration_kind == Assignment::LET ? "let" : "const") << std::endl;
                    std::cout << "   - Scope level: " << var_info.scope_level << std::endl;
                    std::cout << "   - Block scoped: " << (var_info.is_block_scoped ? "true" : "false") << std::endl;
                    
                    // Verify declaration kind
                    if (var_info.declaration_kind != expected.kind) {
                        throw std::runtime_error("Variable " + expected.name + " has wrong declaration kind. Expected: " +
                            std::to_string(expected.kind) + ", Got: " + std::to_string(var_info.declaration_kind));
                    }
                    
                    // Verify scope level
                    if (var_info.scope_level != expected.expected_scope_level) {
                        throw std::runtime_error("Variable " + expected.name + " has wrong scope level. Expected: " +
                            std::to_string(expected.expected_scope_level) + ", Got: " + std::to_string(var_info.scope_level));
                    }
                    
                    // Verify block scoping
                    if (var_info.is_block_scoped != expected.expected_block_scoped) {
                        throw std::runtime_error("Variable " + expected.name + " has wrong block scoping. Expected: " +
                            (expected.expected_block_scoped ? "true" : "false") + ", Got: " +
                            (var_info.is_block_scoped ? "true" : "false"));
                    }
                    
                    std::cout << "   ✅ All scope properties correct!" << std::endl;
                    
                } catch (const std::exception& var_error) {
                    std::cerr << "❌ Variable analysis failed for '" << expected.name << "': " << var_error.what() << std::endl;
                    throw;
                }
            }
            
            std::cout << "✅ All scope validation passed for " << function_name << std::endl;
            
        } catch (const std::exception& e) {
            std::cerr << "❌ Error in JavaScript scoping test: " << e.what() << std::endl;
            throw;
        }
    }
};

int main() {
    std::cout << "🚀 Starting Raw JavaScript ES6 Scoping Tests" << std::endl;
    std::cout << "=============================================" << std::endl;
    
    try {
        ScopeTestHelper::test_es6_for_loop_scoping();
        
        std::cout << "\n🎉 All tests completed successfully!" << std::endl;
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "\n💥 Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
}
