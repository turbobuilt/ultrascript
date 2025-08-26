#include "compiler.h"
#include "static_scope_analyzer.h"
#include <iostream>
#include <cassert>
#include <vector>

// Ultra-complex ES6 scoping test with maximum nesting
class UltraComplexScopeTest {
public:
    static void run_insane_nested_scope_test() {
        std::cout << "\n🔥 ULTRA COMPLEX ES6 SCOPING TEST 🔥" << std::endl;
        std::cout << "=======================================" << std::endl;
        std::cout << "Testing: 8+ nesting levels, mixed declarations, complex variable access patterns" << std::endl;
        
        // The most complex JavaScript scope test ever created
        std::string ultra_complex_js = R"(
function ultraComplexScopeTest() {
    var globalVar = "function-scoped";
    let functionLet = "function-block-scoped";
    const functionConst = 42;
    
    // Level 1: First for-loop
    for (let i = 0; i < 3; i++) {
        const loopConst1 = i * 10;
        var hoistedFromLoop1 = "hoisted1";
        let loopLet1 = loopConst1 + 1;
        
        // Level 2: Nested if in first loop
        if (i > 0) {
            let ifLet1 = loopLet1 + 5;
            const ifConst1 = ifLet1 * 2;
            var hoistedFromIf1 = "hoisted-if1";
            
            // Level 3: Second for-loop inside if
            for (let j = 0; j < 2; j++) {
                const loopConst2 = j + ifConst1;
                let loopLet2 = loopConst2 * 3;
                var hoistedFromLoop2 = "hoisted2";
                
                // Level 4: Nested if in second loop
                if (j === 1) {
                    let ifLet2 = loopLet2 + 100;
                    const ifConst2 = ifLet2 / 2;
                    var hoistedFromIf2 = "hoisted-if2";
                    
                    // Level 5: Third for-loop (deepest)
                    for (let k = 0; k < 1; k++) {
                        const loopConst3 = k + ifConst2;
                        let loopLet3 = loopConst3 + 999;
                        var hoistedFromLoop3 = "hoisted3";
                        
                        // Level 6: Ultra-deep if
                        if (k === 0) {
                            let ifLet3 = loopLet3 + 1000;
                            const ifConst3 = ifLet3 + 2000;
                            var hoistedFromIf3 = "hoisted-if3";
                            
                            // Level 7: Even deeper if
                            if (ifConst3 > 3000) {
                                let ifLet4 = ifConst3 + 5000;
                                const ifConst4 = ifLet4 * 10;
                                var hoistedFromIf4 = "hoisted-if4";
                                
                                // Level 8: Maximum depth if
                                if (ifConst4 > 50000) {
                                    let ultimateVar = ifConst4 + 100000;
                                    const ultimateConst = ultimateVar * 2;
                                    var ultimateHoisted = "ultimate-hoisted";
                                    
                                    // Access variables from ALL scope levels
                                    console.log("ACCESS ALL SCOPES:", 
                                        globalVar,           // level 0
                                        functionLet,         // level 0
                                        functionConst,       // level 0
                                        i,                   // level 1
                                        loopConst1,          // level 1
                                        loopLet1,            // level 1
                                        ifLet1,              // level 2
                                        ifConst1,            // level 2
                                        j,                   // level 3
                                        loopConst2,          // level 3
                                        loopLet2,            // level 3
                                        ifLet2,              // level 4
                                        ifConst2,            // level 4
                                        k,                   // level 5
                                        loopConst3,          // level 5
                                        loopLet3,            // level 5
                                        ifLet3,              // level 6
                                        ifConst3,            // level 6
                                        ifLet4,              // level 7
                                        ifConst4,            // level 7
                                        ultimateVar,         // level 8
                                        ultimateConst,       // level 8
                                        // All hoisted vars should be at level 0
                                        hoistedFromLoop1,
                                        hoistedFromIf1,
                                        hoistedFromLoop2,
                                        hoistedFromIf2,
                                        hoistedFromLoop3,
                                        hoistedFromIf3,
                                        hoistedFromIf4,
                                        ultimateHoisted
                                    );
                                }
                            }
                        }
                    }
                }
            }
        } else {
            // Level 2: else branch creates separate block scope
            let elseLet = "else-branch";
            const elseConst = 777;
            var hoistedFromElse = "hoisted-else";
        }
    }
    
    // Level 1: Second major for-loop (parallel to first)
    for (var varI = 0; varI < 2; varI++) {
        var hoistedVarLoop = "var-loop-hoisted";
        
        // Level 2: for loop inside var for-loop
        for (let whileCounter = 0; whileCounter < 1; whileCounter++) {
            let whileLet = varI + whileCounter;
            const whileConst = whileLet + 888;
            var hoistedFromWhile = "while-hoisted";
            
            // Level 3: try-catch block
            try {
                let tryLet = whileConst + 123;
                const tryConst = tryLet * 4;
                throw new Error("test");
            } catch (e) {
                let catchLet = "caught-error";
                const catchConst = 555;
                var hoistedFromCatch = "catch-hoisted";
            }
        }
    }
}
        )";
        
