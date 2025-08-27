#include "compiler.h"
#include <iostream>

int main() {
    std::cout << "🔍 SWITCH STATEMENT DEBUG TEST" << std::endl;
    
    // Test switch statement
    std::string test_js = R"(
function testSwitch() {
    let x = 5;
    switch (x % 2) {
        case 0:
            console.log("even");
            break;
        case 1:
            console.log("odd");
            break;
        default:
            console.log("unknown");
    }
}
)";
    
    std::cout << "📝 Testing switch statement:" << std::endl;
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
