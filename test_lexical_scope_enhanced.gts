// Test lexical scope with variable access
function outer(x) {
    var y = 42;
    
    go function inner() {
        console.log(x + y); // Should access outer scope variables
    };
}
