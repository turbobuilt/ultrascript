#include <iostream>
#include <vector>
#include <thread>
#include <chrono>
#include "refcount.h"
#include "free_runtime.h"

// ============================================================================
// REFERENCE COUNTING SYSTEM TEST - COMPREHENSIVE VALIDATION
// ============================================================================

// Test object for reference counting
struct TestObject {
    int value;
    std::vector<int> data;
    
    TestObject(int v) : value(v), data(100, v) {
        std::cout << "[TEST] TestObject " << value << " created" << std::endl;
    }
    
    ~TestObject() {
        std::cout << "[TEST] TestObject " << value << " destroyed" << std::endl;
    }
};

// Custom destructor for TestObject
void test_object_destructor(void* ptr) {
    if (ptr) {
        TestObject* obj = static_cast<TestObject*>(ptr);
        obj->~TestObject();
    }
}

// Test basic reference counting operations
void test_basic_refcount() {
    std::cout << "\n=== BASIC REFERENCE COUNTING TEST ===" << std::endl;
    
    // Test 1: Basic allocation and release
    {
        void* obj = rc_alloc(sizeof(TestObject), 100, test_object_destructor);
        new (obj) TestObject(42);
        
        std::cout << "Initial ref count: " << rc_get_count(obj) << std::endl;
        
        void* obj2 = rc_retain(obj);
        std::cout << "After retain: " << rc_get_count(obj) << std::endl;
        
        rc_release(obj2);
        std::cout << "After first release: " << rc_get_count(obj) << std::endl;
        
        rc_release(obj);
        std::cout << "After final release (object should be destroyed)" << std::endl;
    }
    
    // Test 2: Array allocation
    {
        std::cout << "\n--- Array Reference Counting ---" << std::endl;
        void* arr = rc_alloc_array(sizeof(int), 10, 101, rc_destructor_array);
        
        // Initialize array
        int* int_arr = static_cast<int*>(arr);
        for (int i = 0; i < 10; ++i) {
            int_arr[i] = i * i;
        }
        
        std::cout << "Array ref count: " << rc_get_count(arr) << std::endl;
        std::cout << "Array[5] = " << int_arr[5] << std::endl;
        
        rc_release(arr);
    }
}

// Test weak references
#if REFCOUNT_WEAK_REFS
void test_weak_references() {
    std::cout << "\n=== WEAK REFERENCE TEST ===" << std::endl;
    
    void* obj = rc_alloc(sizeof(TestObject), 102, test_object_destructor);
    new (obj) TestObject(100);
    
    // Create weak reference
    void* weak_ref = rc_weak_retain(obj);
    std::cout << "Created weak reference" << std::endl;
    
    // Check if weak reference is valid
    std::cout << "Weak reference expired: " << rc_weak_expired(weak_ref) << std::endl;
    
    // Try to lock weak reference
    void* strong_ref = rc_weak_lock(weak_ref);
    if (strong_ref) {
        std::cout << "Successfully locked weak reference" << std::endl;
        std::cout << "Strong ref count: " << rc_get_count(strong_ref) << std::endl;
        rc_release(strong_ref);
    }
    
    // Release original object
    rc_release(obj);
    
    // Check if weak reference is now expired
    std::cout << "Weak reference expired after release: " << rc_weak_expired(weak_ref) << std::endl;
    
    // Try to lock expired weak reference
    void* expired_lock = rc_weak_lock(weak_ref);
    if (!expired_lock) {
        std::cout << "Correctly failed to lock expired weak reference" << std::endl;
    }
    
    rc_weak_release(weak_ref);
}
#endif

// Test cycle breaking (free shallow functionality)
void test_cycle_breaking() {
    std::cout << "\n=== CYCLE BREAKING TEST (FREE SHALLOW) ===" << std::endl;
    
    // Create objects that could form a cycle
    void* obj1 = rc_alloc(sizeof(TestObject), 103, test_object_destructor);
    void* obj2 = rc_alloc(sizeof(TestObject), 103, test_object_destructor);
    
    new (obj1) TestObject(200);
    new (obj2) TestObject(201);
    
    // Simulate cycle by retaining each other
    rc_retain(obj1);  // obj2 "holds" obj1
    rc_retain(obj2);  // obj1 "holds" obj2
    
    std::cout << "Created potential cycle:" << std::endl;
    std::cout << "Object 1 ref count: " << rc_get_count(obj1) << std::endl;
    std::cout << "Object 2 ref count: " << rc_get_count(obj2) << std::endl;
    
    // Use free shallow (cycle breaking) on one object
    std::cout << "Breaking cycles with rc_break_cycles..." << std::endl;
    rc_break_cycles(obj1);
    
    // Release the other object normally
    rc_release(obj2);
    rc_release(obj2);  // Second release for the "cycle" reference
}

