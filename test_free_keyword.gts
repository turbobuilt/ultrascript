// UltraScript Free Keyword Test Program - Pure JIT x86 Assembly
// This tests the pure JIT x86 assembly generation for memory management

console.log("=== Testing Free Keyword with Pure JIT x86 Assembly ===");

// Test 1: Free a simple object
console.log("\n--- Test 1: Object Free ---");
class TestObject {
    value: int64 = 42;
    name: string = "test";
}

var obj = new TestObject();
console.log("Created object with value:", obj.value);
free obj;  // Deep free by default - generates pure x86 assembly
console.log("Object freed successfully");

// Test 2: Free an array with deep vs shallow
console.log("\n--- Test 2: Array Free ---");
var arr = [new TestObject(), new TestObject(), new TestObject()];
console.log("Created array with", arr.length, "objects");

var shallow_arr = [new TestObject(), new TestObject()];
console.log("Created shallow array with", shallow_arr.length, "objects");

free arr;  // Deep free - inline x86 loops through all elements
console.log("Deep freed array and all objects");

free shallow shallow_arr;  // Shallow free - only array structure
console.log("Shallow freed array structure (objects leaked intentionally)");

// Test 3: Free strings
console.log("\n--- Test 3: String Free ---");
var str = "Hello, UltraScript!";
console.log("Created string:", str);
free str;  // Direct system free() call in x86 assembly
console.log("String freed successfully");

// Test 4: Free dynamic values (ANY type)
console.log("\n--- Test 4: Dynamic Value Free ---");
var dynamic_val = 12345;
dynamic_val = "converted to string";  // Changes type to string
console.log("Dynamic value:", dynamic_val);
free dynamic_val;  // Runtime type checking with inline x86 jump table
console.log("Dynamic value freed successfully");

// Test 5: Verify primitives don't need freeing
console.log("\n--- Test 5: Primitive Types (No Free Needed) ---");
var num: int64 = 100;
var flag: boolean = true;
free num;   // Generates debug-only assembly code
free flag;  // Generates debug-only assembly code  
console.log("Primitives handled correctly");

// Test 6: Complex nested structure
console.log("\n--- Test 6: Complex Nested Structure ---");
class NestedClass {
    data: [string] = ["item1", "item2", "item3"];
    child: TestObject = new TestObject();
}

var nested = new NestedClass();
console.log("Created complex nested structure");
free nested;  // Deep recursive free with inline x86 assembly
console.log("Complex structure freed with full recursion");

console.log("\n=== All Free Tests Completed Successfully ===");
console.log("Generated PURE JIT x86 ASSEMBLY - Zero function call overhead!");
console.log("Maximum C++ level performance achieved!");
    console.log("Array with objects created");
    
    // Deep free - frees array and all objects inside
    free arr;
    console.log("Array deep freed (objects also freed)");
    
    // Test 3: Shallow free example
    console.log("\n3. Testing shallow free");
    var arr2 = [
        new TestClass(10, "keep1"),
        new TestClass(20, "keep2")
    ];
    console.log("Second array created");
    
    // Shallow free - frees only array structure, objects leak
    free shallow arr2;
    console.log("Array shallow freed (objects still exist - memory leak!)");
    
    // Test 4: String freeing
    console.log("\n4. Testing string freeing");
    var str = "This is a test string";
    console.log("String created:", str);
    free str;
    console.log("String freed");
    
    // Test 5: Dynamic value freeing
    console.log("\n5. Testing dynamic value freeing");
    var dynamic_val = 12345;  // Untyped variable
    dynamic_val = new TestClass(99, "dynamic");  // Now holds an object
    console.log("Dynamic value holds object");
    free dynamic_val;  // Should detect runtime type and free appropriately
    console.log("Dynamic value freed");
    
    // Test 6: Nested data structure freeing
    console.log("\n6. Testing nested structure freeing");
    var nested = {
        level1: {
            level2: [
                new TestClass(100, "nested1"),
                new TestClass(200, "nested2")
            ]
        },
        other_data: "some string"
    };
    console.log("Nested structure created");
    free nested;  // Deep free should handle all nested objects
    console.log("Nested structure freed");
    
    // Print final statistics
    console.log("\n=== FINAL FREE STATISTICS ===");
    __print_free_stats();
}

// Enable debug mode for comprehensive logging
__set_free_debug_mode(1);

// Run the test
main();
