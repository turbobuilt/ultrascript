function test() {
    console.log("Inside test function");
    
    function test2() {
        console.log("Inside test2 function");
    }
    
    // This should work with function hoisting
    test2();
}

test();
