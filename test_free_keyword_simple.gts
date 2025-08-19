// Simple test for free shallow keyword parsing and execution
function test() {
    var x = [1, 2, 3];
    var obj = new Object();
    var str = "Hello World";
    
    // Test shallow free - these should work
    free shallow x;
    free shallow obj;
    free shallow str;
    
    console.log("Free shallow tests completed successfully!");
}

test();
