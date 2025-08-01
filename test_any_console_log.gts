// Test for ANY type console.log implementation
// This should properly read DynamicValue structures and print with correct types

// Test different types assigned to ANY variables
let anyVar = 42.5;
console.log("Number in ANY:", anyVar);

anyVar = 123;
console.log("Integer in ANY:", anyVar);

anyVar = true;
console.log("Boolean in ANY:", anyVar);

anyVar = "Hello World";
console.log("String in ANY:", anyVar);

// Test multiple values
console.log("Multiple ANY values:", anyVar, 42.5, true);
