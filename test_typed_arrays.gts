// Test typed array vs dynamic array detection

// This should create a typed int64 array
var typedArray: [int64] = [1, 2, 3];

// This should create a dynamic array
var dynamicArray = [1, 2, 3];

// Test float64 typed array
var floatArray: [float64] = [1.1, 2.2, 3.3];

// Test empty typed array
var emptyTyped: [int32] = [];

// Test empty dynamic array
var emptyDynamic = [];

console.log("Typed arrays test complete");
