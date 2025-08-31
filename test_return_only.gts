function testReturn() {
    return 123;
}

function testAdd(a, b) {
    return a + b;
}

// Test without console.log to isolate the issue
let result1 = testReturn();
let result2 = testAdd(10, 20);
