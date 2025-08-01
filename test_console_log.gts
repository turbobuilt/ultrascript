// Test the new type-aware console.log system
console.log("Testing type-aware console.log:");

// Test different data types
var int_value = 42;
var float_value = 3.14159;
var bool_value = true;
var string_value = "Hello, World!";

console.log("Integer:", int_value);
console.log("Float:", float_value);
console.log("Boolean:", bool_value);
console.log("String:", string_value);

// Test multiple arguments
console.log("Multiple args:", int_value, float_value, bool_value, string_value);

// Test array
var arr = [1, 2, 3];
console.log("Array:", arr);

// Test mixed types in one line
console.log("Mixed:", 123, "text", true, 99.9);
