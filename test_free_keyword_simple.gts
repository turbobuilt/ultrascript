// Simple test for free keyword parsing
function test() {
    var x = [1, 2, 3];
    var obj = new Object();
    
    // Test regular free (deep)
    free x;
    
    // Test shallow free
    free shallow obj;
    
    console.log("Free keyword test completed!");
}

test();
