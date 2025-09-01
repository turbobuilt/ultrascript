var x = 42;
function outer() {
    var y = 21;
    function test() {
        console.log(x);
        console.log(y);
    }
    test();
}
outer();