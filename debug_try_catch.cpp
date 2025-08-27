#include "compiler.h"
#include <iostream>

int main() {
    std::cout << "🔍 TRY-CATCH DEBUG TEST" << std::endl;
    
    // Test minimal try-catch
    std::string test_js = R"(
function testTryCatch() {
    try {
        let test = "hello";
    } catch (error) {
        console.log("caught");
    }
}
)";
    
    std::cout << "📝 Testing minimal try-catch:" << std::endl;
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
