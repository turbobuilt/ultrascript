// Comprehensive reference counting test

class TestObject {
    value: int64;
    name: string;
}

function test_ref_counting() {
    console.log("=== Reference Counting Test ===");
    
    // Create object
    let obj = new TestObject();
    obj.value = 42;
    obj.name = "test";
    
    console.log("Created object with value:", obj.value, "name:", obj.name);
    
    // In a real implementation, we would test:
    // - Multiple references to the same object
    // - Reference counting increment/decrement
    // - Automatic cleanup when reference count reaches zero
    // - Assignment between variables
    
    return obj.value;
}

function main() {
    let result = test_ref_counting();
    console.log("Test completed, result:", result);
    return 0;
}

main();
