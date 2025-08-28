// Clear test for SELF-first priority 
let a = 1; // depth 0
let b = 2; // depth 0

function test() { // depth 1
    let c = 3; // depth 1
    let d = 4; // depth 1
    
    function inner() { // depth 2
        // SELF accesses: b(5x from depth 0), c(1x from depth 1)
        // Expected SELF priority: 0, 1 (b accessed 5 times beats c accessed 1 time)
        console.log(b); console.log(b); console.log(b); console.log(b); console.log(b); // 5x from depth 0
        console.log(c); // 1x from depth 1
        
        {  // depth 3
            // Descendant accesses: a(10x from depth 0), d(1x from depth 1) 
            // But these are descendants only, should come AFTER self
            console.log(a); console.log(a); console.log(a); console.log(a); console.log(a);
            console.log(a); console.log(a); console.log(a); console.log(a); console.log(a); // 10x from depth 0
            console.log(d); // 1x from depth 1
        }
        
        // Expected for inner(): 
        // SELF first (priority sorted): [0, 1] (b:5 from depth 0, c:1 from depth 1)
        // Descendants second (any order): no new depths since a,d are already covered by depths 0,1
        // Final result should be: [0, 1]
    }
    
    inner();
}

test();
