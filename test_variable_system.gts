// Test the new one-variable-per-scope system with error checking
var x: string = "hello";
var y: int64 = 42;

console.log(x);
console.log(y);

// This should work: function hoisting as dynamic value
function test() {
    console.log("Inside test function");
}

test();

// Try to call the hoisted function
test();
