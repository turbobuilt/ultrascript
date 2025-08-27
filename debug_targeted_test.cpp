#include "compiler.h"
#include <iostream>

int main() {
    std::cout << "🔍 TARGETED DEBUG TEST - Finding the exact parsing issue" << std::endl;
    
    // Test from the point where parsing started to fail
    std::string test_js = R"(
function ultimateComplexityTest() {
    var globalVar1 = "function-scoped-1";
    let functionLet1 = "function-block-1";
    const functionConst1 = 100;
    var globalVar2 = "function-scoped-2";
    
    for (let outerI = 0; outerI < 5; outerI++) {
        const outerLoopConst = outerI * 10;
        let outerLoopLet = outerLoopConst + 5;
        var hoistedFromOuter = "hoisted-outer";
        
        if (outerI > 1) {
            let ifLet1 = outerLoopLet + 20;
            const ifConst1 = ifLet1 * 2;
            var hoistedFromIf1 = "hoisted-if-1";
            
            for (let middleJ = 0; middleJ < 3; middleJ++) {
                const middleLoopConst = middleJ + ifConst1;
                let middleLoopLet = middleLoopConst * 3;
                var hoistedFromMiddle = "hoisted-middle";
                
                // This is where parsing likely starts to fail
                try {
                    let tryLet1 = middleLoopLet + 100;
                    console.log("simple test");
                } catch (error) {
                    console.log("catch test");
                }
            }
        }
    }
}
)";
    
    std::cout << "📝 Testing problematic section:" << std::endl;
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
