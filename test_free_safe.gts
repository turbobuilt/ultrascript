console.log("=== Safe Free Shallow Tests ===");

// Test 1: String freeing
console.log("\n--- Test 1: String Freeing ---");
let str1 = "Hello World";
console.log("Before free: str1 =", str1);
free shallow str1;
console.log("After free shallow: str1 freed");

// Test 2: Array shallow freeing
console.log("\n--- Test 2: Array Shallow Freeing ---");
let arr1 = [1, 2, 3, 4, 5];
console.log("Before free: arr1 =", arr1);
free shallow arr1;
console.log("After free shallow: arr1 freed");

// Test 3: Dynamic value freeing
console.log("\n--- Test 3: Dynamic Value (any) Freeing ---");
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

console.log("\n=== All Safe Tests Complete ===");
console.log("Success! No double-free errors.");
