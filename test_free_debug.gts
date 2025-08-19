// Debug test for free with console.log debugging
console.log("=== Free Debug Test ===");

var x: any = 42;
console.log("x =", x);
console.log("About to free x");

free shallow x;

console.log("Free completed successfully");
