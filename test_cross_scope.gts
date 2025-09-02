// Test cross-scope variable resolution
function outer(x) {
    function inner() {
        return x;
    }
    return inner;
}

let closure = outer(42);
let result = closure();
console.log(result);
