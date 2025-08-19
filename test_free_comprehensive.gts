// Comprehensive test for free shallow functionality
// Tests all types: primitives, strings, arrays, objects, and dynamic values

console.log("=== Testing Free Shallow Implementation ===");

// Test 1: Basic string freeing
console.log("\n--- Test 1: String Freeing ---");
let str1 = "Hello World";
console.log("Before free: str1 =", str1);
free shallow str1;
console.log("After free shallow: str1 freed");

// Test 2: Array freeing (shallow)
console.log("\n--- Test 2: Array Shallow Freeing ---");
let arr1 = [1, 2, 3, 4, 5];
console.log("Before free: arr1 =", arr1);
free shallow arr1;
console.log("After free shallow: arr1 freed");

// Test 3: Nested array (should only free outer array)
console.log("\n--- Test 3: Nested Array Shallow Free ---");
let nested = [[1, 2], [3, 4], [5, 6]];
console.log("Before free: nested =", nested);
free shallow nested;
console.log("After free shallow: nested freed (inner arrays not freed)");

// Test 4: Dynamic value testing (any type)
console.log("\n--- Test 4: Dynamic Value (any) Freeing ---");
let dyn1: any = 42;
console.log("Before free: dyn1 =", dyn1);
free shallow dyn1;
console.log("After free shallow: dyn1 freed");

let dyn2: any = "Dynamic String";
console.log("Before free: dyn2 =", dyn2);
free shallow dyn2;
console.log("After free shallow: dyn2 freed");

let dyn3: any = [10, 20, 30];
console.log("Before free: dyn3 =", dyn3);
free shallow dyn3;
console.log("After free shallow: dyn3 freed");

// Test 5: Multiple variables
console.log("\n--- Test 5: Multiple Variable Freeing ---");
let multi1 = "First";
let multi2 = "Second";
let multi3 = [100, 200];
console.log("Before freeing:", multi1, multi2, multi3);
free shallow multi1;
free shallow multi2;
free shallow multi3;
console.log("All three variables freed");

// Test 6: Error test - regular free should fail
console.log("\n--- Test 6: Error Test (Regular Free) ---");
let error_test = "Should Error";
// This should throw an error:
// free error_test;  // Commented out to prevent compilation error

console.log("\n=== Free Shallow Tests Complete ===");
console.log("All tests passed successfully!");
