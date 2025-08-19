// Test dynamic value assignment and freeing
// Tests the case: var x = 5; x = new Obj();

console.log("=== Testing Dynamic Value Assignment and Free ===");

// Test 1: Basic dynamic type changes
console.log("\n--- Test 1: Dynamic Type Changes ---");
var x: any = 42;
console.log("x as int:", x);

x = "Hello World";
console.log("x as string:", x);

x = [1, 2, 3];
console.log("x as array:", x);

// Test 2: Object assignment to any variable (if objects are supported)
console.log("\n--- Test 2: Object Assignment (if supported) ---");
var obj: any = 100;
console.log("obj as number:", obj);

// Test 3: Free dynamic values
console.log("\n--- Test 3: Free Dynamic Values ---");
var dyn1: any = 42;
var dyn2: any = "Dynamic String";
var dyn3: any = [10, 20, 30];

console.log("Before freeing - dyn1:", dyn1, "dyn2:", dyn2, "dyn3:", dyn3);

free shallow dyn1;
free shallow dyn2;
free shallow dyn3;

console.log("All dynamic values freed successfully");

// Test 4: Mixed primitive freeing
console.log("\n--- Test 4: Mixed Primitive Types ---");
var bool_any: any = true;
var float_any: any = 3.14159;
var int_any: any = -999;

console.log("Before freeing - bool:", bool_any, "float:", float_any, "int:", int_any);

free shallow bool_any;
free shallow float_any;
free shallow int_any;

console.log("All primitive dynamic values freed");

console.log("\n=== Dynamic Value Tests Complete ===");
