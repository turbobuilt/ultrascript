// Test dynamic property memory management
class TestObject {
    constructor(name) {
        this.name = name;
    }
}

// Create an object with dynamic properties
let obj = new TestObject("test");
obj.dynamicProp1 = "value1";
obj.dynamicProp2 = 42;
obj["bracket_prop"] = "bracket_value";

// Test access
console.log("Dynamic prop 1:", obj.dynamicProp1);
console.log("Dynamic prop 2:", obj.dynamicProp2);
console.log("Bracket prop:", obj["bracket_prop"]);

// Test modification  
obj.dynamicProp1 = "modified_value";
console.log("Modified prop:", obj.dynamicProp1);

// Create multiple objects to test memory cleanup
for (let i = 0; i < 10; i++) {
    let temp = new TestObject("temp" + i);
    temp.dynamicData = "data" + i;
    temp["index"] = i;
    // Objects should be cleaned up when going out of scope
}

console.log("Test completed - memory should be cleaned up properly");
