// Simple test for string property assignment without console.log

class SimpleTest {
    small_int: int32;
    bool_val: boolean;
    str: string;
}

let test = new SimpleTest();

// Test assignments only
test.small_int = 42;
test.bool_val = true;
test.str = "test string";

console.log("Assignment completed successfully");
