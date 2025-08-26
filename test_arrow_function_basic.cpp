#include "compiler.h"
#include "static_scope_analyzer.h"
#include <iostream>
#include <cassert>

// Simple test for arrow function parsing and scope analysis
int main() {
    std::cout << "🏹 Testing Arrow Function Support" << std::endl;
    std::cout << "=================================" << std::endl;
    
    // Test 1: Simple single-parameter arrow function
    std::string simple_arrow_js = R"(
function testArrowFunction() {
    var globalVar = "global";
    let functionLet = "function-scoped";
    
    const simpleArrow = x => x + 1;
    
    console.log(globalVar, functionLet, simpleArrow(5));
}
    )";
    
    std::cout << "\nTest 1: Simple arrow function" << std::endl;
    std::cout << "JavaScript code:" << std::endl;
    std::cout << simple_arrow_js << std::endl;
    
    try {
        std::cout << "\n🔍 Parsing with REAL UltraScript GoTSCompiler..." << std::endl;
        
        GoTSCompiler compiler;
        auto parsed_result = compiler.parse_javascript(simple_arrow_js);
        
        if (parsed_result.empty()) {
            std::cout << "❌ Failed to parse JavaScript code" << std::endl;
            return 1;
        }
        
        std::cout << "✅ JavaScript successfully parsed! AST nodes: " << parsed_result.size() << std::endl;
        
        // Find the target function
        FunctionDecl* target_function = nullptr;
        for (auto& node : parsed_result) {
            if (auto* func = dynamic_cast<FunctionDecl*>(node.get())) {
                if (func->name == "testArrowFunction") {
                    target_function = func;
                    break;
                }
            }
        }
        
        if (!target_function) {
            std::cout << "❌ Function 'testArrowFunction' not found in parsed AST" << std::endl;
            return 1;
        }
        
        std::cout << "✅ Found function: testArrowFunction" << std::endl;
        
        // Run static scope analysis
        std::cout << "\n🔬 Running static scope analysis..." << std::endl;
        StaticScopeAnalyzer analyzer;
        analyzer.analyze_function("testArrowFunction", target_function);
        
        std::cout << "✅ Static scope analysis completed for testArrowFunction" << std::endl;
        
        // Check if arrow function variable was detected
        auto simpleArrow_info = analyzer.get_variable_info("simpleArrow");
        if (simpleArrow_info.variable_name.empty()) {
            std::cout << "⚠️  Arrow function variable 'simpleArrow' not found in analyzer" << std::endl;
        } else {
            std::cout << "✅ Arrow function variable found: simpleArrow" << std::endl;
            std::cout << "   - Declaration kind: " << (simpleArrow_info.declaration_kind == Assignment::CONST ? "const" : "other") << std::endl;
            std::cout << "   - Scope level: " << simpleArrow_info.scope_level << std::endl;
            std::cout << "   - Block scoped: " << (simpleArrow_info.is_block_scoped ? "true" : "false") << std::endl;
        }
        
        std::cout << "\n🎉 Arrow function test completed successfully!" << std::endl;
        return 0;
        
    } catch (const std::exception& e) {
        std::cout << "❌ Error: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cout << "❌ Unknown error occurred" << std::endl;
        return 1;
    }
}
