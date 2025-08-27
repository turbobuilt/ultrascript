#include "compiler.h"
#include <iostream>

int main() {
    std::cout << "🔍 SWITCH WITH CASE BLOCKS DEBUG TEST" << std::endl;
    
    // Test switch with case blocks
    std::string test_js = R"(
function testSwitchBlocks() {
    let x = 5;
    switch (x % 3) {
        case 0: {
            let case0Var = "case0";
            console.log(case0Var);
        }
        break;
        case 1: {
            let case1Var = "case1";
            console.log(case1Var);
        }
        break;
        default: {
            let defaultVar = "default";
            console.log(defaultVar);
        }
    }
}
)";
    
    std::cout << "📝 Testing switch with case block scopes:" << std::endl;
    std::cout << test_js << std::endl;
    
    try {
        GoTSCompiler compiler;
        std::cout << "🔍 PARSING with REAL UltraScript GoTSCompiler..." << std::endl;
        
        auto ast = compiler.parse_javascript(test_js);
        std::cout << "✅ Parse successful!" << std::endl;
    } catch (const std::exception& e) {
        std::cout << "❌ Parse error: " << e.what() << std::endl;
    }
    
    return 0;
}
