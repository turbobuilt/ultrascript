// Test that number type annotations now use FLOAT64 internally
var x: number = 24.2;
var y = 42;  // Should also be FLOAT64
var z: float64 = 3.14;

console.log(x);
console.log(y); 
console.log(z);

// Test with arrays
var arr = [1.5, 2.7, 3.9];
console.log(arr.sum());
