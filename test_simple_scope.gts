console.log('Starting simple scope test');
var x = 5;
console.log("initial x is", x)

let result = go function() {
    console.log("goroutine: x is", x);
    x = 10;
    console.log("goroutine: x set to", x)
}

console.log("final x is", x)
console.log('Scope test complete');
