#include "runtime.h"
#include "compiler.h"
// Removed lexical_scope.h include - using pure static analysis now
#include "regex.h"
#include "goroutine_system_v2.h"
#include "ultra_performance_array.h"
#include "dynamic_properties.h"
#include <iostream>
#include <algorithm>
#include <chrono>

// External goroutine ID counter from goroutine_system_v2.cpp
extern std::atomic<int64_t> g_next_goroutine_id;
#include <unordered_map>
#include <pthread.h>
#include <signal.h>
#include <setjmp.h>
#include <cmath>
#include <regex>
#include <cstring>
#include <dlfcn.h>

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

// Global console output mutex for thread safety
std::mutex g_console_mutex;

// Debug functions for memory inspection
extern "C" void __debug_stack_store(void* rbp_addr, int64_t offset, void* value) {
    void** stack_location = (void**)((char*)rbp_addr + offset);
    std::cout << "[STACK_DEBUG] STORING: value=" << value 
              << " at [rbp" << (offset >= 0 ? "+" : "") << offset << "] = " << stack_location 
              << " (rbp=" << rbp_addr << ")" << std::endl;
    
    // Add extra validation for goroutine safety
    void* current_sp;
    asm volatile ("mov %%rsp, %0" : "=r"(current_sp));
    std::cout << "[STACK_DEBUG] Current RSP: " << current_sp << ", writing to: " << stack_location << std::endl;
    
    // Check if we're writing to a valid stack location
    if ((char*)stack_location > (char*)current_sp - 0x100000 && // Within 1MB below SP
        (char*)stack_location < (char*)current_sp + 0x1000) {   // Within 4KB above SP
        std::cout << "[STACK_DEBUG] Stack location appears valid" << std::endl;
    } else {
        std::cout << "[STACK_DEBUG] WARNING: Stack location may be invalid!" << std::endl;
    }
}

extern "C" void __debug_stack_load(void* rbp_addr, int64_t offset, void* loaded_value) {
    void** stack_location = (void**)((char*)rbp_addr + offset);
    void* actual_value = *stack_location;
    std::cout << "[STACK_DEBUG] LOADING: from [rbp" << (offset >= 0 ? "+" : "") << offset << "] = " << stack_location 
              << " | Expected=" << loaded_value << " | Actual=" << actual_value << std::endl;
    if (loaded_value != actual_value) {
        std::cout << "[STACK_DEBUG] *** MISMATCH! Expected " << loaded_value << " but loaded " << actual_value << " ***" << std::endl;
    }
}

// Global instances
// Using pointers to control initialization/destruction order
// static GoroutineScheduler* global_scheduler = nullptr;  // REMOVED: Using EventDrivenScheduler V2
static std::mutex scheduler_mutex;

// Timer globals moved to goroutine_system_v2.cpp to avoid duplicates

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

// Object registry removed - property access system redesigned according to CLAUDE.md

// High-Performance Function Registry Implementation
FunctionEntry g_function_table[MAX_FUNCTIONS];
std::atomic<uint16_t> g_next_function_id{1};  // Start at 1, 0 is reserved for "invalid"

// Global promise registry for cleanup
static std::unordered_set<void*> g_allocated_promises;
static std::mutex g_promise_registry_mutex;

// Helper function to create and track a promise - UNUSED in V2
// static void* create_tracked_promise(std::shared_ptr<Promise> promise) {
//     auto* promise_ptr = new std::shared_ptr<Promise>(promise);
//     
//     // Track allocated promise for cleanup
//     {
//         std::lock_guard<std::mutex> lock(g_promise_registry_mutex);
//         g_allocated_promises.insert(promise_ptr);
//     }
    
//     return promise_ptr;
// }

// Global executable memory info for thread-safe access
ExecutableMemoryInfo g_executable_memory = {nullptr, 0, {}};

// Global method registry for dynamic method lookup
static std::unordered_map<std::string, size_t> g_method_offsets;
static std::mutex g_method_registry_mutex;

extern "C" {

// Method registration for dynamic lookup
void __register_method_offset(const char* method_name, size_t offset) {
    std::lock_guard<std::mutex> lock(g_method_registry_mutex);
    g_method_offsets[std::string(method_name)] = offset;
}

void* __get_method_address(const char* method_name) {
    std::lock_guard<std::mutex> lock(g_method_registry_mutex);
    auto it = g_method_offsets.find(std::string(method_name));
    if (it != g_method_offsets.end()) {
        void* base = __get_executable_memory_base();
        if (base) {
            return reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(base) + it->second);
        }
    }
    return nullptr;
}

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

// Prevent double cleanup
static std::atomic<bool> cleanup_completed{false};

// Main cleanup - wait for all goroutines
void __runtime_cleanup() {
    // Check if cleanup was already called
    if (cleanup_completed.exchange(true)) {
        std::cout << "DEBUG: __runtime_cleanup() already called, skipping" << std::endl;
        return;
    }
    
    std::cout << "DEBUG: __runtime_cleanup() starting" << std::endl;
    __new_goroutine_system_cleanup();
    std::cout << "DEBUG: __runtime_cleanup() completed" << std::endl;
}

// Main goroutine functions moved to goroutine_system_v2.cpp

// Optimized goroutine spawn with direct function IDs - NO string lookups
void* __goroutine_spawn_fast(uint16_t func_id) {
    void* func_ptr = __lookup_function_fast(func_id);
    if (!func_ptr) {
        std::cerr << "ERROR: Invalid function ID: " << func_id << std::endl;
        return nullptr;
    }
    
    // Create a task that calls the function directly - minimal overhead
    typedef int64_t (*FuncType)();
    FuncType func = reinterpret_cast<FuncType>(func_ptr);
    
    auto goroutine = std::make_shared<Goroutine>(g_next_goroutine_id.fetch_add(1), 
                                                 [func]() { func(); });
    
    EventDrivenScheduler::instance().schedule_regular(goroutine);
    return reinterpret_cast<void*>(1);
}

