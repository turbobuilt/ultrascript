// Test file to demonstrate the Conservative Maximum Size approach
// This tests variable-size function assignments where the same variable
// gets assigned functions with different closure sizes

var f;  // Variable that will receive different function sizes

function outer() {
    var x = 10;
    
    // Assignment #1: Simple function (no closures) = 16 bytes
    f = function simple() { 
        return 42; 
    };
    
    var y = 20;
    
    function middle() {
        var z = 30;
        
        // Assignment #2: Function with 1 closure (outer) = 24 bytes
        f = function withOneClosure() { 
            return x + 100; 
        };
        
        function inner() {
            var w = 40;
            
            // Assignment #3: Function with 3 closures (outer, middle, inner) = 40 bytes
            f = function deepInner() { 
                return x + y + z + w + 200; 
            };
        }
        inner();
    }
    middle();
}

outer();

// f should be allocated with maximum size (40 bytes) from the beginning
// to support all possible assignments
