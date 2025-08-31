function test() { 
    console.log(42);
    return 84;
} 

function caller() {
    return test;
}

let fn = caller();
// This should trigger the patching system since we're calling through a function instance
