#include "compiler.h"
#include "static_scope_analyzer.h"
#include <iostream>
#include <memory>
#include <string>

using namespace std;

int main() {
    std::cout << "\n🔍 SIMPLE SCOPE OFFSET VALIDATION TEST\n" << std::endl;

    string js_code = R"(
var moduleVar = "global";
var sharedVar = "global";

function testFunction() {
    var functionVar = "function";
    var sharedVar = "function-shadowed";
    
    console.log(sharedVar);     // Current scope (expected=0)
    console.log(functionVar);   // Current scope (expected=0) 
    console.log(moduleVar);     // Parent scope (expected=1)
    
    {
        let blockVar = "block";
        console.log(blockVar);      // Current scope (expected=0)
        console.log(sharedVar);     // Parent scope (expected=1)
        console.log(moduleVar);     // Grandparent scope (expected=2)
    }
}
)";

    std::cout << "📝 JAVASCRIPT CODE TO ANALYZE:" << std::endl;
    std::cout << js_code << std::endl;

    // Parse the JavaScript code
    GoTSCompiler compiler;
    auto parsed_result = compiler.parse_javascript(js_code);
    
    if (parsed_result.empty()) {
        std::cerr << "❌ Failed to parse JavaScript" << std::endl;
        return 1;
    }

    std::cout << "✅ JavaScript successfully parsed! AST nodes: " << parsed_result.size() << std::endl;
    
    // Find the testFunction
    FunctionDecl* target_function = nullptr;
    for (auto& node : parsed_result) {
        if (auto* func = dynamic_cast<FunctionDecl*>(node.get())) {
            if (func->name == "testFunction") {
                target_function = func;
                std::cout << "✅ Found function: " << func->name << std::endl;
                break;
            }
        }
    }
    
    if (!target_function) {
        std::cerr << "❌ Could not find testFunction in AST" << std::endl;
        return 1;
    }
    
    // Analyze scope offsets  
    std::cout << "\n🔬 ANALYZING SCOPE OFFSETS with StaticScopeAnalyzer..." << std::endl;
    
    StaticScopeAnalyzer analyzer;
    analyzer.analyze_function("testFunction", target_function);

    std::cout << "\n=== SCOPE OFFSET RESULTS ===" << std::endl;
    
    // Get function analysis results
    auto function_analysis = analyzer.get_function_analysis("testFunction");
    
    std::cout << "\nFunction: testFunction" << std::endl;
    std::cout << "Scope analysis complete - check debug output above for scope level calculations" << std::endl;
    
    std::cout << "\n✅ SCOPE OFFSET ANALYSIS COMPLETE!" << std::endl;
    std::cout << "\n📊 Look at the debug output above to see scope_level calculations for each variable access" << std::endl;
    return 0;
}
