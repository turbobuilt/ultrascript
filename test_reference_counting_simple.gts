// Simplified Reference Counting Test
// Tests basic assignment scenarios and verifies destructor calls and ref counts

class TestObject {
    name: string;
    
    constructor(name: string) {
        this.name = name;
    }
    
    destructor() {
        console.log("DESTRUCTOR: TestObject '" + this.name + "' destroyed");
    }
}

console.log("=== Starting Reference Counting Tests ===");

// Test 1: Basic object creation and reference counting
console.log("\n--- Test 1: Basic Object Creation ---");
let obj1 = new TestObject("obj1");
let addr1: uint64 = obj1.memoryAddress;
console.log("Created obj1, address:", addr1, "ref_count:", runtime.referenceCounter.getRefCount(addr1));

// Test 2: Object to Object assignment (should increment ref count)
console.log("\n--- Test 2: Object -> Object Assignment ---");
let obj2 = obj1;  // Should increment ref count of obj1
console.log("obj2 = obj1, ref_count:", runtime.referenceCounter.getRefCount(addr1));

// Test 3: ANY type assignments  
console.log("\n--- Test 3: ANY Type Assignments ---");
let obj3 = new TestObject("obj3");
let addr3: uint64 = obj3.memoryAddress;
console.log("Created obj3, ref_count:", runtime.referenceCounter.getRefCount(addr3));

let anyVar: any = obj3;  // Object -> ANY(object)
console.log("anyVar = obj3, ref_count:", runtime.referenceCounter.getRefCount(addr3));

let anyVar2: any = anyVar;  // ANY(object) -> ANY(object)
console.log("anyVar2 = anyVar, ref_count:", runtime.referenceCounter.getRefCount(addr3));

// Test 4: ANY to primitive transition (should destroy object)
console.log("\n--- Test 4: ANY(object) -> primitive Assignment ---");
anyVar = 42;  // ANY(object) -> ANY(primitive) - should decrement ref count
console.log("anyVar = 42, ref_count:", runtime.referenceCounter.getRefCount(addr3));

// Test 5: Object replacement
console.log("\n--- Test 5: Object Replacement ---");
let obj4 = new TestObject("obj4");
let addr4: uint64 = obj4.memoryAddress;
let obj5 = new TestObject("obj5");
let addr5: uint64 = obj5.memoryAddress;
console.log("Created obj4, ref_count:", runtime.referenceCounter.getRefCount(addr4));
console.log("Created obj5, ref_count:", runtime.referenceCounter.getRefCount(addr5));

obj4 = obj5;  // Should destroy obj4 and increment obj5 ref count
console.log("obj4 = obj5");
console.log("obj5 ref_count:", runtime.referenceCounter.getRefCount(addr5));

console.log("=== Reference Counting Tests Complete ===");
