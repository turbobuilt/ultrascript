#include "refcount.h"
#include "refcount_asm.h"
#include "free_runtime.h"
#include <iostream>
#include <chrono>
#include <vector>
#include <thread>

// Disable debug output for clean test
#ifdef REFCOUNT_DEBUG
#undef REFCOUNT_DEBUG
#endif

#ifdef FREE_RUNTIME_DEBUG  
#undef FREE_RUNTIME_DEBUG
#endif

// Simple test object
struct TestObject {
    int value;
    TestObject* ref = nullptr;
    
    TestObject(int v) : value(v) {
        std::cout << "[TEST] TestObject " << value << " created" << std::endl;
    }
    
    ~TestObject() {
        std::cout << "[TEST] TestObject " << value << " destroyed" << std::endl;
    }
};

void test_basic_performance() {
    std::cout << "\n=== PERFORMANCE TEST (1M operations) ===" << std::endl;
    
    // Create test object
    void* obj = rc_alloc(sizeof(TestObject), 1, [](void* ptr) {
        static_cast<TestObject*>(ptr)->~TestObject();
    });
    new(obj) TestObject(1000);
    
    auto start = std::chrono::high_resolution_clock::now();
    
    // 1 million retain/release pairs
    for (int i = 0; i < 1000000; i++) {
        rc_retain(obj);
        rc_release(obj);
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    std::cout << "1M retain/release pairs took: " << duration.count() << " microseconds" << std::endl;
    std::cout << "Average time per operation: " << (double)duration.count() / 2000000.0 << " microseconds" << std::endl;
    
    rc_release(obj); // Final release
}

void test_threading() {
    std::cout << "\n=== THREAD SAFETY TEST ===" << std::endl;
    
    void* obj = rc_alloc(sizeof(TestObject), 2, [](void* ptr) {
        static_cast<TestObject*>(ptr)->~TestObject();
    });
    new(obj) TestObject(2000);
    
    const int num_threads = 8;
    const int ops_per_thread = 10000;
    
    std::vector<std::thread> threads;
    
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back([obj, ops_per_thread]() {
            for (int i = 0; i < ops_per_thread; i++) {
                rc_retain(obj);
                rc_release(obj);
            }
        });
    }
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    std::cout << "Multithreaded test (" << num_threads << " threads, " 
              << ops_per_thread << " ops each) took: " << duration.count() << " microseconds" << std::endl;
    
    rc_release(obj); // Final release
}

int main() {
    std::cout << "=== ULTRASCRIPT HIGH-PERFORMANCE REFERENCE COUNTING SYSTEM ===" << std::endl;
    
    test_basic_performance();
    test_threading();
    
    // Final statistics
    std::cout << "\n";
    rc_print_stats();
    
    std::cout << "\n=== PERFORMANCE TESTS COMPLETED ===\n" << std::endl;
    return 0;
}
