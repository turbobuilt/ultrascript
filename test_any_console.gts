// Test console.log with ANY type variables
let x = 42;          // Should be stored as DynamicValue(int64_t) 
let y = 3.14;        // Should be stored as DynamicValue(double)
let z = "hello";     // Should be stored as DynamicValue(string)
let w = true;        // Should be stored as DynamicValue(bool)

console.log(x);      // Should print: 42 (as int64)
console.log(y);      // Should print: 3.14 (as double) 
console.log(z);      // Should print: hello (as string)
console.log(w);      // Should print: true (as bool)
