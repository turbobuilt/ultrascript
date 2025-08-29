var x = 42;

function test() {
    function test2() {
        console.log(x);
        test();
    }
    test2(); // Call test2 from within test - this is valid
}

test(); // Call test from global scope
