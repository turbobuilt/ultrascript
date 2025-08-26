// Test: Basic lexical scope with variable capture
function outerFunction() {
    var x = 10;
    
    function innerFunction() {
        return x;  // This should capture x from parent scope
    }
    
    return innerFunction();
}

var result = outerFunction();
console.log("Result:", result);
