class Dog {
    name: string = "default";
}

console.log("=== Comprehensive Reference Counting Test ===");

console.log("Test 1: Object Creation and Reference Counting");
var obj1 = new Dog();
console.log("obj1 created, ref_count:", runtime.referenceCounter.getRefCount(obj1.memoryAddress));

var obj2 = new Dog();
console.log("obj2 created, ref_count:", runtime.referenceCounter.getRefCount(obj2.memoryAddress));

console.log("\nTest 2: Assignment Between Objects");
console.log("Before assignment - obj1:", runtime.referenceCounter.getRefCount(obj1.memoryAddress), "obj2:", runtime.referenceCounter.getRefCount(obj2.memoryAddress));

obj1 = obj2;  // obj1 now points to obj2's object
console.log("After assignment - obj1:", runtime.referenceCounter.getRefCount(obj1.memoryAddress), "obj2:", runtime.referenceCounter.getRefCount(obj2.memoryAddress));

console.log("\nTest 3: Self Assignment");
obj1 = obj1;  // Self assignment should be safe
console.log("After self-assignment - obj1:", runtime.referenceCounter.getRefCount(obj1.memoryAddress));

console.log("\nTest 4: Property Access");
console.log("obj1.name:", obj1.name);
console.log("obj2.name:", obj2.name);

console.log("\nAll tests completed successfully!");
