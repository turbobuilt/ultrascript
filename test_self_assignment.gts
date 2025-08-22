class Dog {
    name: string = "default";
}

console.log("=== Test: Stack Layout Debug ===");
var obj1 = new Dog();
console.log("obj1 created at:", runtime.referenceCounter.getRefCount(obj1.memoryAddress));

console.log("About to assign obj1 to obj1 (self-assignment)...");
obj1 = obj1;  // This should be safe - no crash expected
console.log("Self-assignment completed");

console.log("Test completed");
