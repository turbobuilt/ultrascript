var x = 42;

function test() {
    function test2() {
        console.log(x);
    }
    test2();
}

test();