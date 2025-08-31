function test() { 
    console.log(42);
} 

function callFunction(fn) {
    fn();
}

callFunction(test);
