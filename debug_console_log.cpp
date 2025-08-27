#include "compiler.h"
#include <iostream>

int main() {
    std::cout << "🔍 CONSOLE.LOG DEBUG TEST" << std::endl;
    
    // Test minimal console.log to see what parsing issue occurs
    std::string test_js = R"(
function testConsoleLog() {
    let testVar = "hello";
    console.log("test", testVar);
}
)";
    
    std::cout << "📝 Testing minimal console.log JavaScript:" << std::endl;
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
