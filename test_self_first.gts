// Test SELF-first priority sorting
let a = 1; // depth 0  
let b = 2; // depth 0

function outer() { // depth 1
    let c = 3; // depth 1
    let d = 4; // depth 1
    
    function inner() { // depth 2  
        let e = 5; // depth 2
        
        // SELF accesses in inner():
        // a: 3 times (depth 0)
        // c: 2 times (depth 1) 
        // e: 1 time (depth 2)
        // Expected SELF priority: 0, 1, 2 (by access count)
        console.log(a); console.log(a); console.log(a);  // 3x from depth 0
        console.log(c); console.log(c);                  // 2x from depth 1
        console.log(e);                                  // 1x from depth 2
        
        {  // depth 3
            // This will create descendants that inner() doesn't access directly
            console.log(b); // b from depth 0 - descendant only
            console.log(d); // d from depth 1 - descendant only
        }
        // Expected for inner(): SELF [0,1,2] + descendants-only [no new depths since b,d are already covered by 0,1]
    }
    
    inner();
}

outer();
