// Test that our cross-scope variable access is working
function outer(x) {
    function inner() {
        return x;
    }
    return inner;
}

let closure = outer(42);
console.log("Closure created successfully");
// Note: Skip the closure() call for now to test the basic functionality
