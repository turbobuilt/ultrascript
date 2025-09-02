function test() {
    console.log("Pure ASM function instance test");
    return 42;
}

let result = test();
console.log("Result:", result);

// Test closure with captured variables
function outer(x) {
    function inner() {
        return x + 10;
    }
    return inner;
}

let closure = outer(5);
let closureResult = closure();
console.log("Closure result:", closureResult);
