#include "runtime.h"
#include "compiler.h"
#include "lexical_scope.h"
#include "regex.h"
#include "goroutine_system.h"
#include "ultra_performance_array.h"
#include <iostream>
#include <algorithm>
#include <chrono>
#include <unordered_map>
#include <pthread.h>
#include <signal.h>
#include <setjmp.h>
#include <cmath>
#include <regex>
#include <cstring>

// Forward declarations for new goroutine system
extern "C" {
    int64_t __gots_set_timeout(void* callback, int64_t delay_ms);
    int64_t __gots_set_interval(void* callback, int64_t delay_ms);  
    bool __gots_clear_timeout(int64_t timer_id);
    bool __gots_clear_interval(int64_t timer_id);
    void __new_goroutine_system_init();
    void __new_goroutine_system_cleanup();
    void __new_goroutine_spawn(void* func_ptr);
}

// External function declarations
extern "C" void* __lookup_function_by_id(int64_t function_id);

// Global function ID to pointer map
static std::unordered_map<int64_t, void*> g_function_id_map;
static std::mutex g_function_id_mutex;
static std::atomic<int64_t> g_next_function_id{1};

// Global console output mutex for thread safety
std::mutex g_console_mutex;

namespace ultraScript {

// Global instances
// Using pointers to control initialization/destruction order
static GoroutineScheduler* global_scheduler = nullptr;
static std::mutex scheduler_mutex;

// Timer globals moved to goroutine_system.cpp to avoid duplicates

ThreadPool::ThreadPool(size_t num_threads) {
    // Use the full number of available hardware threads for maximum performance
    // This is essential for proper goroutine parallelism
    size_t optimal_thread_count = (num_threads > 0) ? num_threads : std::thread::hardware_concurrency();
    
    for (size_t i = 0; i < optimal_thread_count; ++i) {
        workers.emplace_back([this] {
            while (true) {
                std::function<void()> task;
                
                {
                    std::unique_lock<std::mutex> lock(queue_mutex);
                    condition.wait(lock, [this] { return stop.load() || !tasks.empty(); });
                    
                    if (stop.load() && tasks.empty()) {
                        return;
                    }
                    
                    if (!tasks.empty()) {
                        task = std::move(tasks.front());
                        tasks.pop();
                    }
                }
                
                if (task) {
                    try {
                        task();
                    } catch (const std::exception& e) {
                        std::cerr << "Worker task failed: " << e.what() << std::endl;
                    } catch (...) {
                        std::cerr << "Worker task failed with unknown exception" << std::endl;
                    }
                }
            }
        });
    }
}

ThreadPool::~ThreadPool() {
    shutdown();
}

void ThreadPool::shutdown() {
    stop.store(true);
    condition.notify_all();
    
    for (std::thread& worker : workers) {
        if (worker.joinable()) {
            worker.join();
        }
    }
}

void ThreadPool::enqueue_simple(std::function<void()> task) {
    {
        std::unique_lock<std::mutex> lock(queue_mutex);
        
        if (stop.load()) {
            return; // Don't enqueue if stopped
        }
        
        tasks.push(task);
    }
    
    condition.notify_one();
}

// Old GoroutineScheduler implementations removed - using new system

// Global object registry
std::unordered_map<int64_t, std::unique_ptr<ObjectInstance>> object_registry;
std::atomic<int64_t> next_object_id{1};

// High-Performance Function Registry Implementation
FunctionEntry g_function_table[MAX_FUNCTIONS];
std::atomic<uint16_t> g_next_function_id{1};  // Start at 1, 0 is reserved for "invalid"

// Global promise registry for cleanup
static std::unordered_set<void*> g_allocated_promises;
static std::mutex g_promise_registry_mutex;

// Helper function to create and track a promise
static void* create_tracked_promise(std::shared_ptr<Promise> promise) {
    auto* promise_ptr = new std::shared_ptr<Promise>(promise);
    
    // Track allocated promise for cleanup
    {
        std::lock_guard<std::mutex> lock(g_promise_registry_mutex);
        g_allocated_promises.insert(promise_ptr);
    }
    
    return promise_ptr;
}

// Global executable memory info for thread-safe access
ExecutableMemoryInfo g_executable_memory = {nullptr, 0, {}};

extern "C" {

// Function ID registration and lookup
void __register_function_id(int64_t function_id, void* function_ptr) {
    std::lock_guard<std::mutex> lock(g_function_id_mutex);
    g_function_id_map[function_id] = function_ptr;
}

// __lookup_function_by_id is now defined in ast_codegen.cpp to avoid duplicate symbol

int64_t __allocate_function_id() {
    return g_next_function_id.fetch_add(1);
}


// High-Performance Function Registration - O(1) access
uint16_t __register_function_fast(void* func_ptr, uint16_t arg_count, uint8_t calling_convention) {
    uint16_t func_id = g_next_function_id.fetch_add(1);
    
    if (func_id >= MAX_FUNCTIONS) {
        std::cerr << "ERROR: Function table overflow! Maximum " << MAX_FUNCTIONS << " functions supported." << std::endl;
        return 0;  // Return invalid ID
    }
    
    FunctionEntry& entry = g_function_table[func_id];
    entry.func_ptr = func_ptr;
    entry.arg_count = arg_count;
    entry.calling_convention = calling_convention;
    entry.flags = 0;
    
    return func_id;
}

void* __lookup_function_fast(uint16_t func_id) {
    if (func_id == 0 || func_id >= g_next_function_id.load()) {
        return nullptr;  // Invalid function ID
    }
    
    return g_function_table[func_id].func_ptr;
}

// Initialize the new goroutine system
void __runtime_init() {
    // Initialize function table
    for (size_t i = 0; i < MAX_FUNCTIONS; i++) {
        g_function_table[i].func_ptr = nullptr;
    }
    __new_goroutine_system_init();
}

// Main cleanup - wait for all goroutines
void __runtime_cleanup() {
    __new_goroutine_system_cleanup();
}

// Main goroutine functions moved to goroutine_system.cpp

// Optimized goroutine spawn with direct function IDs - NO string lookups
void* __goroutine_spawn_fast(uint16_t func_id) {
    void* func_ptr = __lookup_function_fast(func_id);
    if (!func_ptr) {
        std::cerr << "ERROR: Invalid function ID: " << func_id << std::endl;
        return nullptr;
    }
    
    // Create a task that calls the function directly - minimal overhead
    auto task = [func_ptr]() {
        typedef int64_t (*FuncType)();
        FuncType func = reinterpret_cast<FuncType>(func_ptr);
        return func();
    };
    
    GoroutineScheduler::instance().spawn(task);
    return reinterpret_cast<void*>(1);
}

void* __goroutine_spawn_fast_arg1(uint16_t func_id, int64_t arg1) {
    void* func_ptr = __lookup_function_fast(func_id);
    if (!func_ptr) {
        std::cerr << "ERROR: Invalid function ID: " << func_id << std::endl;
        return nullptr;
    }
    
    // Create a task that calls the function with one argument - minimal overhead
    auto task = [func_ptr, arg1]() {
        typedef int64_t (*FuncType)(int64_t);
        FuncType func = reinterpret_cast<FuncType>(func_ptr);
        return func(arg1);
    };
    
    GoroutineScheduler::instance().spawn(task);
    return reinterpret_cast<void*>(1);
}

void* __goroutine_spawn_fast_arg2(uint16_t func_id, int64_t arg1, int64_t arg2) {
    void* func_ptr = __lookup_function_fast(func_id);
    if (!func_ptr) {
        std::cerr << "ERROR: Invalid function ID: " << func_id << std::endl;
        return nullptr;
    }
    
    // Create a task that calls the function with two arguments - minimal overhead
    auto task = [func_ptr, arg1, arg2]() {
        typedef int64_t (*FuncType)(int64_t, int64_t);
        FuncType func = reinterpret_cast<FuncType>(func_ptr);
        return func(arg1, arg2);
    };
    
    GoroutineScheduler::instance().spawn(task);
    return reinterpret_cast<void*>(1);
}

// Proper array creation using UltraScript DynamicArray
void* __array_create(int64_t size) {
    DynamicArray* array = new DynamicArray();
    
    // Initialize with zeros if size > 0
    if (size > 0) {
        for (int64_t i = 0; i < size; i++) {
            array->push(0.0);
        }
    }
    
    return array;
}

// Array push implementation for DynamicArray
void __array_push(void* array, int64_t value) {
    if (!array) return;
    
    DynamicArray* arr = static_cast<DynamicArray*>(array);
    
    // Convert int64_t bits to double for unified storage
    union {
        int64_t i;
        double d;
    } converter;
    converter.i = value;
    
    arr->push(converter.d);
}

// Array pop implementation
int64_t __array_pop(void* array) {
    if (!array) return 0;
    
    DynamicArray* arr = static_cast<DynamicArray*>(array);
    if (arr->empty()) return 0;
    
    DynamicValue val = arr->pop();
    double d = val.to_number();
    
    // Convert back to int64_t bits
    union {
        int64_t i;
        double d;
    } converter;
    converter.d = d;
    
    return converter.i;
}

// Array size
int64_t __array_size(void* array) {
    if (!array) return 0;
    
    DynamicArray* arr = static_cast<DynamicArray*>(array);
    return static_cast<int64_t>(arr->size());
}

// Array access
int64_t __array_access(void* array, int64_t index) {
    if (!array) return 0;
    
    DynamicArray* arr = static_cast<DynamicArray*>(array);
    if (index < 0 || index >= static_cast<int64_t>(arr->size())) return 0;
    
    double d = (*arr)[index].to_number();
    
    // Convert to int64_t bits
    union {
        int64_t i;
        double d;
    } converter;
    converter.d = d;
    
    return converter.i;
}

// Typed array access functions for maximum performance
extern "C" int64_t __array_access_int64(void* array, int64_t index) {
    if (!array) return 0;
    Int64Array* arr = static_cast<Int64Array*>(array);
    if (index < 0 || index >= static_cast<int64_t>(arr->size())) return 0;
    return (*arr)[index];
}

extern "C" int64_t __array_access_float64(void* array, int64_t index) {
    if (!array) return 0;
    Float64Array* arr = static_cast<Float64Array*>(array);
    if (index < 0 || index >= static_cast<int64_t>(arr->size())) return 0;
    
    double value = (*arr)[index];
    // Convert double to int64_t bit pattern for return
    union {
        int64_t i;
        double d;
    } converter;
    converter.d = value;
    return converter.i;
}

extern "C" int64_t __array_access_int32(void* array, int64_t index) {
    if (!array) return 0;
    Int32Array* arr = static_cast<Int32Array*>(array);
    if (index < 0 || index >= static_cast<int64_t>(arr->size())) return 0;
    return static_cast<int64_t>((*arr)[index]);
}

extern "C" int64_t __array_access_float32(void* array, int64_t index) {
    if (!array) return 0;
    Float32Array* arr = static_cast<Float32Array*>(array);
    if (index < 0 || index >= static_cast<int64_t>(arr->size())) return 0;
    
    float value = (*arr)[index];
    // Convert float to double, then to int64_t bit pattern
    union {
        int64_t i;
        double d;
    } converter;
    converter.d = static_cast<double>(value);
    return converter.i;
}
}

// Array raw data access (returns pointer to first element)
int64_t* __array_data(void* array) {
    if (!array) return nullptr;
    
    // For DynamicArray, we can't return raw int64_t* since it stores DynamicValue
    // This is a compatibility function that may not be fully functional
    // Consider using array access functions instead
    return nullptr;
}

// Missing utility functions
void __set_executable_memory(void* memory, size_t size) {
    // Set the global executable memory pointer
    std::lock_guard<std::mutex> lock(g_executable_memory.mutex);
    g_executable_memory.ptr = memory;
    g_executable_memory.size = size;
}

void __console_log(const char* message) {
    std::cout << message;
}

void __console_log_newline() {
    std::cout << std::endl;
}

void __console_log_space() {
    std::cout << " ";
}

void __console_log_number(int64_t value) {
    std::lock_guard<std::mutex> lock(g_console_mutex);
    std::cout << value;
    std::cout.flush();
}

void __console_log_double_bits(int64_t double_bits) {
    // Convert int64 bit pattern back to double for proper display
    union {
        double d;
        int64_t i;
    } converter;
    converter.i = double_bits;
    
    std::lock_guard<std::mutex> lock(g_console_mutex);
    std::cout << converter.d;
    std::cout.flush();
}

extern "C" void __console_log_universal(int64_t value) {
    std::lock_guard<std::mutex> lock(g_console_mutex);
    
    // Simple, safe implementation without recursion
    if (value == 0) {
        std::cout << "null";
        std::cout.flush();
        return;
    }
    
    // Check if this looks like a pointer (rough heuristic)
    if (value > 0x100000 && value < 0x7fffffffffff) {
        // For safety, just treat all pointers as generic objects for now
        // TODO: Implement proper type tagging system
        std::cout << "Object@" << std::hex << value << std::dec;
        std::cout.flush();
        return;
    }
    
    // Treat as number - try double bit pattern first
    union {
        double d;
        int64_t i;
    } converter;
    converter.i = value;
    
    // Check if this looks like a reasonable double
    if (std::isfinite(converter.d) && std::abs(converter.d) < 1e15) {
        std::cout << converter.d;
    } else {
        // Treat as integer
        std::cout << value;
    }
    
    std::cout.flush();
}

void __console_log_auto(int64_t value) {
    // Check if it's a likely heap pointer (string or object)
    if (value > 0x100000) {  // Likely a heap pointer
        // Try to safely read the string by using the __console_log_string function
        // This way we use the existing safe string handling
        void* ptr = reinterpret_cast<void*>(value);
        
        // Try to call the string logger directly and see if it works
        try {
            __console_log_string(ptr);
            return;
        } catch (...) {
            // String printing failed, try other types
        }
        
        // Check if it might be an object ID
        if (object_registry.find(value) != object_registry.end()) {
            __console_log_object(value);
            return;
        }
    }
    
    // Default: treat as number
    std::cout << value;
}

void __console_log_string(void* string_ptr) {
    std::lock_guard<std::mutex> lock(g_console_mutex);
    if (string_ptr) {
        // Handle basic char* strings
        const char* str = static_cast<const char*>(string_ptr);
        std::cout << str;
        std::cout.flush();
    }
}

void __console_log_object(int64_t object_id) {
    auto it = object_registry.find(object_id);
    if (it != object_registry.end()) {
        std::cout << "[object Object]";
    } else {
        std::cout << "Object#" << object_id;
    }
}

// Helper function to extract C string from GoTSString pointer
const char* __gots_string_to_cstr(void* gots_string_ptr) {
    if (!gots_string_ptr) {
        return "";
    }
    GoTSString* str = static_cast<GoTSString*>(gots_string_ptr);
    return str->c_str();
}

// Stub function for unimplemented runtime functions
void __runtime_stub_function() {
    // Do nothing - just return
}

// Forward declarations for timer functions
extern "C" int64_t __gots_set_timeout(void* callback, int64_t delay_ms);
extern "C" int64_t __gots_set_interval(void* callback, int64_t delay_ms);  
extern "C" bool __gots_clear_timeout(int64_t timer_id);
extern "C" bool __gots_clear_interval(int64_t timer_id);

// Stub implementations for functions used by runtime_syscalls.cpp
void* __string_create(const char* str) {
    return (void*)strdup(str);
}

// String interning for literals - simple implementation for now
void* __string_intern(const char* str) {
    // For now, just return a copy - could optimize with interning later
    return (void*)strdup(str);
}

// Duplicate function definition removed

// Deprecated simple array functions removed - use new DynamicArray/TypedArray system

// All deprecated simple array functions removed

// Type-aware array creation functions - know the array type at creation time
extern "C" void* __array_create_dynamic(int64_t size) {
    // Creates a DynamicArray for mixed types
    DynamicArray* array = new DynamicArray();
    if (size > 0) {
        for (int64_t i = 0; i < size; i++) {
            array->push(0.0);  // Initialize with zeros
        }
    }
    return array;
}

extern "C" void* __array_create_int64(int64_t size) {
    // Creates a typed Int64Array
    std::vector<size_t> shape = {static_cast<size_t>(size)};
    Int64Array* array = new Int64Array(shape);
    return array;
}

extern "C" void* __array_create_float64(int64_t size) {
    // Creates a typed Float64Array  
    std::vector<size_t> shape = {static_cast<size_t>(size)};
    Float64Array* array = new Float64Array(shape);
    return array;
}

extern "C" void* __array_create_int32(int64_t size) {
    // Creates a typed Int32Array
    std::vector<size_t> shape = {static_cast<size_t>(size)};
    Int32Array* array = new Int32Array(shape);
    return array;
}

extern "C" void* __array_create_float32(int64_t size) {
    // Creates a typed Float32Array
    std::vector<size_t> shape = {static_cast<size_t>(size)};
    Float32Array* array = new Float32Array(shape);
    return array;
}

// Type-aware array push functions - no type guessing needed
extern "C" void __array_push_dynamic(void* array, int64_t value_bits) {
    if (!array) return;
    DynamicArray* arr = static_cast<DynamicArray*>(array);
    
    // Convert int64_t bits to double and store as DynamicValue
    union {
        int64_t i;
        double d;
    } converter;
    converter.i = value_bits;
    arr->push(converter.d);
}

extern "C" void __array_push_int64_typed(void* array, int64_t value) {
    if (!array) return;
    Int64Array* arr = static_cast<Int64Array*>(array);
    arr->push(value);
}

extern "C" void __array_push_float64_typed(void* array, double value) {
    if (!array) return;
    Float64Array* arr = static_cast<Float64Array*>(array);
    arr->push(value);
}

extern "C" void __array_push_int32_typed(void* array, int32_t value) {
    if (!array) return;
    Int32Array* arr = static_cast<Int32Array*>(array);
    arr->push(value);
}

extern "C" void __array_push_float32_typed(void* array, float value) {
    if (!array) return;
    Float32Array* arr = static_cast<Float32Array*>(array);
    arr->push(value);
}

// Array factory functions for specific use cases
extern "C" void* __array_zeros_typed(int64_t size, void* dtype_string) {
    if (!dtype_string) {
        // No dtype specified, create dynamic array with zeros
        DynamicArray* array = new DynamicArray();
        if (size > 0) {
            for (int64_t i = 0; i < size; i++) {
                array->push(0.0);
            }
        }
        return array;
    }
    
    // Extract dtype string (assuming it's a GoTSString pointer)
    GoTSString* dtype_str = static_cast<GoTSString*>(dtype_string);
    std::string dtype = dtype_str->c_str();
    
    std::vector<size_t> shape = {static_cast<size_t>(size)};
    
    if (dtype == "int64") {
        return new Int64Array(shape);  // Automatically initializes with zeros
    } else if (dtype == "float64") {
        return new Float64Array(shape);  // Automatically initializes with zeros
    } else if (dtype == "int32") {
        return new Int32Array(shape);  // Automatically initializes with zeros
    } else if (dtype == "float32") {
        return new Float32Array(shape);  // Automatically initializes with zeros
    } else {
        // Unknown dtype, fallback to dynamic array
        DynamicArray* array = new DynamicArray();
        if (size > 0) {
            for (int64_t i = 0; i < size; i++) {
                array->push(0.0);
            }
        }
        return array;
    }
}

extern "C" void* __array_ones_dynamic(int64_t size) {
    // Creates a DynamicArray filled with ones
    DynamicArray* array = new DynamicArray();
    if (size > 0) {
        for (int64_t i = 0; i < size; i++) {
            array->push(1.0);  // Fill with ones
        }
    }
    return array;
}

// Typed ones array creation functions for maximum performance
extern "C" void* __array_ones_int64(int64_t size) {
    std::vector<size_t> shape = {static_cast<size_t>(size)};
    Int64Array* array = new Int64Array(shape);
    // Fill with ones
    for (int64_t i = 0; i < size; i++) {
        array->push(1);
    }
    return array;
}

extern "C" void* __array_ones_float64(int64_t size) {
    std::vector<size_t> shape = {static_cast<size_t>(size)};
    Float64Array* array = new Float64Array(shape);
    // Fill with ones
    for (int64_t i = 0; i < size; i++) {
        array->push(1.0);
    }
    return array;
}

extern "C" void* __array_ones_int32(int64_t size) {
    std::vector<size_t> shape = {static_cast<size_t>(size)};
    Int32Array* array = new Int32Array(shape);
    // Fill with ones
    for (int64_t i = 0; i < size; i++) {
        array->push(1);
    }
    return array;
}

extern "C" void* __array_ones_float32(int64_t size) {
    std::vector<size_t> shape = {static_cast<size_t>(size)};
    Float32Array* array = new Float32Array(shape);
    // Fill with ones
    for (int64_t i = 0; i < size; i++) {
        array->push(1.0f);
    }
    return array;
}

// All deprecated simple array functions removed - use __array_* functions instead

// Simple array functions have been deprecated - use new DynamicArray system instead

// Timer management functions moved to goroutine_system.cpp

} // extern "C"

