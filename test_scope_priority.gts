function outer() {
    let x = 10;
    let y = 20;
    
    function middle() {
        let z = 30;
        
        function inner() {
            // Access x multiple times (should be higher priority)
            let a = x + x + x;
            // Access y once  
            let b = y;
            // Access z twice
            let c = z + z;
            return a + b + c;
        }
        
        return inner();
    }
    
    return middle();
}

let result = outer();
