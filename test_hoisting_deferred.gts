console.log(x); // should work now with deferred resolution
var x = 5;
function x() {
    console.log("This is function x");
}
var x = 10;
console.log(x); // should also work
