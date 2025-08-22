class Dog {
    name: string = "default";
}

console.log("=== Test: Simple Assignment ===");
var obj1 = new Dog();
console.log("obj1 created");

var obj2 = new Dog();
console.log("obj2 created");

console.log("About to perform assignment...");
obj1 = obj2;
console.log("Assignment completed");
