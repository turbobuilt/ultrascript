// Test to verify the reference counting fix
// Issue: new objects were incorrectly starting with ref_count = 2 instead of 1

class TestClass {
    value: int64;
}

function main() {
    console.log("Reference Count Fix Verification Test");
    console.log("=====================================");
    
    // FIXED: New object assignment uses transfer semantics (ref_count = 1)
    let obj = new TestClass();
    let count_after_creation = runtime.referenceCounter.getRefCount(obj);
    
    if (count_after_creation == 1) {
        console.log("✓ PASS: New object has correct ref_count = 1");
    } else {
        console.log("✗ FAIL: New object has ref_count =", count_after_creation, "(expected 1)");
    }
    
    // Copy assignment should use copy semantics (ref_count = 2)
    let obj2 = obj;
    let count_after_copy = runtime.referenceCounter.getRefCount(obj);
    
    if (count_after_copy == 2) {
        console.log("✓ PASS: Copy assignment correctly increments ref_count to 2");
    } else {
        console.log("✗ FAIL: Copy assignment has ref_count =", count_after_copy, "(expected 2)");
    }
    
    console.log("Test completed successfully!");
}
