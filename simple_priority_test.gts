// Simple test for priority verification
let a = 1; // depth 0
let b = 2; // depth 0

function test() { // depth 1
    let c = 3; // depth 1
    
    {  // depth 2
        // Access: a(3 times), b(1 time), c(2 times)  
        // Expected priority: depth 0 total=4, depth 1 total=2
        // So priority should be: 0 1
        console.log(a);
        console.log(a);
        console.log(a);
        console.log(b);
        console.log(c);
        console.log(c);
    }
}

test();
