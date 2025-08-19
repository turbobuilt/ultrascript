// Test UltraScript dynamic property support

class TestClass {
    constructor() {
        // Constructor can set static properties
        this.staticProp = 42;
    }
}

// Create an instance
let obj = new TestClass();

// Access static property - should work with direct offset
console.log("Static property:", obj.staticProp);

// Set dynamic property - should go to hash map
obj.dynamicProp = "Hello World";

// Access dynamic property - should use hash map lookup
console.log("Dynamic property:", obj.dynamicProp);

// Set another dynamic property
obj.anotherDynamic = 3.14;

// Access it
console.log("Another dynamic property:", obj.anotherDynamic);

// Try to access non-existent property
console.log("Non-existent property:", obj.nonExistent);

console.log("Dynamic property test completed!");
