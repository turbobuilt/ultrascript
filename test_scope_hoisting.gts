// Test file for variable scoping and function hoisting during parse phase

// Test function hoisting
function outer() {
    console.log("In outer function");
    
    // This should work due to function hoisting
    inner();
    
    function inner() {
        console.log("In inner function - hoisted");
    }
    
    // Test var hoisting
    if (true) {
        var hoistedVar = "I should be hoisted to function scope";
        let blockScoped = "I am block-scoped";
        const alsoBlockScoped = "I am also block-scoped";
    }
    
    // hoistedVar should be accessible here (hoisted to function scope)
    console.log(hoistedVar);
    
    // These should NOT be accessible here (block-scoped)
    // console.log(blockScoped); // Would be an error
    // console.log(alsoBlockScoped); // Would be an error
}

outer();
