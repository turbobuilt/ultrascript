class Person {
    name: string;
}

function main() {
    console.log("=== Reference Count Verification ===");
    
    // Test 1: New object should have ref_count = 1
    let p1 = new Person();
    let count1 = runtime.referenceCounter.getRefCount(p1);
    console.log("Test 1 - New object ref_count:", count1, "(should be 1)");
    
    // Test 2: Copy assignment should increment to ref_count = 2
    let p2 = p1;
    let count1_after = runtime.referenceCounter.getRefCount(p1);
    let count2 = runtime.referenceCounter.getRefCount(p2);
    console.log("Test 2a - Original after copy:", count1_after, "(should be 2)");
    console.log("Test 2b - Copy ref_count:", count2, "(should be 2)");
    
    // Test 3: Another copy should increment to ref_count = 3
    let p3 = p1;
    let count1_final = runtime.referenceCounter.getRefCount(p1);
    let count3 = runtime.referenceCounter.getRefCount(p3);
    console.log("Test 3a - Original after second copy:", count1_final, "(should be 3)");
    console.log("Test 3b - Second copy ref_count:", count3, "(should be 3)");
    
    console.log("=== All tests completed ===");
}
