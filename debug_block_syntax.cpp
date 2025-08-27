#include "compiler.h"
#include <iostream>

int main() {
    std::cout << "🔍 BLOCK SYNTAX DEBUG TEST" << std::endl;
    
    // Test standalone block syntax
    std::string test_js = R"(
function testBlock() {
    let x = 1;
    {
        let y = 2;
        console.log(x, y);
    }
}
)";
    
    std::cout << "📝 Testing standalone block syntax:" << std::endl;
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
