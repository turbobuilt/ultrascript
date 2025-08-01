// Ultra-performance path
console.log("Program started!");
console.log("starting typed")
var matrix = Array.zeros([1000], "float64");
console.log("Matrix is", matrix);
matrix.push(3.14159);  // Ultra-fast with type conversion
var sum = matrix.sum();      // SIMD-optimized

console.log("flexible")
// Flexible path  s
var mixed = [];
mixed.push(42);
mixed.push("hello");
mixed.push([1,2,3]);         // Can store anything

// Type safety
console.log("more typed");
var int_arr = Array.zeros([1], "int64");
int_arr.push(3.14);          // ✓ Converts to 3
int_arr.push("hello");       // ❌ Crashes - incompatible type