// Test AST depth storage for TypeInference migration
function testScopeDepth() {
    let outerVar = 10;
    
    if (true) {
        let innerVar = 20;
        outerVar = 30;  // Cross-scope assignment
        
        if (true) {
            let deepVar = 40;
            outerVar = deepVar;  // Deep cross-scope assignment
            innerVar = 50;      // Mid-level assignment
        }
    }
}

testScopeDepth();
console.log("AST depth storage test completed");
