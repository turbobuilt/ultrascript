// Test JavaScript function hoisting conflicts
console.log(x); // Should print function (if hoisting works correctly)
var x = 5;
function x() {
    return 42;
}
var x = 10;
console.log(x); // Should print 10
