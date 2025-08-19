// Comprehensive test for dynamic properties in UltraScript

class Person {
    constructor(name: string) {
        this.name = name;
        this.age = 25;
    }
    
    greet(): string {
        return "Hello, I'm " + this.name;
    }
}

// Test 1: Basic dynamic property assignment
console.log("=== Test 1: Basic Dynamic Properties ===");
let person = new Person("Alice");

// Static properties should work normally
console.log("Name:", person.name);
console.log("Age:", person.age);
console.log("Greeting:", person.greet());

// Dynamic properties
person.hobby = "Programming";
person.salary = 75000;
person.married = true;

console.log("Hobby:", person.hobby);
console.log("Salary:", person.salary);
console.log("Married:", person.married);

// Test 2: Bracket notation access
console.log("\n=== Test 2: Bracket Notation ===");
person["city"] = "New York";
person["country"] = "USA";

console.log("City:", person["city"]);
console.log("Country:", person["country"]);

// Test 3: Mixed static and dynamic access
console.log("\n=== Test 3: Mixed Access Patterns ===");
let key = "dynamicKey";
person[key] = "dynamicValue";
console.log("Dynamic via variable key:", person[key]);

// Test 4: Undefined properties
console.log("\n=== Test 4: Undefined Properties ===");
console.log("Undefined property:", person.undefinedProp);
console.log("Undefined via bracket:", person["undefinedProp"]);

console.log("\nAll dynamic property tests completed!");
