// Comprehensive Reference Counting Test
// Tests all assignment combinations and verifies destructor calls and ref counts

class TestObject {
    name: string;
    
    constructor(name: string) {
        this.name = name;
    }
    
    destructor() {
        console.log("DESTRUCTOR: TestObject '" + this.name + "' destroyed");
    }
}

console.log("=== Starting Comprehensive Reference Counting Tests ===");

// Test 1: Basic object creation and reference counting
console.log("\n--- Test 1: Basic Object Creation ---");
let obj1 = new TestObject("obj1");
let addr1: uint64 = obj1.memoryAddress;
console.log("Created obj1, address:", addr1, "ref_count:", runtime.referenceCounter.getRefCount(addr1));

// Test 2: Object to Object assignment (should increment ref count)
console.log("\n--- Test 2: Object -> Object Assignment ---");
let obj2 = obj1;  // Should increment ref count of obj1
console.log("obj2 = obj1, ref_count:", runtime.referenceCounter.getRefCount(addr1));

// Test 3: Object to null assignment (should decrement ref count)
console.log("\n--- Test 3: Object -> null Assignment ---");
obj1 = null;  // Should decrement ref count but not destroy (obj2 still has reference)
console.log("obj1 = null, ref_count:", runtime.referenceCounter.getRefCount(addr1));

// Test 4: Last reference removal (should destroy object)
console.log("\n--- Test 4: Last Reference Removal ---");
obj2 = null;  // Should decrement ref count to 0 and destroy object
console.log("obj2 = null - object should be destroyed above");

// Test 5: ANY type assignments
console.log("\n--- Test 5: ANY Type Assignments ---");
let obj3 = new TestObject("obj3");
let addr3: uint64 = obj3.memoryAddress;
console.log("Created obj3, ref_count:", runtime.referenceCounter.getRefCount(addr3));

let anyVar: any = obj3;  // Object -> ANY(object)
console.log("anyVar = obj3, ref_count:", runtime.referenceCounter.getRefCount(addr3));

let anyVar2: any = anyVar;  // ANY(object) -> ANY(object)
console.log("anyVar2 = anyVar, ref_count:", runtime.referenceCounter.getRefCount(addr3));

// Test 6: ANY to primitive transition (should destroy object)
console.log("\n--- Test 6: ANY(object) -> primitive Assignment ---");
anyVar = 42;  // ANY(object) -> ANY(primitive) - should decrement ref count
console.log("anyVar = 42, ref_count:", runtime.referenceCounter.getRefCount(addr3));

// Test 7: Chained object assignments
console.log("\n--- Test 7: Chained Object Assignments ---");
let obj4 = new TestObject("obj4");
let addr4: uint64 = obj4.memoryAddress;
let obj5 = new TestObject("obj5");
let addr5: uint64 = obj5.memoryAddress;
console.log("Created obj4, ref_count:", runtime.referenceCounter.getRefCount(addr4));
console.log("Created obj5, ref_count:", runtime.referenceCounter.getRefCount(addr5));

obj4 = obj5;  // Should destroy obj4 and increment obj5 ref count
console.log("obj4 = obj5");
console.log("obj5 ref_count:", runtime.referenceCounter.getRefCount(addr5));

// Test 8: ANY to ANY object assignment
console.log("\n--- Test 8: ANY -> ANY Object Assignment ---");
let obj6 = new TestObject("obj6");
let addr6: uint64 = obj6.memoryAddress;
let anyVar3: any = 100;
let anyVar4: any = obj6;
console.log("Created obj6, ref_count:", runtime.referenceCounter.getRefCount(addr6));

anyVar3 = anyVar4;  // ANY(primitive) -> ANY(object)
console.log("anyVar3 = anyVar4, ref_count:", runtime.referenceCounter.getRefCount(addr6));

// Test 9: Complex assignment chains
console.log("\n--- Test 9: Complex Assignment Chains ---");
let obj7 = new TestObject("obj7");
let addr7: uint64 = obj7.memoryAddress;
let obj8 = new TestObject("obj8");
let addr8: uint64 = obj8.memoryAddress;

console.log("obj7 ref_count:", runtime.referenceCounter.getRefCount(addr7));
console.log("obj8 ref_count:", runtime.referenceCounter.getRefCount(addr8));

let temp: any = obj7;
obj7 = obj8;
obj8 = temp;  // Swap objects through ANY type
console.log("After swap:");
console.log("obj7 (was obj8) ref_count:", runtime.referenceCounter.getRefCount(addr8));
console.log("obj8 (was obj7) ref_count:", runtime.referenceCounter.getRefCount(addr7));

// Final cleanup - should trigger destructors for remaining objects
console.log("\n--- Final Cleanup ---");
obj3 = null;
anyVar2 = null;
obj5 = null;
obj6 = null;
anyVar3 = null;
anyVar4 = null;
obj7 = null;
obj8 = null;
temp = null;

console.log("=== Reference Counting Tests Complete ===");
