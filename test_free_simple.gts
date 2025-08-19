// Simple test for free shallow keyword - basic types only
function test() {
    var x = [1, 2, 3];
    var str = "Hello World";
    
    // Test shallow free - these should work
    free shallow x;
    free shallow str;
    
    console.log("Free shallow tests completed successfully!");
}

test();
