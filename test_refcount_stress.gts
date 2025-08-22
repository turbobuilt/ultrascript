// Stress test for reference counting performance and memory leak detection

class StressTestObject {
    id: int64;
    data: string;
    
    constructor(id: int64) {
        this.id = id;
        this.data = "stress_test_data_" + id;
    }
    
    destructor() {
        // Note: In stress test, we don't log every destructor to avoid spam
        // But the destructor should still be called
    }
}

function stress_test_basic_assignments() {
    console.log("=== Stress Test 1: Basic Assignments (10,000 objects) ===");
    
    let start_time = Date.now();
    let objects = [];
    
    // Create 10,000 objects
    for (let i = 0; i < 10000; i++) {
        let obj = new StressTestObject(i);
        objects.push(obj);
    }
    
    console.log("Created 10,000 objects");
    
    // Create cross-references (more ref counting operations)
    for (let i = 0; i < 5000; i++) {
        objects[i].ref = objects[i + 5000];  // Should increment ref_count
    }
    
    console.log("Created 5,000 cross-references");
    
    // Clear all references
    objects = [];
    
    let end_time = Date.now();
    console.log("Stress test 1 completed in:", (end_time - start_time), "ms");
    console.log("All 10,000 objects should be destroyed\n");
}

function stress_test_rapid_reassignments() {
    console.log("=== Stress Test 2: Rapid Reassignments (100,000 operations) ===");
    
    let start_time = Date.now();
    let obj = new StressTestObject(0);
    
    // Rapidly reassign the same variable 100,000 times
    for (let i = 1; i < 100000; i++) {
        obj = new StressTestObject(i);  // Should destroy previous, create new
    }
    
    obj = null;  // Destroy final object
    
    let end_time = Date.now();
    console.log("Stress test 2 completed in:", (end_time - start_time), "ms");
    console.log("100,000 objects created and destroyed\n");
}

function stress_test_deep_nesting() {
    console.log("=== Stress Test 3: Deep Object Nesting (1,000 levels) ===");
    
    let start_time = Date.now();
    
    // Create deeply nested structure
    let root = new StressTestObject(0);
    let current = root;
    
    for (let i = 1; i < 1000; i++) {
        let next = new StressTestObject(i);
        current.next = next;  // Should increment ref_count
        current = next;
    }
    
    console.log("Created 1,000 level deep nesting");
    
    // Clear root reference - should trigger cascading destruction
    root = null;
    
    let end_time = Date.now();
    console.log("Stress test 3 completed in:", (end_time - start_time), "ms");
    console.log("All 1,000 nested objects should be destroyed\n");
}

function stress_test_circular_references() {
    console.log("=== Stress Test 4: Circular Reference Patterns ===");
    
    let start_time = Date.now();
    let objects = [];
    
    // Create 1,000 objects with circular references
    for (let i = 0; i < 1000; i++) {
        objects.push(new StressTestObject(i));
    }
    
    // Create circular reference pattern
    for (let i = 0; i < 1000; i++) {
        let next_index = (i + 1) % 1000;
        objects[i].next = objects[next_index];  // Creates one big cycle
    }
    
    console.log("Created 1,000 objects in circular reference pattern");
    
    // Clear array - objects should still exist due to cycles
    objects = [];
    
    console.log("Cleared array - objects still exist due to cycles");
    console.log("Note: Manual cycle breaking with 'free shallow' would be needed");
    
    let end_time = Date.now();
    console.log("Stress test 4 completed in:", (end_time - start_time), "ms\n");
}

function stress_test_concurrent_simulation() {
    console.log("=== Stress Test 5: Concurrent-Style Access Simulation ===");
    
    let start_time = Date.now();
    let shared_objects = [];
    
    // Create shared objects
    for (let i = 0; i < 100; i++) {
        shared_objects.push(new StressTestObject(i));
    }
    
    // Simulate multiple "threads" accessing objects
    // (This is single-threaded but simulates the access patterns)
    for (let thread = 0; thread < 10; thread++) {
        for (let op = 0; op < 1000; op++) {
            let index = op % 100;
            let temp = shared_objects[index];  // Should increment ref_count
            temp = null;  // Should decrement ref_count
        }
    }
    
    console.log("Simulated 10,000 concurrent-style operations");
    
    // Clear shared objects
    shared_objects = [];
    
    let end_time = Date.now();
    console.log("Stress test 5 completed in:", (end_time - start_time), "ms");
    console.log("All shared objects should be destroyed\n");
}

function performance_benchmark() {
    console.log("=== Performance Benchmark ===");
    
    let start_time = Date.now();
    
    // Benchmark: 1 million reference counting operations
    let obj = new StressTestObject(0);
    let temp;
    
    for (let i = 0; i < 1000000; i++) {
        temp = obj;   // Should increment ref_count
        temp = null;  // Should decrement ref_count
    }
    
    obj = null;  // Final cleanup
    
    let end_time = Date.now();
    let duration = end_time - start_time;
    
    console.log("1 million ref count operations completed in:", duration, "ms");
    console.log("Average per operation:", (duration * 1000000 / 1000000), "nanoseconds");
    
    // Target: under 20 nanoseconds per operation for ultra-fast atomic operations
    if (duration > 20) {
        console.log("⚠️  Performance warning: operations slower than 20ns target");
    } else {
        console.log("✅ Performance excellent: under 20ns per operation");
    }
    
    console.log("");
}

function main() {
    console.log("==========================================");
    console.log("UltraScript Reference Counting Stress Test");
    console.log("==========================================\n");
    
    let total_start = Date.now();
    
    stress_test_basic_assignments();
    stress_test_rapid_reassignments();
    stress_test_deep_nesting();
    stress_test_circular_references();
    stress_test_concurrent_simulation();
    performance_benchmark();
    
    let total_end = Date.now();
    
    console.log("==========================================");
    console.log("All stress tests completed in:", (total_end - total_start), "ms");
    console.log("==========================================");
    
    // Debug mode should report:
    // - Total objects created vs destroyed
    // - Any leaked objects (should be 0 except for circular references)
    // - Performance statistics
    
    return 0;
}

main();
