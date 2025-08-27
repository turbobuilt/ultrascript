#include "compiler.h"
#include "static_scope_analyzer.h"
#include <iostream>
#include <cassert>
#include <vector>
#include <iomanip>

// Scope Offset Validation Test using REAL UltraScript compiler
class ScopeOffsetTest {
private:
    // Expected scope levels for different variable access patterns
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
        std::string test_js_code = R"(
var moduleVar = "module-level";

function testFunction() {
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
)";
        
        std::cout << "\nJavaScript Test Code:" << std::endl;
        std::cout << test_js_code << std::endl;
        
        try {
            // Parse with REAL UltraScript compiler
            std::cout << "\nParsing with UltraScript compiler..." << std::endl;
            GoTSCompiler compiler;
            auto tokens = compiler.parse_javascript(test_js_code);
            
            if (tokens.empty()) {
                throw std::runtime_error("No AST nodes parsed");
            }
            
            std::cout << "✓ JavaScript successfully parsed! AST nodes: " << tokens.size() << std::endl;
            
            // Find functions to analyze
            std::vector<std::string> function_names;
            for (const auto& node : tokens) {
                if (auto func_decl = dynamic_cast<FunctionDecl*>(node.get())) {
                    function_names.push_back(func_decl->name);
                    std::cout << "✓ Found function: " << func_decl->name << std::endl;
                }
            }
            
            if (function_names.empty()) {
                throw std::runtime_error("No functions found to analyze");
            }
            
            // Analyze scope offsets for each function
            StaticScopeAnalyzer analyzer;
            
            for (const auto& func_name : function_names) {
                std::cout << "\nAnalyzing function: " << func_name << std::endl;
                
                // Find the function AST node
                ASTNode* target_function = nullptr;
                for (const auto& node : tokens) {
                    if (auto func_decl = dynamic_cast<FunctionDecl*>(node.get())) {
                        if (func_decl->name == func_name) {
                            target_function = func_decl;
                            break;
                        }
                    }
                }
                
                if (!target_function) {
                    std::cout << "✗ Could not find AST node for function: " << func_name << std::endl;
                    continue;
                }
                
                // Run scope analysis
                analyzer.analyze_function(func_name, target_function);
                
                // Get function analysis results
                auto analysis = analyzer.get_function_analysis(func_name);
                
                std::cout << "Function '" << func_name << "' scope analysis:" << std::endl;
                std::cout << "  Variables found: " << analysis.variables.size() << std::endl;
                std::cout << "  Required parent scope levels: ";
                for (int level : analysis.required_parent_scopes) {
                    std::cout << level << " ";
                }
                std::cout << std::endl;
                
                // Validate variable scope levels
                validate_scope_levels(analyzer, func_name, analysis);
            }
            
            std::cout << "\n====================================================" << std::endl;
            std::cout << "✓ SCOPE OFFSET TEST COMPLETED SUCCESSFULLY" << std::endl;
            std::cout << "✓ Variable scope level tracking validated" << std::endl;
            std::cout << "====================================================" << std::endl;
            
        } catch (const std::exception& e) {
            std::cout << "✗ COMPILER ERROR: " << e.what() << std::endl;
            throw;
        }
    }
    
private:
    static void validate_scope_levels(const StaticScopeAnalyzer& analyzer, 
                                    const std::string& func_name,
                                    const FunctionScopeAnalysis& analysis) {
        std::cout << "\n  Variable scope level validation:" << std::endl;
        std::cout << "  " << std::setw(15) << "Variable" 
                  << std::setw(15) << "Scope Level"
                  << std::setw(20) << "Declaration Type" 
                  << "  Description" << std::endl;
        std::cout << "  " << std::string(65, '-') << std::endl;
        
        for (const auto& [var_name, scope_info] : analysis.variables) {
            std::string decl_type;
            switch (scope_info.declaration_kind) {
                case Assignment::DeclarationKind::VAR: decl_type = "var"; break;
                case Assignment::DeclarationKind::LET: decl_type = "let"; break;
                case Assignment::DeclarationKind::CONST: decl_type = "const"; break;
                default: decl_type = "unknown"; break;
            }
            
            std::string description;
            if (scope_info.scope_level == 0) {
                description = "current scope";
            } else if (scope_info.scope_level == 1) {
                description = "parent scope";
            } else if (scope_info.scope_level == 2) {
                description = "grandparent scope";
            } else {
                description = "ancestor scope (" + std::to_string(scope_info.scope_level) + " levels up)";
            }
            
            std::cout << "  " << std::setw(15) << var_name
                      << std::setw(15) << scope_info.scope_level
                      << std::setw(20) << decl_type
                      << "  " << description << std::endl;
        }
        
        // Validate expected results
        auto expected = get_expected_results();
        bool validation_passed = true;
        
        std::cout << "\n  Validation against expected results:" << std::endl;
        for (const auto& expected_info : expected) {
            if (expected_info.accessed_in_function == func_name) {
                auto it = analysis.variables.find(expected_info.var_name);
                if (it != analysis.variables.end()) {
                    int actual_level = it->second.scope_level;
                    bool matches = (actual_level == expected_info.expected_scope_level);
                    
                    std::cout << "  " << (matches ? "✓" : "✗") 
                              << " " << expected_info.var_name 
                              << " expected level " << expected_info.expected_scope_level
                              << ", got " << actual_level
                              << " (" << expected_info.description << ")" << std::endl;
                    
                    if (!matches) {
                        validation_passed = false;
                    }
                } else {
                    std::cout << "  ✗ " << expected_info.var_name << " not found in analysis" << std::endl;
                    validation_passed = false;
                }
            }
        }
        
        if (!validation_passed) {
            throw std::runtime_error("Scope level validation failed for function: " + func_name);
        }
    }
};

int main() {
    std::cout << "SCOPE OFFSET VALIDATION TEST" << std::endl;
    std::cout << "Testing variable scope level tracking in UltraScript" << std::endl;
    
    try {
        ScopeOffsetTest::run_scope_offset_test();
        
        std::cout << "\n====================================================" << std::endl;
        std::cout << "✓ ALL TESTS PASSED" << std::endl;
        std::cout << "✓ Scope offset tracking working correctly" << std::endl;
        std::cout << "====================================================" << std::endl;
        
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "\n✗ TEST FAILED: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "\n✗ TEST FAILED: Unknown exception" << std::endl;
        return 1;
    }
}
