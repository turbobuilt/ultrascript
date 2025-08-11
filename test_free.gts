// Simple test for the free statement
var x = [1, 2, 3];
var obj = new Object();

console.log("Testing free statement...");

// Deep free test
free x;

// Shallow free test  
free shallow obj;

console.log("Free statement test completed!");
