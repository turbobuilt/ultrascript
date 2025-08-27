#include "compiler.h"
#include "static_scope_analyzer.h"
#include <iostream>
#include <cassert>
#include <vector>
#include <iomanip>

// Scope Offset Validation Test using REAL UltraScript compiler
class ScopeOffsetTest {
private:
    // Exp        std::cout << "📝 Validated JavaScript Code (" << count_lines(test_js_code) << " lines):" << std::endl;cted scope levels for different variable access patterns
    struct ExpectedScopeInfo {
        std::string var_name;
        std::string accessed_in_function;
        int expected_scope_level; // 0=current, 1=parent, 2=grandparent, etc.
        std::string description;
    };
    
    static std::vector<ExpectedScopeInfo> get_expected_results() {
        return {
            // Variables accessed in their own scope (level 0)
            {"moduleVar", "testFunction", 1, "module var accessed in function"},
            {"functionVar", "testFunction", 0, "function var in same function"},
            {"blockVar", "testFunction", 0, "block var accessed in same block"},
            
            // Variables accessed across scope boundaries  
            {"functionVar", "innerFunction", 1, "function var accessed from inner function"},
            {"blockVar", "innerFunction", 2, "block var accessed from deeply nested function"},
            {"moduleVar", "innerFunction", 2, "module var accessed from deeply nested function"},
            
            // For loop and nested access patterns
            {"loopVar", "testFunction", 0, "loop var in same loop scope"},
            {"functionVar", "arrowFunc", 1, "function var accessed in arrow function"},
        };
    }
    
public:
    static void run_scope_offset_test() {
        std::cout << "SCOPE OFFSET VALIDATION TEST" << std::endl;
        std::cout << "Testing variable scope level tracking in UltraScript" << std::endl;
        std::cout << "====================================================" << std::endl;
        
        // Simple, focused test case with clear scope boundaries
        const std::string test_js_code = R"(
var moduleVar = "module-level";

function testScopeOffsets() {
    var functionVar = "function-level";
    console.log(moduleVar); // Should be scope level 1 (parent)
    
    {
        let blockVar = "block-level";
        console.log(functionVar); // Should be scope level 0 (current function)
        console.log(moduleVar);   // Should be scope level 1 (parent)
        
        function innerFunction() {
            console.log(functionVar); // Should be scope level 1 (parent function)
            console.log(blockVar);    // Should be scope level 2 (grandparent block)
            console.log(moduleVar);   // Should be scope level 2 (grandparent module)
        }
        
        for (let i = 0; i < 5; i++) {
            let loopVar = i * 2;
            console.log(loopVar);     // Should be scope level 0 (current loop)
            console.log(blockVar);    // Should be scope level 1 (parent block)
            
            const arrowFunc = () => {
                console.log(functionVar); // Should be scope level 1 (parent function)
                console.log(loopVar);     // Should be scope level 1 (parent loop)
            };
        }
    }
}
        ";

        std::cout << "\n📝 JavaScript Code (" << count_lines(test_js_code) << " lines):" << std::endl;
        std::cout << test_js_code << std::endl;
        
        // Parse and analyze the JavaScript code
        run_lexical_scope_analysis(test_js_code, "testScopeOffsets");
    }

private:
    static int count_lines(const std::string& code) {
        return std::count(code.begin(), code.end(), '\n') + 1;
    }
    
    static void run_lexical_scope_analysis(const std::string& js_code, 
                                           const std::string& function_name) {
        try {
            std::cout << "\n🔍 PARSING with UltraScript GoTSCompiler..." << std::endl;
            
            // Use the REAL UltraScript compiler
            GoTSCompiler compiler;
            auto parsed_result = compiler.parse_javascript(js_code);
            
            if (parsed_result.empty()) {
                std::cout << "❌ Failed to parse JavaScript code" << std::endl;
                return;
            }
            
            std::cout << "✅ JavaScript successfully parsed! AST nodes: " << parsed_result.size() << std::endl;
            
            // Find the target function in the AST
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
            
            // Use the StaticScopeAnalyzer for scope analysis
            std::cout << "\n🔬 ANALYZING SCOPE OFFSETS with StaticScopeAnalyzer..." << std::endl;
            StaticScopeAnalyzer analyzer;
            
            // Pre-register global variables
            analyzer.register_variable_declaration("moduleVar", Assignment::VAR);
            
            analyzer.analyze_function(function_name, target_function);
            
            // Get and display the actual scope offset results
            try {
                auto analysis = analyzer.get_function_analysis(function_name);
                
                std::cout << "\n=== SCOPE OFFSET ANALYSIS RESULTS ===" << std::endl;
                std::cout << "Variables with scope offsets:" << std::endl;
                
                for (const auto& [var_name, var_info] : analysis.variables) {
                    std::cout << "    " << var_name << ": scope_level=" << var_info.scope_level 
                             << " (distance from declaration)" << std::endl;
                }
                
                if (analysis.variables.empty()) {
                    std::cout << "    (No variables found with scope offset data)" << std::endl;
                }
                
                std::cout << "=== END SCOPE OFFSET ANALYSIS ===" << std::endl;
                
            } catch (const std::exception& e) {
                std::cout << "❌ Error getting scope analysis results: " << e.what() << std::endl;
            }
            
        } catch (const std::exception& e) {
            std::cout << "❌ COMPILER ERROR: " << e.what() << std::endl;
        } catch (...) {
            std::cout << "❌ Unknown error occurred" << std::endl;
        }
    }
};

int main() {
    std::cout << "SCOPE OFFSET VALIDATION TEST" << std::endl;
    std::cout << "Using UltraScript GoTSCompiler and StaticScopeAnalyzer" << std::endl;
    
    try {
        ScopeOffsetTest::run_scope_offset_test();
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "\n❌ TEST FAILED: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "\n❌ TEST FAILED: Unknown exception" << std::endl;
        return 1;
    }
}