        std::cout << "\n📝 JavaScript code with MAXIMUM COMPLEXITY:" << std::endl;
        std::cout << ultra_complex_js << std::endl;
        
        // Expected variables with their correct scope levels
        std::vector<std::tuple<std::string, Assignment::DeclarationKind, int, bool>> expected_vars = {
            // Function-level variables (scope 0)
            {"globalVar", Assignment::VAR, 0, false},
            {"functionLet", Assignment::LET, 0, true},
            {"functionConst", Assignment::CONST, 0, true},
            
            // All hoisted var declarations (should be at function scope level 0)
            {"hoistedFromLoop1", Assignment::VAR, 0, false},
            {"hoistedFromIf1", Assignment::VAR, 0, false},
            {"hoistedFromLoop2", Assignment::VAR, 0, false},
            {"hoistedFromIf2", Assignment::VAR, 0, false},
            {"hoistedFromLoop3", Assignment::VAR, 0, false},
            {"hoistedFromIf3", Assignment::VAR, 0, false},
            {"hoistedFromIf4", Assignment::VAR, 0, false},
            {"ultimateHoisted", Assignment::VAR, 0, false},
            {"hoistedFromElse", Assignment::VAR, 0, false},
            {"varI", Assignment::VAR, 0, false},                // var for-loop variable hoists
            {"hoistedVarLoop", Assignment::VAR, 0, false},
            {"hoistedFromWhile", Assignment::VAR, 0, false},
            {"hoistedFromCatch", Assignment::VAR, 0, false},
            
            // Level 1: First for-loop block scope
            {"i", Assignment::LET, 1, true},
            {"loopConst1", Assignment::CONST, 1, true},
            {"loopLet1", Assignment::LET, 1, true},
            
            // Level 2: First if block scope  
            {"ifLet1", Assignment::LET, 2, true},
            {"ifConst1", Assignment::CONST, 2, true},
            
            // Level 3: Second for-loop block scope
            {"j", Assignment::LET, 3, true},
            {"loopConst2", Assignment::CONST, 3, true},
            {"loopLet2", Assignment::LET, 3, true},
            
            // Level 4: Second if block scope
            {"ifLet2", Assignment::LET, 4, true},
            {"ifConst2", Assignment::CONST, 4, true},
            
            // Level 5: Third for-loop block scope
            {"k", Assignment::LET, 5, true},
            {"loopConst3", Assignment::CONST, 5, true},
            {"loopLet3", Assignment::LET, 5, true},
            
            // Level 6: Third if block scope
            {"ifLet3", Assignment::LET, 6, true},
            {"ifConst3", Assignment::CONST, 6, true},
            
            // Level 7: Fourth if block scope
            {"ifLet4", Assignment::LET, 7, true},
            {"ifConst4", Assignment::CONST, 7, true},
            
            // Level 8: Ultimate depth if block scope
            {"ultimateVar", Assignment::LET, 8, true},
            {"ultimateConst", Assignment::CONST, 8, true},
            
            // Level 2: else branch block scope (parallel to first if)
            {"elseLet", Assignment::LET, 2, true},
            {"elseConst", Assignment::CONST, 2, true},
            
            // Level 2: while loop block scope (in second major branch)
            {"whileCounter", Assignment::LET, 1, true},      // for-loop creates level 1
            {"whileLet", Assignment::LET, 2, true},          // while creates level 2  
            {"whileConst", Assignment::CONST, 2, true},
            
            // Level 3: try block scope
            {"tryLet", Assignment::LET, 3, true},
            {"tryConst", Assignment::CONST, 3, true},
            
            // Level 3: catch block scope (parallel to try)
            {"catchLet", Assignment::LET, 3, true},
            {"catchConst", Assignment::CONST, 3, true}
        };
        
