// Test direct scope pointer optimization
function testDirectScopeAccess() {
    let x = 100;
    
    if (true) {
        let y = 200;
        x = y + 50;  // This should show cross-scope access with direct pointers
    }
    
    return x;
}

let result = testDirectScopeAccess();
console.log("Result:", result);
console.log("Direct scope pointer test completed");
