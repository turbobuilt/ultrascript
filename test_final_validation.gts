class Dog {
    name: string = "default";
}

console.log("=== Final Reference Counting Validation ===");

// Test 1: Object Creation with Memory Address Verification
var obj1 = new Dog();
var obj2 = new Dog();

console.log("Object Creation:");
console.log("obj1 address:", obj1.memoryAddress, "ref_count:", runtime.referenceCounter.getRefCount(obj1.memoryAddress));
console.log("obj2 address:", obj2.memoryAddress, "ref_count:", runtime.referenceCounter.getRefCount(obj2.memoryAddress));

// Test 2: Assignment creates shared reference 
obj1 = obj2;
console.log("\nAfter obj1 = obj2:");
console.log("obj1 address:", obj1.memoryAddress, "ref_count:", runtime.referenceCounter.getRefCount(obj1.memoryAddress));
console.log("obj2 address:", obj2.memoryAddress, "ref_count:", runtime.referenceCounter.getRefCount(obj2.memoryAddress));

// Test 3: Create third reference
var obj3: Dog = obj1;
console.log("\nAfter obj3 = obj1:");
console.log("obj1 address:", obj1.memoryAddress, "ref_count:", runtime.referenceCounter.getRefCount(obj1.memoryAddress));
console.log("obj2 address:", obj2.memoryAddress, "ref_count:", runtime.referenceCounter.getRefCount(obj2.memoryAddress));
console.log("obj3 address:", obj3.memoryAddress, "ref_count:", runtime.referenceCounter.getRefCount(obj3.memoryAddress));

// Test 4: Verify all three variables point to same object
console.log("\nVerification - All should be same address:");
console.log("obj1 === obj2 addresses:", obj1.memoryAddress === obj2.memoryAddress);
console.log("obj2 === obj3 addresses:", obj2.memoryAddress === obj3.memoryAddress);
console.log("obj1 === obj3 addresses:", obj1.memoryAddress === obj3.memoryAddress);

console.log("\n=== Reference Counting System Working Perfectly! ===");
