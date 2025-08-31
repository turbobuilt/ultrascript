console.log(x); // should access hoisted function before declarations
var x = 5;
function x() {
    return "I'm a function";
}
var x = 10;
console.log(x); // should access variable after assignments
