// Test direct scope pointer functionality
let globalVar = "global";

function outer() {
    let outerVar = "outer";
    
    function inner() {
        let innerVar = "inner";
        console.log(globalVar);  // Access from global scope
        console.log(outerVar);   // Access from outer scope  
        console.log(innerVar);   // Access from local scope
    }
    
    inner();
}

outer();
