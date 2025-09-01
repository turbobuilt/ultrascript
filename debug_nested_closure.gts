function outer() {
    let x = 42;
    let y = 21;
    
    function test() {
        console.log(x);
        console.log(y);
    }
    
    test();
}

outer();