// Legacy function removed - use __lookup_function_fast(func_id) instead

// Thread-local storage for goroutine context

thread_local bool g_is_goroutine_context = false;

namespace ultraScript {
    extern std::atomic<int64_t> g_active_goroutine_count;
}

extern "C" void __set_goroutine_context(int64_t is_goroutine) {
    bool was_goroutine = g_is_goroutine_context;
    g_is_goroutine_context = (is_goroutine != 0);
    
    if (g_is_goroutine_context && !was_goroutine) {
        // Setting up goroutine context
        // Increment active goroutine count
        ultraScript::g_active_goroutine_count.fetch_add(1);
    } else if (!g_is_goroutine_context && was_goroutine) {
        // Cleaning up goroutine context
        // Decrement active goroutine count
        ultraScript::g_active_goroutine_count.fetch_sub(1);
    }
}


// Ultra-High-Performance Direct Address Goroutine Spawn
namespace ultraScript {
void* __goroutine_spawn_func_ptr(void* func_ptr, void* arg) {
    
    printf("[DEBUG] __goroutine_spawn_func_ptr called with func_ptr=%p, arg=%p\n", func_ptr, arg);
    
    // Cast function pointer to proper type and spawn goroutine directly
    // This is the FASTEST possible goroutine spawn - zero overhead, direct address call
    if (func_ptr) {
        printf("[DEBUG] Spawning goroutine with function at address %p\n", func_ptr);
        
        // Use the goroutine scheduler to spawn with the function pointer
        typedef void (*func_t)();
        func_t function = reinterpret_cast<func_t>(func_ptr);
        
        std::function<void()> task = [function]() {
            printf("[DEBUG] Goroutine task executing, calling function at %p\n", function);
            function();
            printf("[DEBUG] Goroutine task completed\n");
        };
        
        GoroutineScheduler::instance().spawn(task, nullptr);
        printf("[DEBUG] Goroutine spawned successfully\n");
    } else {
        std::cerr << "ERROR: __goroutine_spawn_func_ptr called with null function pointer" << std::endl;
    }
    
    return nullptr; // TODO: Return actual goroutine handle if needed
}

// Get the executable memory base address for relative offset calculations
extern "C" void* __get_executable_memory_base() {
    std::lock_guard<std::mutex> lock(g_executable_memory.mutex);
    printf("[DEBUG] __get_executable_memory_base called, returning %p\n", g_executable_memory.ptr);
    return g_executable_memory.ptr;
}

// Timer system functions moved to goroutine_system.cpp

extern "C" const char* __dynamic_method_toString(void* obj) {
    // For now, we'll treat this as a simple array toString
    // In a full implementation, this would check object type and call appropriate toString
    if (obj) {
        Array* array = static_cast<Array*>(obj);
        std::string str = array->toString();
        return strdup(str.c_str());
    }
    return strdup("undefined");
}

// Typed array functions - stubs for now
extern "C" void* __typed_array_create_int32(int64_t size) {
    std::vector<size_t> shape = {static_cast<size_t>(size)};
    Int32Array* arr = new Int32Array(shape);
    return arr;
}

extern "C" void* __typed_array_create_int64(int64_t size) {
    std::vector<size_t> shape = {static_cast<size_t>(size)};
    Int64Array* arr = new Int64Array(shape);
    return arr;
}

extern "C" void* __typed_array_create_float32(int64_t size) {
    std::vector<size_t> shape = {static_cast<size_t>(size)};
    Float32Array* arr = new Float32Array(shape);
    return arr;
}

extern "C" void* __typed_array_create_float64(int64_t size) {
    std::cout << "[DEBUG] Creating Float64Array with size=" << size << std::endl;
    std::cout.flush();
    std::vector<size_t> shape = {static_cast<size_t>(size)};
    Float64Array* arr = new Float64Array(shape);
    std::cout << "[DEBUG] Float64Array created at address: " << arr << ", is_1d=" << arr->is_1d() << ", size=" << arr->size() << std::endl;
    std::cout.flush();
    return arr;
}

extern "C" void* __typed_array_create_uint8(int64_t size) {
    std::vector<size_t> shape = {static_cast<size_t>(size)};
    Uint8Array* arr = new Uint8Array(shape);
    return arr;
}

extern "C" void* __typed_array_create_uint16(int64_t size) {
    std::vector<size_t> shape = {static_cast<size_t>(size)};
    Uint16Array* arr = new Uint16Array(shape);
    return arr;
}

extern "C" void* __typed_array_create_uint32(int64_t size) {
    std::vector<size_t> shape = {static_cast<size_t>(size)};
    Uint32Array* arr = new Uint32Array(shape);
    return arr;
}

extern "C" void* __typed_array_create_uint64(int64_t size) {
    std::vector<size_t> shape = {static_cast<size_t>(size)};
    Uint64Array* arr = new Uint64Array(shape);
    return arr;
}

extern "C" void __typed_array_push_int32(void* array, int32_t value) {
    if (array) {
        static_cast<Int32Array*>(array)->push(value);
    }
}

extern "C" void __typed_array_push_int64(void* array, int64_t value) {
    if (array) {
        static_cast<Int64Array*>(array)->push(value);
    }
}

extern "C" void __typed_array_push_float32(void* array, float value) {
    if (array) {
        static_cast<Float32Array*>(array)->push(value);
    }
}

extern "C" void __typed_array_push_float64(void* array, double value) {
    if (array) {
        static_cast<Float64Array*>(array)->push(value);
    }
}

extern "C" void __typed_array_push_uint8(void* array, uint8_t value) {
    if (array) {
        static_cast<Uint8Array*>(array)->push(value);
    }
}

extern "C" void __typed_array_push_uint16(void* array, uint16_t value) {
    if (array) {
        static_cast<Uint16Array*>(array)->push(value);
    }
}

extern "C" void __typed_array_push_uint32(void* array, uint32_t value) {
    if (array) {
        static_cast<Uint32Array*>(array)->push(value);
    }
}

extern "C" void __typed_array_push_uint64(void* array, uint64_t value) {
    if (array) {
        static_cast<Uint64Array*>(array)->push(value);
    }
}

extern "C" int64_t __typed_array_size(void* array) {
    if (array) {
        // Assuming we can cast to any typed array for size (they all have same interface)
        return static_cast<int64_t>(static_cast<Int32Array*>(array)->size());
    }
    return 0;
}

extern "C" double __typed_array_sum_float64(void* array) {
    if (array) {
        return static_cast<Float64Array*>(array)->sum();
    }
    return 0.0;
}

extern "C" int64_t __typed_array_sum_int64(void* array) {
    if (array) {
        return static_cast<Int64Array*>(array)->sum();
    }
    return 0;
}

// Deprecated typed array functions removed - use new DynamicArray/TypedArray system instead

// DynamicValue allocation functions for ANY type variables
extern "C" void* __dynamic_value_create_from_double(int64_t double_bits) {
    // Convert int64_t bit pattern back to double
    union {
        int64_t i;
        double d;
    } converter;
    converter.i = double_bits;
    
    DynamicValue* dyn_val = new DynamicValue(converter.d);
    return static_cast<void*>(dyn_val);
}

extern "C" void* __dynamic_value_create_from_int64(int64_t value) {
    DynamicValue* dyn_val = new DynamicValue(value);
    return static_cast<void*>(dyn_val);
}

extern "C" void* __dynamic_value_create_from_bool(bool value) {
    DynamicValue* dyn_val = new DynamicValue(value);
    return static_cast<void*>(dyn_val);
}

extern "C" void* __dynamic_value_create_from_string(void* string_ptr) {
    // Convert GoTSString to std::string and create DynamicValue
    GoTSString* gots_str = static_cast<GoTSString*>(string_ptr);
    std::string cpp_str(gots_str->c_str());
    DynamicValue* dyn_val = new DynamicValue(cpp_str);
    return static_cast<void*>(dyn_val);
}

// Object management functions
extern "C" int64_t __object_create(const char* class_name, int64_t property_count) {
    using namespace ultraScript;
    
    // Create a new ObjectInstance
    auto obj = std::make_unique<ObjectInstance>(class_name ? class_name : "", property_count);
    
    // Assign a unique ID and store in registry
    int64_t object_id = next_object_id.fetch_add(1);
    object_registry[object_id] = std::move(obj);
    
    return object_id;
}

extern "C" void __object_set_property(int64_t object_id, int64_t property_index, int64_t value) {
    using namespace ultraScript;
    
    auto it = object_registry.find(object_id);
    if (it != object_registry.end() && property_index >= 0 && 
        property_index < it->second->property_count) {
        it->second->property_data[property_index] = value;
    }
}

extern "C" void __object_set_property_name(int64_t object_id, int64_t property_index, const char* property_name) {
    using namespace ultraScript;
    
    auto it = object_registry.find(object_id);
    if (it != object_registry.end() && property_index >= 0 && 
        property_index < it->second->property_count && property_name) {
        it->second->property_names[property_index] = property_name;
    }
}

extern "C" int64_t __object_get_property(int64_t object_id, int64_t property_index) {
    using namespace ultraScript;
    
    auto it = object_registry.find(object_id);
    if (it != object_registry.end() && property_index >= 0 && 
        property_index < it->second->property_count) {
        return it->second->property_data[property_index];
    }
    return 0;
}

extern "C" const char* __object_get_property_name(int64_t object_id, int64_t property_index) {
    using namespace ultraScript;
    
    auto it = object_registry.find(object_id);
    if (it != object_registry.end() && property_index >= 0 && 
        property_index < it->second->property_count) {
        return it->second->property_names[property_index].c_str();
    }
    return nullptr;
}

extern "C" void* __dynamic_get_property(void* dynamic_value_ptr, const char* property_name) {
    using namespace ultraScript;
    
    if (!dynamic_value_ptr || !property_name) {
        return nullptr;
    }
    
    // The dynamic_value_ptr actually contains an object ID (int64_t)
    int64_t object_id = *reinterpret_cast<int64_t*>(dynamic_value_ptr);
    
    // Find the object in the registry
    auto it = object_registry.find(object_id);
    if (it == object_registry.end()) {
        return nullptr;
    }
    
    ObjectInstance* obj = it->second.get();
    
    // Look for the property by name in the properties map
    auto prop_it = obj->properties.find(property_name);
    if (prop_it != obj->properties.end()) {
        // Create a dynamic value containing the property value
        static int64_t result_value;
        result_value = prop_it->second;
        return &result_value;
    }
    
    // Also check the property_names array for indexed access
    for (int64_t i = 0; i < obj->property_count; i++) {
        if (obj->property_names[i] == property_name) {
            static int64_t result_value;
            result_value = obj->property_data[i];
            return &result_value;
        }
    }
    
    return nullptr;
}

// String functions
extern "C" void* __string_concat(void* str1, void* str2) {
    if (!str1 || !str2) return nullptr;
    
    // Simple implementation - concatenate C strings
    char* s1 = static_cast<char*>(str1);
    char* s2 = static_cast<char*>(str2);
    
    size_t len1 = strlen(s1);
    size_t len2 = strlen(s2);
    char* result = static_cast<char*>(malloc(len1 + len2 + 1));
    
    strcpy(result, s1);
    strcat(result, s2);
    
    return result;
}

// Console timing functions
extern "C" void __console_time(const char* label) {
    static std::unordered_map<std::string, std::chrono::high_resolution_clock::time_point> timers;
    std::string timer_label = label ? label : "default";
    timers[timer_label] = std::chrono::high_resolution_clock::now();
}

extern "C" void __console_timeEnd(const char* label) {
    static std::unordered_map<std::string, std::chrono::high_resolution_clock::time_point> timers;
    std::string timer_label = label ? label : "default";
    
    auto it = timers.find(timer_label);
    if (it != timers.end()) {
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - it->second);
        
        std::cout << timer_label << ": " << duration.count() << "ms" << std::endl;
        timers.erase(it);
    }
}

