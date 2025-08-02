// Test direct property console.log

class PropertyTest {
    value: int32;
}

let test = new PropertyTest();
test.value = 42;

// Direct console.log without assignment
console.log(test.value);
