class Person {
    name: string;
}

function main() {
    console.log("=== Testing reference count initialization ===");
    
    // Test 1: New object assignment - should be ref_count = 1
    let p = new Person();
    let ref_count = runtime.referenceCounter.getRefCount(p);
    console.log("Test 1 - New object ref_count:", ref_count, "(should be 1)");
    
    // Test 2: Assignment to another variable - should increment to 2
    let p2 = p;
    let ref_count_p = runtime.referenceCounter.getRefCount(p);
    let ref_count_p2 = runtime.referenceCounter.getRefCount(p2);
    console.log("Test 2a - Original ref_count:", ref_count_p, "(should be 2)");
    console.log("Test 2b - Copy ref_count:", ref_count_p2, "(should be 2)");
}
