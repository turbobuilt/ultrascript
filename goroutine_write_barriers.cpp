#include <iostream>
#include <mutex>
#include <atomic>
#include <memory>
#include <cstdlib>

// Simple write barriers without GC

class SimpleWriteBarrier {
public:
    static void write_ref(void* obj, void* field, void* new_value) {
        // Simple direct write, no barriers
        *reinterpret_cast<void**>(field) = new_value;
    }
    
    static void* read_ref(void* obj, void* field) {
        // Simple direct read
        return *reinterpret_cast<void**>(field);
    }
};