        std::cout << "\n🧪 Expected Variables: " << expected_vars.size() << " total" << std::endl;
        
        run_ultra_complex_analysis(ultra_complex_js, "ultraComplexScopeTest", expected_vars);
        
        std::cout << "\n🏆 ULTRA COMPLEX SCOPE TEST COMPLETED!" << std::endl;
    }

private:
    static void run_ultra_complex_analysis(const std::string& js_code, 
                                          const std::string& function_name,
                                          const std::vector<std::tuple<std::string, Assignment::DeclarationKind, int, bool>>& expected_vars) {
        
        try {
            // Parse JavaScript using UltraScript parser
            std::cout << "\n🔍 Parsing ultra-complex JavaScript..." << std::endl;
            
            GoTSCompiler compiler;
            auto parsed_result = compiler.parse_javascript(js_code);
            
            if (parsed_result.empty()) {
                std::cout << "❌ Failed to parse JavaScript code" << std::endl;
                return;
            }
            
            std::cout << "✅ Ultra-complex JavaScript parsed! AST nodes: " << parsed_result.size() << std::endl;
            
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
                std::cout << "❌ Function '" << function_name << "' not found" << std::endl;
                return;
            }
            
            std::cout << "✅ Found ultra-complex function: " << function_name << std::endl;
            
            // Run comprehensive static scope analysis
            std::cout << "\n🔬 Running COMPREHENSIVE static scope analysis..." << std::endl;
            StaticScopeAnalyzer analyzer;
            analyzer.analyze_function(function_name, target_function);
            
            std::cout << "✅ Ultra-complex scope analysis completed!" << std::endl;
            
            // Get detailed function analysis
            auto function_analysis = analyzer.get_function_analysis(function_name);
            
            std::cout << "\n📊 DETAILED SCOPE ANALYSIS RESULTS:" << std::endl;
            std::cout << "=====================================" << std::endl;
            std::cout << "Function: " << function_name << std::endl;
            std::cout << "Required parent scope levels: ";
            for (int level : function_analysis.required_parent_scopes) {
                std::cout << level << " ";
            }
            std::cout << std::endl;
            std::cout << "Function uses escaping variables: " << (function_analysis.has_escaping_variables ? "YES" : "NO") << std::endl;
            std::cout << "Total variables analyzed: " << expected_vars.size() << std::endl;
            
            // Validate every single variable
            std::cout << "\n🎯 VARIABLE-BY-VARIABLE VALIDATION:" << std::endl;
            std::cout << "===================================" << std::endl;
            
            int passed = 0;
            int failed = 0;
            std::vector<std::string> scope_level_summary[10];  // Track variables by scope level
            
            for (const auto& [var_name, expected_kind, expected_scope, expected_block_scoped] : expected_vars) {
                std::cout << "\n🔍 Variable '" << var_name << "':" << std::endl;
                
                auto var_info_ptr = &analyzer.get_variable_info(var_name);
                if (var_info_ptr->variable_name.empty()) {
                    std::cout << "   ❌ Variable not found in scope analysis!" << std::endl;
                    failed++;
                    continue;
                }
                
                // Display comprehensive variable info
                std::cout << "   📋 Declaration kind: " << 
                    (var_info_ptr->declaration_kind == Assignment::VAR ? "var" : 
                     var_info_ptr->declaration_kind == Assignment::LET ? "let" : "const") << std::endl;
                std::cout << "   📍 Scope level: " << var_info_ptr->scope_level << std::endl;
                std::cout << "   🔒 Block scoped: " << (var_info_ptr->is_block_scoped ? "true" : "false") << std::endl;
                std::cout << "   💾 Memory offset: " << var_info_ptr->offset_in_scope << std::endl;
                std::cout << "   📏 Size: " << var_info_ptr->size_bytes << " bytes" << std::endl;
                std::cout << "   🚀 Escapes function: " << (var_info_ptr->escapes_current_function ? "YES" : "NO") << std::endl;
                std::cout << "   🔥 Access frequency: " << var_info_ptr->access_frequency << std::endl;
                
                // Validate all properties
                bool tests_pass = (var_info_ptr->declaration_kind == expected_kind) &&
                                (var_info_ptr->scope_level == expected_scope) &&
                                (var_info_ptr->is_block_scoped == expected_block_scoped);
                
                if (tests_pass) {
                    std::cout << "   ✅ ALL PROPERTIES CORRECT!" << std::endl;
                    passed++;
                    scope_level_summary[expected_scope].push_back(var_name);
                } else {
                    std::cout << "   ❌ VALIDATION FAILED!" << std::endl;
                    std::cout << "      Expected: kind=" << (expected_kind == Assignment::VAR ? "var" : 
                                                           expected_kind == Assignment::LET ? "let" : "const")
                             << ", scope=" << expected_scope << ", block_scoped=" << expected_block_scoped << std::endl;
                    failed++;
                }
            }
            
            // Summary by scope levels
            std::cout << "\n📈 SCOPE LEVEL SUMMARY:" << std::endl;
            std::cout << "======================" << std::endl;
            for (int level = 0; level < 10; level++) {
                if (!scope_level_summary[level].empty()) {
                    std::cout << "Scope Level " << level << " (" << scope_level_summary[level].size() << " variables): ";
                    for (const auto& var : scope_level_summary[level]) {
                        std::cout << var << " ";
                    }
                    std::cout << std::endl;
                }
            }
            
            // Final results
            std::cout << "\n🎯 FINAL RESULTS:" << std::endl;
            std::cout << "=================" << std::endl;
            std::cout << "✅ Passed: " << passed << std::endl;
            std::cout << "❌ Failed: " << failed << std::endl;
            std::cout << "📊 Success rate: " << (double(passed) / (passed + failed) * 100.0) << "%" << std::endl;
            
            if (failed == 0) {
                std::cout << "\n🏆 PERFECT! ALL ULTRA-COMPLEX SCOPE ANALYSIS PASSED! 🏆" << std::endl;
                std::cout << "🚀 Your UltraScript compiler handles the most complex ES6 scoping scenarios flawlessly!" << std::endl;
            } else {
                std::cout << "\n⚠️ Some validations failed. The ultra-complex test revealed edge cases." << std::endl;
            }
            
        } catch (const std::exception& e) {
            std::cout << "💥 Ultra-complex test failed with exception: " << e.what() << std::endl;
        }
    }
};

int main() {
    std::cout << "🔥 ULTRA-COMPLEX ES6 SCOPING TEST SUITE 🔥" << std::endl;
    std::cout << "==========================================" << std::endl;
    std::cout << "Testing the absolute limits of ES6 block scoping!" << std::endl;
    
    try {
        UltraComplexScopeTest::run_insane_nested_scope_test();
        return 0;
    } catch (const std::exception& e) {
        std::cout << "💥 Ultra-complex test suite crashed: " << e.what() << std::endl;
        return 1;
    }
}
