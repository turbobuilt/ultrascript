// Test file for function instance size computation

// Simple function - should be 16 bytes (no closures)
function simple() {
    let x = 42;
    return x;
}

// Function with one closure - should be 24 bytes (16 + 1*8)
let global_var = 100;
function withOneClosure() {
    return global_var + 1;
}

// Function with nested closures
function outer() {
    let outer_var = 200;
    
    function inner() {
        let inner_var = 300;
        
        // This should capture both outer_var and global_var
        function deepInner() {
            return global_var + outer_var + inner_var;
        }
        
        return deepInner();
    }
    
    return inner();
}

// Arrow function
let arrow = () => {
    return global_var * 2;
};

console.log("Testing function instance sizes");
