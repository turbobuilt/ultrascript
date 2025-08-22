class Dog {
    name: string = "default";
}

console.log("=== Test: Object Creation Order ===");
var obj1 = new Dog();
console.log("obj1 created, ref_count:", runtime.referenceCounter.getRefCount(obj1.memoryAddress));

console.log("Creating obj2...");
var obj2 = new Dog(); 
console.log("obj2 created, ref_count:", runtime.referenceCounter.getRefCount(obj2.memoryAddress));

console.log("Both objects created successfully. obj1=", obj1.memoryAddress, "obj2=", obj2.memoryAddress);
console.log("Test completed without assignment");
