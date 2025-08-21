class Person {
    name: string;
    age: int64;
}

class Car {
    brand: string;
    model: string;
}

// Test 1: Basic object assignment and reassignment
console.log("=== Test 1: Basic Assignment ===");
var person1 = new Person{ name: "Alice", age: 30 };
console.log("Created person1:", person1.name, person1.age);

var person2 = person1;  // This should increment ref count
console.log("Assigned person1 to person2:", person2.name, person2.age);

// Test 2: Reassignment should decrement old object's ref count
console.log("=== Test 2: Reassignment ===");
var anotherPerson = new Person{ name: "Bob", age: 25 };
console.log("Created anotherPerson:", anotherPerson.name, anotherPerson.age);

person2 = anotherPerson;  // Should decrement person1's ref count and increment anotherPerson's
console.log("Reassigned person2 to anotherPerson:", person2.name, person2.age);

// Test 3: Multiple reassignments
console.log("=== Test 3: Multiple Reassignments ===");
var car = new Car{ brand: "Toyota", model: "Camry" };
console.log("Created car:", car.brand, car.model);

person2 = car;  // Should work with ANY type assignment
console.log("Assigned car to person2 (ANY type)");

// Test 4: Objects going out of scope / being reassigned to null-like values
console.log("=== Test 4: Cleanup Test ===");
person1 = new Person{ name: "Charlie", age: 35 };  // Original person1 should be cleaned up
console.log("Reassigned person1 to new object:", person1.name, person1.age);

console.log("=== Reference Counting Test Complete ===");
