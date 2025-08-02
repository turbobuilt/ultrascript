// Simple test for property assignment with basic types

class SimpleTest {
    small_int: int32;
    bool_val: boolean;
}

let test = new SimpleTest();

// Test small values first
test.small_int = 42;
test.bool_val = true;

console.log("small_int:", test.small_int);
console.log("bool_val:", test.bool_val);
