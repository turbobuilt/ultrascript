console.log("Testing Array.zeros with increasing sizes");

console.log("Size 10:");
var arr10 = Array.zeros([10], "float64");
console.log("Size 10 success");

console.log("Size 100:");
var arr100 = Array.zeros([100], "float64");
console.log("Size 100 success");

console.log("Size 500:");
var arr500 = Array.zeros([500], "float64");
console.log("Size 500 success");

console.log("Size 1000:");
var arr1000 = Array.zeros([1000], "float64");
console.log("Size 1000 success");

console.log("All sizes completed successfully");
