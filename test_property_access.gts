// Test property access 

class PropertyTest {
    value: int32;
}

let test = new PropertyTest();
test.value = 42;

// Test property access
let result = test.value;
console.log("Property access completed, result is in variable");