// Promise functions
extern "C" void* __promise_all(void* promises_array) {
    // Simplified implementation - create a new promise that resolves when all input promises resolve
    using namespace ultraScript;
    
    auto promise = std::make_shared<Promise>();
    // For now, resolve immediately with the input array
    promise->resolve(promises_array);
    
    return new std::shared_ptr<Promise>(promise);
}

extern "C" void* __promise_await(void* promise_ptr) {
    if (!promise_ptr) return 0;
    
    using namespace ultraScript;
    auto promise_shared = *static_cast<std::shared_ptr<Promise>*>(promise_ptr);
    
    // Simple blocking wait
    while (!promise_shared->resolved.load()) {
        std::this_thread::yield();
    }
    
    // Return the promise value as void*
    return promise_shared->value.get();
}

// Regex functions
extern "C" void* __register_regex_pattern(const char* pattern) {
    // Simple implementation - just return the pattern as ID
    static std::atomic<int64_t> pattern_id{1};
    int64_t id = pattern_id.fetch_add(1);
    
    // Store pattern in a registry (simplified)
    static std::unordered_map<int64_t, std::string> pattern_registry;
    pattern_registry[id] = pattern ? pattern : "";
    
    return reinterpret_cast<void*>(id);
}

extern "C" void* __regex_create_by_id(int64_t pattern_id) {
    // Simple implementation - return the pattern ID as a regex object
    return reinterpret_cast<void*>(pattern_id);
}

extern "C" void* __string_match(void* string_ptr, void* regex_ptr) {
    if (!string_ptr || !regex_ptr) return nullptr;
    
    // Simple implementation - always return a match result for now
    // This would need proper regex matching implementation
    
    // Create a simple array with match results
    using namespace ultraScript;
    Array* result = new Array();
    result->push(std::string("match")); // Simplified match result
    
    return result;
}

} // namespace ultraScript
