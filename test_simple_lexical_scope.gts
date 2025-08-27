// Test the new Simple Lexical Scope System
// This tests absolute depth tracking and register allocation

function outer() {
    let x = 10;  // depth 1
    
    function middle(y) { // depth 2
        let z = 20;
        
        function inner() { // depth 3
            // Should access:
            // x from depth 1 (using r12 for parent-of-parent)
            // y from depth 2 (using r13 for parent) 
            // z from depth 2 (using r13 for parent)
            console.log("x=" + x + ", y=" + y + ", z=" + z);
            return x + y + z;
        }
        
        return inner();
    }
    
    return middle(5);
}

// Call the function to test lexical scope
let result = outer();
console.log("Result: " + result);
