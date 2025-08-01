// Test console.log with ANY type variables
var x = 42;           // Should be ANY type
var y = 3.14;         // Should be ANY type  
var z = true;         // Should be ANY type
var w: int64 = 100;   // Should be specific int64 type

console.log("Testing ANY type console.log:");
console.log("x =", x);
console.log("y =", y); 
console.log("z =", z);
console.log("w =", w);
