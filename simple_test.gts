console.log("=== UltraScript Array System Test ===");

console.log("1. Basic array literal");
var arr1 = [1, 2, 3];
console.log("Created [1, 2, 3]");

console.log("2. Array.zeros without dtype");
var arr2 = Array.zeros([5]);
console.log("Created Array.zeros([5])");

console.log("3. Array.zeros with dtype");
var arr3 = Array.zeros([3], "float64");
console.log("Created Array.zeros([3], 'float64')");

console.log("4. Empty array");
var arr4 = [];
console.log("Created []");

console.log("=== All tests completed ===");
