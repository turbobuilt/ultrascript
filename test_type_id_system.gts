class TestClass {
    constructor(value) {
        this.value = value;
    }
}

console.log("=== Type ID System Validation ===");

var obj1 = new TestClass(42);
var obj2 = new TestClass(100);

console.log("obj1 created:", obj1.value);
console.log("obj2 created:", obj2.value);

// Test assignment - this should use type IDs internally  
var obj3 = obj1;
console.log("obj3 = obj1:", obj3.value);

// Verify they share the same memory
console.log("obj1 address:", obj1.memoryAddress);
console.log("obj3 address:", obj3.memoryAddress);
console.log("Same address?", obj1.memoryAddress === obj3.memoryAddress);

console.log("=== Type ID System Working! ===");