// Test performance with many operations
void test_performance() {
    std::cout << "\n=== PERFORMANCE TEST ===" << std::endl;
    
    const int NUM_OBJECTS = 10000;
    std::vector<void*> objects;
    objects.reserve(NUM_OBJECTS);
    
    auto start = std::chrono::high_resolution_clock::now();
    
    // Allocate many objects
    for (int i = 0; i < NUM_OBJECTS; ++i) {
        void* obj = rc_alloc(sizeof(int), 104, nullptr);
        *static_cast<int*>(obj) = i;
        objects.push_back(obj);
    }
    
    // Retain and release operations
    for (void* obj : objects) {
        rc_retain(obj);
        rc_release(obj);
    }
    
    // Final cleanup
    for (void* obj : objects) {
        rc_release(obj);
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    std::cout << "Processed " << NUM_OBJECTS << " objects in " 
              << duration.count() << " microseconds" << std::endl;
    std::cout << "Average per operation: " 
              << (duration.count() / (NUM_OBJECTS * 3.0)) << " microseconds" << std::endl;
}

// Test thread safety
void test_thread_safety() {
    std::cout << "\n=== THREAD SAFETY TEST ===" << std::endl;
    
    void* shared_obj = rc_alloc(sizeof(TestObject), 105, test_object_destructor);
    new (shared_obj) TestObject(300);
    
    const int NUM_THREADS = 4;
    const int OPS_PER_THREAD = 1000;
    
    std::vector<std::thread> threads;
    
    for (int t = 0; t < NUM_THREADS; ++t) {
        threads.emplace_back([shared_obj, OPS_PER_THREAD]() {
            for (int i = 0; i < OPS_PER_THREAD; ++i) {
                rc_retain(shared_obj);
                std::this_thread::sleep_for(std::chrono::nanoseconds(1));
                rc_release(shared_obj);
            }
        });
    }
    
    // Wait for all threads
    for (auto& thread : threads) {
        thread.join();
    }
    
    std::cout << "Final ref count: " << rc_get_count(shared_obj) << std::endl;
    
    // Clean up
    rc_release(shared_obj);
}

// Test integration with free runtime
void test_free_integration() {
    std::cout << "\n=== FREE RUNTIME INTEGRATION TEST ===" << std::endl;
    
    // Initialize migration
    __migrate_to_rc_alloc();
    
    // Test reference counted object with free shallow
    void* obj = rc_alloc(sizeof(TestObject), 106, test_object_destructor);
    new (obj) TestObject(400);
    
    std::cout << "Testing free shallow integration..." << std::endl;
    rc_integrate_with_free_shallow(obj);
    
    // Test array
    void* arr = rc_alloc_array(sizeof(int), 5, 107, rc_destructor_array);
    int* int_arr = static_cast<int*>(arr);
    for (int i = 0; i < 5; ++i) {
        int_arr[i] = i * 10;
    }
    
    std::cout << "Testing array free integration..." << std::endl;
    __free_array_shallow(arr);
}

// Test C++ template interface
void test_cpp_interface() {
    std::cout << "\n=== C++ TEMPLATE INTERFACE TEST ===" << std::endl;
    
    // Test RefPtr with TestObject
    {
        auto obj1 = make_ref<TestObject>(500);
        std::cout << "RefPtr created, use count: " << obj1.use_count() << std::endl;
        
        {
            auto obj2 = obj1;  // Copy
            std::cout << "After copy, use count: " << obj1.use_count() << std::endl;
            
            auto obj3 = std::move(obj1);  // Move
            std::cout << "After move, obj3 use count: " << obj3.use_count() << std::endl;
            std::cout << "obj1 is null: " << (!obj1) << std::endl;
            
            std::cout << "TestObject value: " << obj3->value << std::endl;
        }
        
        std::cout << "After scope exit, should be destroyed" << std::endl;
    }
}

int main() {
    std::cout << "=== ULTRASCRIPT REFERENCE COUNTING SYSTEM TEST ===" << std::endl;
    
    // Set debug mode for detailed output
    rc_set_debug_mode(1);
    
    try {
        test_basic_refcount();
        
        #if REFCOUNT_WEAK_REFS
        test_weak_references();
        #endif
        
        test_cycle_breaking();
        test_performance();
        test_thread_safety();
        test_free_integration();
        test_cpp_interface();
        
        // Print final statistics
        std::cout << "\n=== FINAL STATISTICS ===" << std::endl;
        rc_print_stats();
        __print_free_stats();
        
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
    
    std::cout << "\n=== ALL TESTS COMPLETED SUCCESSFULLY ===" << std::endl;
    return 0;
}
