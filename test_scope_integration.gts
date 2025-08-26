// Test lexical scope integration
var x = 5;

go function() {
    console.log(x); // This should trigger our scope address system
}();
