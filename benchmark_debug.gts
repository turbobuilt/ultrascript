console.log("Testing benchmark step by step");

console.log("Step 1: Create matrix");
var matrix = Array.zeros([1000], "float64");
console.log("Matrix created successfully");

console.log("Step 2: Push to matrix");
matrix.push(3.14159);
console.log("Push completed successfully");

console.log("Step 3: Call sum()");
var sum = matrix.sum();
console.log("Sum result:", sum);

console.log("All benchmark steps completed");