void* __goroutine_spawn_fast_arg1(uint16_t func_id, int64_t arg1) {
    void* func_ptr = __lookup_function_fast(func_id);
    if (!func_ptr) {
        std::cerr << "ERROR: Invalid function ID: " << func_id << std::endl;
        return nullptr;
    }
    
    // Create a task that calls the function with one argument - minimal overhead
    typedef int64_t (*FuncType)(int64_t);
    FuncType func = reinterpret_cast<FuncType>(func_ptr);
    
    auto goroutine = std::make_shared<Goroutine>(g_next_goroutine_id.fetch_add(1), 
                                                 [func, arg1]() { func(arg1); });
    
    EventDrivenScheduler::instance().schedule_regular(goroutine);
    return reinterpret_cast<void*>(1);
}

void* __goroutine_spawn_fast_arg2(uint16_t func_id, int64_t arg1, int64_t arg2) {
    void* func_ptr = __lookup_function_fast(func_id);
    if (!func_ptr) {
        std::cerr << "ERROR: Invalid function ID: " << func_id << std::endl;
        return nullptr;
    }
    
    // Create a task that calls the function with two arguments - minimal overhead
    typedef int64_t (*FuncType)(int64_t, int64_t);
    FuncType func = reinterpret_cast<FuncType>(func_ptr);
    
    auto goroutine = std::make_shared<Goroutine>(g_next_goroutine_id.fetch_add(1), 
                                                 [func, arg1, arg2]() { func(arg1, arg2); });
    
    EventDrivenScheduler::instance().schedule_regular(goroutine);
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

// Class property lookup for optimized bracket access
extern "C" int64_t __class_property_lookup(void* object, void* property_name_string, void* class_info_ptr) {
    if (!object || !property_name_string || !class_info_ptr) {
        return 0;
    }
    
    ClassInfo* class_info = static_cast<ClassInfo*>(class_info_ptr);
    
    // Extract the property name from the GoTSString
    GoTSString* gots_str = static_cast<GoTSString*>(property_name_string);
    std::string property_name;
    if (gots_str && gots_str->data()) {
        property_name = std::string(gots_str->data(), gots_str->length());
    } else {
        return 0;
    }
    
    // Find the property in the class fields
    for (size_t i = 0; i < class_info->fields.size(); ++i) {
        if (class_info->fields[i].name == property_name) {
            // Object layout: [class_name_ptr][property_count][ref_count][dynamic_map_ptr][property0][property1]...
            // Properties start at offset 32 (4 * 8 bytes for metadata)
            int64_t property_offset = OBJECT_PROPERTIES_START_OFFSET + (i * 8);
            
            // Direct memory access to get the property value
            int64_t* object_ptr = static_cast<int64_t*>(object);
            void* property_value = reinterpret_cast<void*>(object_ptr[property_offset / 8]);
            
            // Determine the property type and create appropriate DynamicValue
            DataType property_type = class_info->fields[i].type;
            switch (property_type) {
                case DataType::STRING: {
                    // Create DynamicValue from string - check for null/uninitialized
                    if (property_value && property_value != nullptr) {
                        GoTSString* str_ptr = static_cast<GoTSString*>(property_value);
                        if (str_ptr && str_ptr->data()) {
                            DynamicValue* dyn_val = new DynamicValue(std::string(str_ptr->c_str()));
                            return reinterpret_cast<int64_t>(dyn_val);
                        }
                    }
                    // Return empty string for uninitialized string properties
                    DynamicValue* dyn_val = new DynamicValue(std::string(""));
                    return reinterpret_cast<int64_t>(dyn_val);
                }
                case DataType::INT64: {
                    // Create DynamicValue from int64 - handle uninitialized values
                    if (property_value && property_value != nullptr) {
                        int64_t int_value = reinterpret_cast<int64_t>(property_value);
                        DynamicValue* dyn_val = new DynamicValue(static_cast<double>(int_value));
                        return reinterpret_cast<int64_t>(dyn_val);
                    }
                    // Return 0 for uninitialized int64 properties
                    DynamicValue* dyn_val = new DynamicValue(0.0);
                    return reinterpret_cast<int64_t>(dyn_val);
                }
                case DataType::FLOAT64: {
                    // Create DynamicValue from double - handle uninitialized values
                    if (property_value && property_value != nullptr) {
                        double* double_ptr = static_cast<double*>(property_value);
                        if (double_ptr) {
                            DynamicValue* dyn_val = new DynamicValue(*double_ptr);
                            return reinterpret_cast<int64_t>(dyn_val);
                        }
                    }
                    // Return 0.0 for uninitialized float64 properties
                    DynamicValue* dyn_val = new DynamicValue(0.0);
                    return reinterpret_cast<int64_t>(dyn_val);
                }
                default: {
                    // For other types, create a generic DynamicValue
                    DynamicValue* dyn_val = new DynamicValue(property_value);
                    dyn_val->type = property_type;
                    return reinterpret_cast<int64_t>(dyn_val);
                }
            }
        }
    }
    
    // Property not found
    return 0;
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

// Console log for GoTSString objects
void __console_log_gots(void* gots_string_ptr) {
    if (gots_string_ptr) {
        GoTSString* str = static_cast<GoTSString*>(gots_string_ptr);
        std::cout.write(str->data(), str->size());
    }
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
        
        // Try to call the GoTSString logger directly and see if it works
        try {
            __console_log_string(ptr);
            return;
        } catch (...) {
            // String printing failed, try other types
        }
        
        // Object ID check removed - object registry system redesigned
    }
    
    // Default: treat as number
    std::cout << value;
}

void __console_log_string(void* string_ptr) {
    std::lock_guard<std::mutex> lock(g_console_mutex);
    if (string_ptr) {
        // Handle GoTSString objects properly
        GoTSString* gots_str = static_cast<GoTSString*>(string_ptr);
        // Use data() and size() to properly handle null bytes
        std::cout.write(gots_str->data(), gots_str->size());
        std::cout.flush();
    }
}

void __console_log_object(int64_t object_address) {
    // Object registry system removed - will be reimplemented according to new architecture
    std::cout << "Object#" << object_address;
}

// Debug and introspection functions
int64_t __debug_get_ref_count(int64_t object_address) {
    std::cout << "[DEBUG] __debug_get_ref_count called with address: " << object_address << std::endl;
    std::cout.flush();
    
    if (object_address == 0) {
        std::cout << "[DEBUG] __debug_get_ref_count: null address, returning 0" << std::endl;
        std::cout.flush();
        return 0;
    }
    
    // Cast object_address back to actual object pointer
    void* object_ptr = reinterpret_cast<void*>(object_address);
    std::cout << "[DEBUG] __debug_get_ref_count: object_ptr=" << object_ptr << std::endl;
    std::cout.flush();
    
    // Object layout: [class_name_ptr][property_count][ref_count][properties...]
    // ref_count is at offset 16 (8 bytes for class_name_ptr + 8 bytes for property_count)
    std::atomic<int64_t>* ref_count_ptr = reinterpret_cast<std::atomic<int64_t>*>(
        static_cast<char*>(object_ptr) + 16
    );
    
    std::cout << "[DEBUG] __debug_get_ref_count: ref_count_ptr=" << ref_count_ptr << std::endl;
    std::cout.flush();
    
    int64_t ref_count = ref_count_ptr->load();
    std::cout << "[DEBUG] __debug_get_ref_count: loaded ref_count=" << ref_count << std::endl;
    std::cout.flush();
    
    return ref_count;
}

int64_t __object_get_memory_address(int64_t object_address) {
    // This function simply returns the object address itself
    // since object_address IS the memory address in our system
    return object_address;
}

// Runtime global object functions
void* __runtime_get_ref_count(int64_t object_address) {
    int64_t ref_count = __debug_get_ref_count(object_address);
    printf("[DEBUG] __runtime_get_ref_count: About to create DynamicValue from ref_count=%ld\n", ref_count);
    void* result = __dynamic_value_create_from_uint64(static_cast<uint64_t>(ref_count));
    printf("[DEBUG] __runtime_get_ref_count: Created DynamicValue at %p\n", result);
    return result;
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
    GoTSString* gots_str = new GoTSString(str);
    return static_cast<void*>(gots_str);
}

// String interning for literals - simple implementation for now
void* __string_intern(const char* str) {
    std::cout << "[DEBUG] __string_intern called with: " << (str ? str : "null") << std::endl;
    std::cout.flush();
    
    try {
        // For now, just return a new GoTSString - could optimize with interning later
        GoTSString* gots_str = new GoTSString(str);
        std::cout << "[DEBUG] __string_intern created GoTSString at: " << gots_str << std::endl;
        std::cout.flush();
        return static_cast<void*>(gots_str);
    } catch (const std::exception& e) {
        std::cout << "[ERROR] __string_intern failed: " << e.what() << std::endl;
        std::cout.flush();
        return nullptr;
    }
}

// String creation with length - handles null bytes properly
extern "C" void* __string_create_with_length(const char* data, size_t length) {
    GoTSString* gots_str = new GoTSString(data, length);
    return static_cast<void*>(gots_str);
}

// String creation from std::string
extern "C" void* __string_create_from_std_string(const std::string& str) {
    GoTSString* gots_str = new GoTSString(str.data(), str.length());
    return static_cast<void*>(gots_str);
}

// Get string length (handles null bytes properly)
extern "C" size_t __string_length(void* string_ptr) {
    if (!string_ptr) return 0;
    GoTSString* str = static_cast<GoTSString*>(string_ptr);
    return str->size();
}

// Get string data pointer (for interfacing with C code)
extern "C" const char* __string_data(void* string_ptr) {
    if (!string_ptr) return nullptr;
    GoTSString* str = static_cast<GoTSString*>(string_ptr);
    return str->data();
}

// String comparison functions for high-performance string operations
extern "C" bool __string_equals(void* str1_ptr, void* str2_ptr) {
    if (!str1_ptr || !str2_ptr) {
        return (str1_ptr == str2_ptr);  // Both null = equal, one null = not equal
    }
    
    GoTSString* str1 = static_cast<GoTSString*>(str1_ptr);
    GoTSString* str2 = static_cast<GoTSString*>(str2_ptr);
    
    // Fast path: same length check first
    if (str1->length() != str2->length()) {
        return false;
    }
    
    // Use memcmp for high-performance comparison (handles null bytes correctly)
    return (memcmp(str1->data(), str2->data(), str1->length()) == 0);
}

extern "C" int64_t __string_compare(void* str1_ptr, void* str2_ptr) {
    if (!str1_ptr && !str2_ptr) return 0;
    if (!str1_ptr) return -1;
    if (!str2_ptr) return 1;
    
    GoTSString* str1 = static_cast<GoTSString*>(str1_ptr);
    GoTSString* str2 = static_cast<GoTSString*>(str2_ptr);
    
    size_t min_len = std::min(str1->length(), str2->length());
    int result = memcmp(str1->data(), str2->data(), min_len);
    
    if (result == 0) {
        // Strings are equal up to min_len, compare lengths
        if (str1->length() < str2->length()) return -1;
        if (str1->length() > str2->length()) return 1;
        return 0;
    }
    
    return (result < 0) ? -1 : 1;
}

// DynamicValue to typed value extraction functions
extern "C" void* __dynamic_value_extract_string(void* dynamic_value_ptr) {
    if (!dynamic_value_ptr) {
        // Return empty string for null
        return __string_intern("");
    }
    
    DynamicValue* dyn_val = static_cast<DynamicValue*>(dynamic_value_ptr);
    
    // Allow coercion for basic types, throw for object types
    switch (dyn_val->type) {
        case DataType::STRING:
        case DataType::INT8:
        case DataType::INT16:
        case DataType::INT32:
        case DataType::INT64:
        case DataType::UINT8:
        case DataType::UINT16:
        case DataType::UINT32:
        case DataType::UINT64:
        case DataType::FLOAT32:
        case DataType::FLOAT64:
        case DataType::BOOLEAN: {
            // Allow conversion of basic types to string
            std::string str_value = dyn_val->to_string();
            return __string_intern(str_value.c_str());
        }
        case DataType::ARRAY:
        case DataType::CLASS_INSTANCE:
        case DataType::ANY:
        default: {
            // Throw for object types and other complex types
            std::string type_name;
            switch (dyn_val->type) {
                case DataType::ARRAY: type_name = "array"; break;
                case DataType::CLASS_INSTANCE: type_name = "object"; break;
                case DataType::ANY: type_name = "any"; break;
                default: type_name = "type " + std::to_string(static_cast<int>(dyn_val->type)); break;
            }
            
            std::string error_msg = "TypeError: Cannot convert " + type_name + " to string. " +
                                   "Use JSON.stringify() for objects/arrays or call .toString() method explicitly.";
            throw std::runtime_error(error_msg);
        }
    }
}

extern "C" int64_t __dynamic_value_extract_int64(void* dynamic_value_ptr) {
    if (!dynamic_value_ptr) {
        return 0;
    }
    
    DynamicValue* dyn_val = static_cast<DynamicValue*>(dynamic_value_ptr);
    
    // Convert to number and cast to int64
    double num_value = dyn_val->to_number();
    return static_cast<int64_t>(num_value);
}

extern "C" double __dynamic_value_extract_float64(void* dynamic_value_ptr) {
    if (!dynamic_value_ptr) {
        return 0.0;
    }
    
    DynamicValue* dyn_val = static_cast<DynamicValue*>(dynamic_value_ptr);
    return dyn_val->to_number();
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


// Legacy function removed - use __lookup_function_fast(func_id) instead

// Thread-local storage for goroutine context

thread_local bool g_is_goroutine_context = false;

extern std::atomic<int64_t> g_active_goroutine_count;

extern "C" void __set_goroutine_context(int64_t is_goroutine) {
    bool was_goroutine = g_is_goroutine_context;
    g_is_goroutine_context = (is_goroutine != 0);
    
    if (g_is_goroutine_context && !was_goroutine) {
        // Setting up goroutine context
        // Increment active goroutine count
        g_active_goroutine_count.fetch_add(1);
    } else if (!g_is_goroutine_context && was_goroutine) {
        // Cleaning up goroutine context
        // Decrement active goroutine count
        g_active_goroutine_count.fetch_sub(1);
    }
}


// Ultra-High-Performance Direct Address Goroutine Spawn
void* __goroutine_spawn_func_ptr(void* func_ptr, void* arg) {
    
    printf("[DEBUG] __goroutine_spawn_func_ptr called with func_ptr=%p, arg=%p\n", func_ptr, arg);
    
    // Cast function pointer to proper type and spawn goroutine directly
    // This is the FASTEST possible goroutine spawn - zero overhead, direct address call
    if (func_ptr) {
        printf("[DEBUG] Spawning goroutine with function at address %p\n", func_ptr);
        
        // Use the goroutine scheduler to spawn with the function pointer
        // CRITICAL: Must match the calling convention of the main function!
        typedef int (*func_t)();  // Same signature as main function
        func_t function = reinterpret_cast<func_t>(func_ptr);
        
        auto goroutine = std::make_shared<Goroutine>(g_next_goroutine_id.fetch_add(1), [function]() {
            printf("[DEBUG] Goroutine task executing, calling function at %p\n", function);
            int result = function();  // Properly handle return value
            printf("[DEBUG] Goroutine task completed with result: %d\n", result);
        });
        
        EventDrivenScheduler::instance().schedule_regular(goroutine);
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

extern "C" void* __dynamic_method_toString(void* obj) {
    // For now, we'll treat this as a simple array toString
    // In a full implementation, this would check object type and call appropriate toString
    if (obj) {
        Array* array = static_cast<Array*>(obj);
        std::string str = array->toString();
        GoTSString* result = new GoTSString(str.c_str(), str.length());
        return static_cast<void*>(result);
    }
    GoTSString* result = new GoTSString("undefined");
    return static_cast<void*>(result);
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
    std::cout << "[DYNAMIC_VALUE_CREATE] Starting with double_bits: " << double_bits << std::endl;
    
    // Convert int64_t bit pattern back to double
    union {
        int64_t i;
        double d;
    } converter;
    converter.i = double_bits;
    
    std::cout << "[DYNAMIC_VALUE_CREATE] Union conversion done" << std::endl;
    
    // Test if the issue is in the DynamicValue constructor
    try {
        DynamicValue* dyn_val = new DynamicValue(converter.d);
        std::cout << "[DYNAMIC_VALUE_CREATE] DynamicValue created successfully at: " << dyn_val << std::endl;
        return static_cast<void*>(dyn_val);
    } catch (...) {
        std::cout << "[DYNAMIC_VALUE_CREATE] Exception caught in DynamicValue constructor!" << std::endl;
        return nullptr;
    }
}

extern "C" void* __dynamic_value_create_from_int64(int64_t value) {
    DynamicValue* dyn_val = new DynamicValue(value);
    return static_cast<void*>(dyn_val);
}

extern "C" void* __dynamic_value_create_from_uint64(uint64_t value) {
    DynamicValue* dyn_val = new DynamicValue(static_cast<int64_t>(value));
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

extern "C" void* __dynamic_value_create_from_object(void* object_ptr) {
    DynamicValue* dyn_val = new DynamicValue(object_ptr);
    dyn_val->type = DataType::CLASS_INSTANCE;
    return static_cast<void*>(dyn_val);
}

extern "C" void* __dynamic_value_create_from_array(void* array_ptr) {
    DynamicValue* dyn_val = new DynamicValue(array_ptr);
    dyn_val->type = DataType::ARRAY;
    return static_cast<void*>(dyn_val);
}

// DynamicValue copy constructor for parameter passing (JavaScript value semantics)
extern "C" void* __dynamic_value_copy_for_parameter(void* source_dynamic_value) {
    if (!source_dynamic_value) {
        std::cout << "[DYNAMIC_VALUE_COPY] Source is null, creating null DynamicValue" << std::endl;
        return __dynamic_value_create_from_double(0); // Create default DynamicValue with 0
    }
    
    DynamicValue* source = static_cast<DynamicValue*>(source_dynamic_value);
    
    std::cout << "[DYNAMIC_VALUE_COPY] Copying DynamicValue from " << source_dynamic_value << std::endl;
    
    // Create a new DynamicValue by copying the contents (not the pointer)
    // This implements JavaScript value semantics for primitive types
    DynamicValue* new_dyn_val = nullptr;
    
    try {
        if (std::holds_alternative<double>(source->value)) {
            double val = std::get<double>(source->value);
            new_dyn_val = new DynamicValue(val);
            std::cout << "[DYNAMIC_VALUE_COPY] Copied double value: " << val << std::endl;
        }
        else if (std::holds_alternative<int64_t>(source->value)) {
            int64_t val = std::get<int64_t>(source->value);
            new_dyn_val = new DynamicValue(val);
            std::cout << "[DYNAMIC_VALUE_COPY] Copied int64_t value: " << val << std::endl;
        }
        else if (std::holds_alternative<bool>(source->value)) {
            bool val = std::get<bool>(source->value);
            new_dyn_val = new DynamicValue(val);
            std::cout << "[DYNAMIC_VALUE_COPY] Copied bool value: " << (val ? "true" : "false") << std::endl;
        }
        else if (std::holds_alternative<std::string>(source->value)) {
            std::string val = std::get<std::string>(source->value);
            new_dyn_val = new DynamicValue(val);
            std::cout << "[DYNAMIC_VALUE_COPY] Copied string value: " << val << std::endl;
        }
        else if (std::holds_alternative<void*>(source->value)) {
            // For objects, we DO copy the pointer (objects are passed by reference in JS)
            void* val = std::get<void*>(source->value);
            new_dyn_val = new DynamicValue(val);
            new_dyn_val->type = source->type; // Preserve the object type
            std::cout << "[DYNAMIC_VALUE_COPY] Copied object pointer: " << val << std::endl;
        }
        else {
            // Fallback: create a copy with the same variant value
            new_dyn_val = new DynamicValue();
            new_dyn_val->value = source->value;
            new_dyn_val->type = source->type;
            std::cout << "[DYNAMIC_VALUE_COPY] Fallback copy of variant value" << std::endl;
        }
        
        std::cout << "[DYNAMIC_VALUE_COPY] Created new DynamicValue at: " << new_dyn_val << std::endl;
        return static_cast<void*>(new_dyn_val);
        
    } catch (const std::exception& e) {
        std::cout << "[DYNAMIC_VALUE_COPY] ERROR: " << e.what() << std::endl;
        // Return a default DynamicValue with 0 if copying fails
        return __dynamic_value_create_from_double(0);
    }
}

// Object creation function removed - will be reimplemented according to new architecture

// Property access runtime functions removed - will be reimplemented according to new architecture

// String functions
extern "C" void* __string_concat(void* str1, void* str2) {
    if (!str1 || !str2) return nullptr;
    
    // Handle GoTSString concatenation properly
    GoTSString* s1 = static_cast<GoTSString*>(str1);
    GoTSString* s2 = static_cast<GoTSString*>(str2);
    
    // Create new GoTSString with proper concatenation
    GoTSString* result = new GoTSString(*s1 + *s2);
    
    return static_cast<void*>(result);
}

// String concatenation with C string (for number conversion)
extern "C" void* __string_concat_cstr(void* str_ptr, const char* cstr) {
    if (!str_ptr || !cstr) return nullptr;
    
    GoTSString* str = static_cast<GoTSString*>(str_ptr);
    GoTSString* result = new GoTSString(*str + GoTSString(cstr));
    
    return static_cast<void*>(result);
}

// String concatenation with C string on left (for number conversion)
extern "C" void* __string_concat_cstr_left(const char* cstr, void* str_ptr) {
    if (!cstr || !str_ptr) return nullptr;
    
    GoTSString* str = static_cast<GoTSString*>(str_ptr);
    GoTSString* result = new GoTSString(GoTSString(cstr) + *str);
    
    return static_cast<void*>(result);
}

// Console timing functions
extern "C" void __console_time(void* label_ptr) {
    static std::unordered_map<std::string, std::chrono::high_resolution_clock::time_point> timers;
    std::string timer_label = "default";
    
    if (label_ptr) {
        GoTSString* label_str = static_cast<GoTSString*>(label_ptr);
        timer_label = std::string(label_str->data(), label_str->size());
    }
    
    timers[timer_label] = std::chrono::high_resolution_clock::now();
}

extern "C" void __console_timeEnd(void* label_ptr) {
    static std::unordered_map<std::string, std::chrono::high_resolution_clock::time_point> timers;
    std::string timer_label = "default";
    
    if (label_ptr) {
        GoTSString* label_str = static_cast<GoTSString*>(label_ptr);
        timer_label = std::string(label_str->data(), label_str->size());
    }
    
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

    
    auto promise = std::make_shared<Promise>();
    // For now, resolve immediately with the input array
    promise->resolve(promises_array);
    
    return new std::shared_ptr<Promise>(promise);
}

extern "C" void* __promise_await(void* promise_ptr) {
    if (!promise_ptr) return 0;
    

    auto promise_shared = *static_cast<std::shared_ptr<Promise>*>(promise_ptr);
    
    // Simple blocking wait
    while (!promise_shared->resolved.load()) {
        std::this_thread::yield();
    }
    
    // Return the promise value as void*
    return promise_shared->value.get();
}

// Regex functions
extern "C" void* __register_regex_pattern(void* pattern_ptr) {
    // Simple implementation - just return the pattern as ID
    static std::atomic<int64_t> pattern_id{1};
    int64_t id = pattern_id.fetch_add(1);
    
    // Store pattern in a registry (simplified)
    static std::unordered_map<int64_t, std::string> pattern_registry;
    
    if (pattern_ptr) {
        GoTSString* pattern_str = static_cast<GoTSString*>(pattern_ptr);
        pattern_registry[id] = std::string(pattern_str->data(), pattern_str->size());
    } else {
        pattern_registry[id] = "";
    }
    
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

    Array* result = new Array();
    result->push(std::string("match")); // Simplified match result
    
    return result;
}

// ==================== Object Creation Functions ====================

int64_t __object_create(void* class_name_ptr, int64_t property_count) {
    std::cout << "[DEBUG] __object_create called with class_name_ptr=" << class_name_ptr << ", property_count=" << property_count << std::endl;
    std::cout.flush();
    
    try {
        // Object creation with reference counting and direct property access layout
        // New layout: [class_name_ptr][property_count][ref_count][dynamic_map_ptr][property0][property1]...
        
        // Calculate total size: metadata + ref_count + dynamic map pointer + inline properties
        size_t metadata_size = sizeof(void*) * 4; // class_name pointer + property_count + ref_count + dynamic_map_ptr
        size_t property_storage_size = property_count * sizeof(void*);
        size_t total_size = metadata_size + property_storage_size;
        
        // Allocate contiguous memory block
        void* raw_memory = calloc(1, total_size);
        if (!raw_memory) {
            throw std::bad_alloc();
        }
        
        // Layout: [class_name_ptr][property_count][ref_count][dynamic_map_ptr][property0][property1]...
        void** obj_data = static_cast<void**>(raw_memory);
        
        std::cout << "[DEBUG] __object_create allocated object at " << raw_memory << " (size=" << total_size << ")" << std::endl;
        std::cout.flush();
        
        // Store class name pointer at offset 0
        if (class_name_ptr) {
            GoTSString* name_str = static_cast<GoTSString*>(class_name_ptr);
            obj_data[0] = class_name_ptr; // Store the GoTSString pointer directly
            std::cout << "[DEBUG] __object_create: class_name=\"" << std::string(name_str->data(), name_str->size()) << "\"" << std::endl;
        } else {
            obj_data[0] = nullptr;
            std::cout << "[DEBUG] __object_create: null class_name" << std::endl;
        }
        
        // Store property count at offset 1 (8 bytes)
        obj_data[1] = reinterpret_cast<void*>(property_count);
        
        // Initialize reference count to 1 at offset 2 (16 bytes)
        // Use placement new to properly initialize atomic
        new (reinterpret_cast<char*>(raw_memory) + OBJECT_REF_COUNT_OFFSET) std::atomic<int64_t>(1);
        
        // Initialize dynamic property map pointer to nullptr at offset 3 (24 bytes) - lazy initialization
        obj_data[3] = nullptr;
        
        // Initialize property slots (starting at offset 4, which is 32 bytes)
        for (int64_t i = 0; i < property_count; i++) {
            obj_data[4 + i] = nullptr;
        }
        
        int64_t result = reinterpret_cast<int64_t>(raw_memory);
        std::cout << "[DEBUG] __object_create returning object id: " << result << std::endl;
        
        // Verify object layout is correct
        void** test_ptr = static_cast<void**>(raw_memory);
        std::cout << "[DEBUG] __object_create verification: class_name_ptr=" << test_ptr[0] 
                  << ", property_count=" << reinterpret_cast<int64_t>(test_ptr[1]) 
                  << ", ref_count=" << GET_OBJECT_REF_COUNT(raw_memory).load()
                  << ", dynamic_map_ptr=" << test_ptr[3] << std::endl;
        
        // Test write to property slot 0 (offset 32)
        if (property_count > 0) {
            test_ptr[4] = nullptr; // This should be safe
            std::cout << "[DEBUG] __object_create: Successfully wrote to property slot 0" << std::endl;
        }
        
        std::cout.flush();
        return result;
    } catch (const std::exception& e) {
        std::cout << "[ERROR] __object_create failed: " << e.what() << std::endl;
        std::cout.flush();
        return 0;
    }
}


extern "C" void* __jit_object_create(void* class_name_ptr) {
    // JIT-optimized object creation - for now, same as regular object creation
    int64_t obj_id = __object_create(class_name_ptr, 0);
    void* result = reinterpret_cast<void*>(obj_id);
    
    std::cout << "[DEBUG] __jit_object_create returning pointer: " << result 
              << " (from obj_id: " << obj_id << ")" << std::endl;
    std::cout.flush();
    
    return result;
}

extern "C" void* __jit_object_create_sized(void* class_name_ptr, size_t size) {
    std::cout << "[DEBUG] __jit_object_create_sized called with class_name_ptr=" << class_name_ptr << ", size=" << size << std::endl;
    std::cout.flush();
    
    try {
        // JIT-optimized object creation with known size
        // The size represents the total field size (each field is 8 bytes)
        // So property_count = size / 8
        int64_t property_count = size / 8;
        
        std::cout << "[DEBUG] __jit_object_create_sized calculated property_count=" << property_count << std::endl;
        std::cout.flush();
        
        int64_t obj_id = __object_create(class_name_ptr, property_count);
        std::cout << "[DEBUG] __jit_object_create_sized created object with id=" << obj_id << std::endl;
        std::cout.flush();
        return reinterpret_cast<void*>(obj_id);
    } catch (const std::exception& e) {
        std::cout << "[ERROR] __jit_object_create_sized failed: " << e.what() << std::endl;
        std::cout.flush();
        return nullptr;
    }
}

// ==================== Reference Counting Functions ====================

extern "C" void __object_add_ref(void* object_ptr) {
    if (!object_ptr) return;
    
    std::atomic<int64_t>& ref_count = GET_OBJECT_REF_COUNT(object_ptr);
    int64_t old_count = ref_count.fetch_add(1);
    
    std::cout << "[DEBUG] __object_add_ref: object=" << object_ptr 
              << ", ref_count " << old_count << " -> " << (old_count + 1) << std::endl;
}

extern "C" void __object_release(void* object_ptr) {
    if (!object_ptr) return;
    
    std::atomic<int64_t>& ref_count = GET_OBJECT_REF_COUNT(object_ptr);
    int64_t old_count = ref_count.fetch_sub(1);
    
    std::cout << "[DEBUG] __object_release: object=" << object_ptr 
              << ", ref_count " << old_count << " -> " << (old_count - 1) << std::endl;
    
    if (old_count == 1) {
        // Reference count reached zero, free the object
        std::cout << "[DEBUG] __object_release: freeing object " << object_ptr << std::endl;
        
        // First, clean up the dynamic property map if it exists
        DynamicPropertyMap* dynamic_map = GET_OBJECT_DYNAMIC_MAP(object_ptr);
        if (dynamic_map) {
            dynamic_map->release();  // This will delete the map if its ref count reaches 0
        }
        
        // Destroy the atomic ref_count object
        reinterpret_cast<std::atomic<int64_t>*>(
            reinterpret_cast<char*>(object_ptr) + OBJECT_REF_COUNT_OFFSET)->~atomic();
        
        // Free the object memory
        free(object_ptr);
        
        std::cout << "[DEBUG] __object_release: object freed" << std::endl;
    }
}

extern "C" void __object_destruct(void* object_ptr) {
    if (!object_ptr) return;
    
    std::cout << "[DEBUG] __object_destruct called for object: " << object_ptr << std::endl;
    
    // The ref_count has already been decremented to 0 by the calling code
    // So we just need to clean up and free the object
    
    // First, call the destructor method if it exists
    void* class_name_ptr = GET_OBJECT_CLASS_NAME(object_ptr);
    if (class_name_ptr) {
        GoTSString* name_str = static_cast<GoTSString*>(class_name_ptr);
        if (name_str && name_str->data()) {
            std::string class_name = name_str->c_str();
            std::cout << "[DEBUG] __object_destruct: calling destructor for class " << class_name << std::endl;
            
            // Try to find the destructor method using the method registry with class name
            std::string method_name = "__method_destructor_" + class_name;
            void* method_func = __get_method_address(method_name.c_str());
            if (method_func) {
                std::cout << "[DEBUG] __object_destruct: found destructor at " << method_func << std::endl;
                // Call the destructor method with the object as parameter
                // The method signature should be: void destructor(void* this_ptr)
                typedef void (*DestructorFunc)(void*);
                DestructorFunc destructor = reinterpret_cast<DestructorFunc>(method_func);
                destructor(object_ptr);
            } else {
                std::cout << "[DEBUG] __object_destruct: no destructor method found" << std::endl;
            }
        }
    }
    
    std::cout << "[DEBUG] __object_destruct: freeing object " << object_ptr << std::endl;
    
    // Clean up the dynamic property map if it exists
    DynamicPropertyMap* dynamic_map = GET_OBJECT_DYNAMIC_MAP(object_ptr);
    if (dynamic_map) {
        dynamic_map->release();  // This will delete the map if its ref count reaches 0
    }
    
    // Destroy the atomic ref_count object
    reinterpret_cast<std::atomic<int64_t>*>(
        reinterpret_cast<char*>(object_ptr) + OBJECT_REF_COUNT_OFFSET)->~atomic();
    
    // Free the object memory
    free(object_ptr);
    
    std::cout << "[DEBUG] __object_destruct: object freed" << std::endl;
}

extern "C" void __object_free_direct(void* object_ptr) {
    if (!object_ptr) return;
    
    std::cout << "[DEBUG] __object_free_direct called for object: " << object_ptr << std::endl;
    
    // Direct free without reference counting - used for stack-allocated objects
    // where we know the destructor has already been called directly
    
    // Clean up the dynamic property map if it exists
    DynamicPropertyMap* dynamic_map = GET_OBJECT_DYNAMIC_MAP(object_ptr);
    if (dynamic_map) {
        dynamic_map->release();  // This will delete the map if its ref count reaches 0
    }
    
    // Destroy the atomic ref_count object
    reinterpret_cast<std::atomic<int64_t>*>(
        reinterpret_cast<char*>(object_ptr) + OBJECT_REF_COUNT_OFFSET)->~atomic();
    
    // Free the object memory
    free(object_ptr);
    
    std::cout << "[DEBUG] __object_free_direct: object freed" << std::endl;
}

extern "C" int64_t __object_get_ref_count(void* object_ptr) {
    if (!object_ptr) return 0;
    
    std::atomic<int64_t>& ref_count = GET_OBJECT_REF_COUNT(object_ptr);
    return ref_count.load();
}

// ==================== Advanced Reference Counting for Dynamic Values ====================

extern "C" void __dynamic_value_release_if_object(void* dynamic_value_ptr) {
    if (!dynamic_value_ptr) return;
    
    DynamicValue* dv = static_cast<DynamicValue*>(dynamic_value_ptr);
    
    // Check if the DynamicValue contains a class instance
    if (dv->type == DataType::CLASS_INSTANCE) {
        // Get the object pointer from the variant
        if (std::holds_alternative<void*>(dv->value)) {
            void* object_ptr = std::get<void*>(dv->value);
            if (object_ptr) {
                __object_release(object_ptr);
            }
        }
    }
    
    // Free the DynamicValue itself
    delete dv;
}

extern "C" void* __dynamic_value_copy_with_refcount(void* dynamic_value_ptr) {
    if (!dynamic_value_ptr) return nullptr;
    
    DynamicValue* source_dv = static_cast<DynamicValue*>(dynamic_value_ptr);
    
    // Create a new DynamicValue as a copy
    DynamicValue* new_dv = new DynamicValue(*source_dv);
    
    // If it contains a class instance, increment the reference count
    if (new_dv->type == DataType::CLASS_INSTANCE) {
        if (std::holds_alternative<void*>(new_dv->value)) {
            void* object_ptr = std::get<void*>(new_dv->value);
            if (object_ptr) {
                __object_add_ref(object_ptr);
            }
        }
    }
    
    return new_dv;
}

extern "C" void* __dynamic_value_extract_object_with_refcount(void* dynamic_value_ptr) {
    if (!dynamic_value_ptr) return nullptr;
    
    DynamicValue* dv = static_cast<DynamicValue*>(dynamic_value_ptr);
    
    // Extract object pointer if it's a class instance
    if (dv->type == DataType::CLASS_INSTANCE) {
        if (std::holds_alternative<void*>(dv->value)) {
            void* object_ptr = std::get<void*>(dv->value);
            if (object_ptr) {
                // Increment reference count for the extracted object
                __object_add_ref(object_ptr);
                return object_ptr;
            }
        }
    }
    
    // Return null if not a class instance
    return nullptr;
}

//=============================================================================
// LEGACY FUNCTION SYSTEM REMOVED
// Replaced with compile-time static analysis and direct assembly generation
//=============================================================================

// All function runtime components have been removed for performance:
// - GlobalScopeAddressRegistry (replaced with static analysis)
// - initialize_function_variable (replaced with direct MOV instructions)
// - Function instance runtime creation (replaced with compile-time generation)
// This provides zero runtime overhead and eliminates race conditions.

// ====================================================================================
// RUNTIME SCOPE FUNCTIONS COMPLETELY REMOVED 
// ====================================================================================
//
// All runtime-based scope lookup functions have been removed because they violate
// the new compile-time scope system described in FUNCTION.md.
//
// The following functions are DELETED and should never be called:
// - __register_scope_address_for_depth
// - __get_scope_address_for_depth  
// - __unregister_scope_address_for_depth
//
// Any code generating calls to these functions must be fixed to use the new 
// compile-time approach with hidden parameters.
// ====================================================================================













// DEPRECATED FUNCTION STUBS - These functions were removed but are still referenced by legacy code
// These will be removed once all references are cleaned up

extern "C" void __register_function_code_address(const char* function_name, void* address) {
    // DEPRECATED STUB - Legacy function registration
    (void)function_name; (void)address; // Suppress unused parameter warnings
}

extern "C" void* __get_function_code_address(const char* function_name) {
    // DEPRECATED STUB - Legacy function lookup
    (void)function_name; // Suppress unused parameter warnings
    return nullptr;
}

extern "C" void* __create_function_instance(const char* function_name, void* parent_scope) {
    // DEPRECATED STUB - Legacy function instance creation
    (void)function_name; (void)parent_scope; // Suppress unused parameter warnings
    return nullptr;
}

extern "C" void* __get_function_instance_scope_address(void* function_instance) {
    // DEPRECATED STUB - Legacy scope extraction
    (void)function_instance; // Suppress unused parameter warnings
    return nullptr;
}

extern "C" size_t __get_function_instance_size(void* function_instance) {
    // DEPRECATED STUB - Legacy size lookup
    (void)function_instance; // Suppress unused parameter warnings
    return 0;
}

extern "C" void __register_function_instance_for_patching(void* instance_ptr, const char* function_name, size_t code_addr_offset) {
    // DEPRECATED STUB - Legacy patching system
    (void)instance_ptr; (void)function_name; (void)code_addr_offset; // Suppress unused parameter warnings
}

extern "C" void __patch_all_function_instances(void* executable_memory_base) {
    // DEPRECATED STUB - Legacy patching
    (void)executable_memory_base; // Suppress unused parameter warnings
}

extern "C" void initialize_function_variable(void* scope_ptr, void* value_ptr, size_t value_size, void** function_instances, size_t max_function_instance_size) {
    // DEPRECATED STUB - Legacy variable initialization
    (void)scope_ptr; (void)value_ptr; (void)value_size; (void)function_instances; (void)max_function_instance_size; // Suppress unused parameter warnings
}

// Function call error handling
extern "C" void __throw_function_type_error() {
    std::cout << "[RUNTIME_ERROR] TypeError: Variable is not a function" << std::endl;
    throw std::runtime_error("TypeError: Variable is not a function");
}

// Placeholder for getting current code address during compilation
extern "C" void* __get_current_code_address() {
    // This will be implemented by the code generator to return current code position
    // For now return a placeholder
    return reinterpret_cast<void*>(0x1000000);  // Placeholder address
}

// DynamicValue extraction function for type conversion
extern "C" int64_t __dynamic_value_get_number_bits(void* dv_ptr) {
    std::cout << "[DYNAMIC_VALUE_GET_BITS] Called with pointer: " << dv_ptr << std::endl;
    
    if (!dv_ptr) {
        std::cout << "[DYNAMIC_VALUE_GET_BITS] NULL pointer passed!" << std::endl;
        return 0;
    }
    
    DynamicValue* dv = static_cast<DynamicValue*>(dv_ptr);
    
    std::cout << "[DYNAMIC_VALUE_GET_BITS] Attempting to extract value from DynamicValue at " << dv << std::endl;
    
    // Try to extract numeric value from variant and convert to bit pattern
    double result = 0.0;
    if (std::holds_alternative<double>(dv->value)) {
        result = std::get<double>(dv->value);
        std::cout << "[DYNAMIC_VALUE_GET_BITS] Extracted double: " << result << std::endl;
    } else if (std::holds_alternative<float>(dv->value)) {
        result = static_cast<double>(std::get<float>(dv->value));
        std::cout << "[DYNAMIC_VALUE_GET_BITS] Extracted float as double: " << result << std::endl;
    } else if (std::holds_alternative<int64_t>(dv->value)) {
        result = static_cast<double>(std::get<int64_t>(dv->value));
        std::cout << "[DYNAMIC_VALUE_GET_BITS] Extracted int64 as double: " << result << std::endl;
    } else if (std::holds_alternative<int32_t>(dv->value)) {
        result = static_cast<double>(std::get<int32_t>(dv->value));
        std::cout << "[DYNAMIC_VALUE_GET_BITS] Extracted int32 as double: " << result << std::endl;
    } else {
        std::cout << "[DYNAMIC_VALUE_GET_BITS] WARNING: Could not extract number from DynamicValue, returning 0.0" << std::endl;
    }
    
    // Return the double as bit pattern in integer register
    union {
        double d;
        int64_t bits;
    } converter;
    converter.d = result;
    
    std::cout << "[DYNAMIC_VALUE_GET_BITS] Returning bits: " << converter.bits << " (double: " << result << ")" << std::endl;
    return converter.bits;
}

// Original function for compatibility
extern "C" double __dynamic_value_get_number(void* dv_ptr) {
    int64_t bits = __dynamic_value_get_number_bits(dv_ptr);
    union {
        int64_t bits;
        double d;
    } converter;
    converter.bits = bits;
    return converter.d;
}

// DynamicValue addition function that works with bit patterns
extern "C" void* __dynamic_value_add_bits(int64_t left_bits, int64_t right_bits) {
    std::cout << "[DYNAMIC_VALUE_ADD_BITS] Called with left_bits: " << left_bits << ", right_bits: " << right_bits << std::endl;
    
    // Convert bit patterns to doubles
    union {
        int64_t bits;
        double d;
    } left_conv, right_conv;
    
    left_conv.bits = left_bits;
    right_conv.bits = right_bits;
    
    std::cout << "[DYNAMIC_VALUE_ADD_BITS] Converted to doubles: left=" << left_conv.d << ", right=" << right_conv.d << std::endl;
    
    double result = left_conv.d + right_conv.d;
    
    std::cout << "[DYNAMIC_VALUE_ADD_BITS] Addition result: " << result << std::endl;
    
    // Create new DynamicValue with the result
    DynamicValue* dyn_val = new DynamicValue(result);
    std::cout << "[DYNAMIC_VALUE_ADD_BITS] Created result DynamicValue at: " << dyn_val << std::endl;
    
    return static_cast<void*>(dyn_val);
}
