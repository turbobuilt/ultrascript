// Test duplicate variable declaration error checking
var x: string = "hello";
var x: int64 = 42;  // This should trigger an error

console.log(x);
