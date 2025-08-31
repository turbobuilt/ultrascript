function outer() {
    console.log(x); // Should access x from outer scope (depth 2 accessing depth 1)
    function inner() {
        console.log(x); // Should access x from outer scope (depth 3 accessing depth 1) 
        console.log(y); // Should access y from inner scope (depth 3 accessing depth 2)
        var y = "inner var";
    }
    inner();
    var x = "outer var";
    function x() {  // Hoisting conflict with var x
        return "I'm a function";
    }
}
outer();
