function testComplexArrowFunctions() {
    var globalVar = "global";
    let blockScoped = "block";
    
    // Multiple parameter arrow function with block body
    const complexArrow = (a, b) => {
        let result = a + b;
        return result * 2;
    };
    
    // Single parameter without parentheses  
    const simpleArrow = x => x + 1;
    
    // No parameter arrow function
    const noParamArrow = () => "hello";
    
    console.log(globalVar, blockScoped);
    console.log(complexArrow(5, 3));
    console.log(simpleArrow(10));
    console.log(noParamArrow());
}
