// Reference Counting Comprehensive Test Suite
// This tests all assignment combinations for reference counting

class Dog {
    name: string = "default";
    
    destructor() {
        console.log("Dog destroyed:", this.name);
    }
}

// Test 1: Basic object creation and reference counting
console.log("=== Test 1: Basic Object Creation ===");
let obj1 = new Dog();
obj1.name = "Buddy";
console.log("Created obj1:", obj1.name);

// Test 2: Object assignment (should increment ref count)
console.log("=== Test 2: Object Assignment ===");
let obj2 = obj1;  // obj1 ref count should go from 1 to 2
obj2.name = "Rex";
console.log("obj1.name:", obj1.name);  // Should be "Rex" (same object)
console.log("obj2.name:", obj2.name);  // Should be "Rex" (same object)

// Test 3: Object reassignment (should decrement old, increment new)
console.log("=== Test 3: Object Reassignment ===");
let obj3 = new Dog();
obj3.name = "Spot";
obj2 = obj3;  // obj1 ref count should drop, obj3 ref count should increase
console.log("obj1.name:", obj1.name);  // Should still be "Rex"
console.log("obj2.name:", obj2.name);  // Should be "Spot"
console.log("obj3.name:", obj3.name);  // Should be "Spot"

// Test 4: Setting to null (should decrement and potentially destroy)
console.log("=== Test 4: Setting to Null ===");
obj1 = null;  // Should decrement ref count of "Rex" dog

// Test 5: ANY variable assignments
console.log("=== Test 5: ANY Variable Assignments ===");
let anyVar: any = new Dog();
anyVar.name = "AnyDog";
console.log("anyVar.name:", anyVar.name);

let anotherAny: any = anyVar;  // Should increment ref count
anotherAny.name = "SharedDog";
console.log("anyVar.name:", anyVar.name);      // Should be "SharedDog"
console.log("anotherAny.name:", anotherAny.name);  // Should be "SharedDog"

// Test 6: ANY to primitive (should destroy object)
console.log("=== Test 6: ANY to Primitive ===");
anyVar = 42;  // Should decrement ref count, might destroy

console.log("=== Test 7: Final Cleanup ===");
obj2 = null;
obj3 = null;
anotherAny = null;

console.log("Test completed - watch for destructor calls!");
