function outer() {
    let x = 10;
    let y = 20;
    
    function middle() {
        let z = 30;
        
        function inner() {
            // Access variables different numbers of times
            let a = x;    // Access x once (depth 1)
            let b = y + y + y;  // Access y three times (depth 1) 
            let c = z + z;      // Access z twice (depth 2)
            return a + b + c;
        }
        
        return inner();
    }
    
    return middle();
}

let result = outer();
