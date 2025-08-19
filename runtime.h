#pragma once

#include <thread>
#include <vector>
#include <queue>
#include <map>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <future>
#include <cstdint>
#include <cstring>
#include <functional>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <type_traits>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <chrono>
#include <optional>

// Forward declare DataType from compiler.h to avoid circular dependency
enum class DataType;
class LexicalScope;

// Global executable memory info structure
struct ExecutableMemoryInfo {
    void* ptr;
    size_t size;
    std::mutex mutex;
};

// Global executable memory instance
extern ExecutableMemoryInfo g_executable_memory;

// Ultra-Performance Array System - The new unified array implementation
// is now in ultra_performance_array.h and included in compiler.h
// Old TypedArray and LegacyArray implementations have been removed

// High-Performance String Implementation with Small String Optimization (SSO)
// This implements an extremely fast string type optimized for JIT compilation
class GoTSString {
public:
    // SSO threshold - strings up to 22 bytes are stored inline (on 64-bit systems)
    static constexpr size_t SSO_THRESHOLD = sizeof(void*) + sizeof(size_t) + sizeof(size_t) - 1;
    
private:
    union {
        // Large string storage
        struct {
            char* data;
            size_t size;
            size_t capacity;
        } large;
        
        // Small string storage - direct inline storage
        struct {
            char buffer[SSO_THRESHOLD + 1]; // +1 for null terminator
            uint8_t size; // Size fits in 1 byte for small strings
        } small;
    };
    
    // Use the MSB of small.size byte to indicate if it's a small string
    // This avoids overwriting string data in small.buffer[16-23]
    bool is_small() const {
        // Check the high bit of the byte at offset 24 (small.size location)
        return (reinterpret_cast<const uint8_t*>(this)[24] & 0x80) != 0;
    }
    
    void set_small_flag() {
        // Set the high bit of small.size location to indicate small string
        // This leaves small.buffer[16-23] untouched
        reinterpret_cast<uint8_t*>(this)[24] |= 0x80;
    }
    
    void clear_small_flag() {
        // Clear the high bit for large strings
        reinterpret_cast<uint8_t*>(this)[24] &= 0x7F;
    }
    
    uint8_t get_small_size() const {
        // Mask off the flag bit to get actual size (max 127 chars for small strings)
        return small.size & 0x7F;
    }

public:
    // Default constructor - creates empty small string
    GoTSString() noexcept {
        small.buffer[0] = '\0';
        small.size = 0x80;  // Set flag bit + size 0
    }
    
    // Constructor from C string - optimized for literal strings
    GoTSString(const char* str) {
        if (!str) {
            *this = GoTSString();
            return;
        }
        
        size_t len = strlen(str);
        if (len <= SSO_THRESHOLD && len <= 127) {  // Max 127 chars due to flag bit
            // Small string optimization
            memcpy(small.buffer, str, len);
            small.buffer[len] = '\0';
            small.size = static_cast<uint8_t>(len) | 0x80;  // Set flag bit + actual size
        } else {
            // Large string - allocate on heap
            large.size = len;
            large.capacity = ((len + 16) & ~15) | 1; // Round up to 16-byte boundary, set odd flag
            large.data = new char[large.capacity & ~1]; // Mask off the flag bit
            memcpy(large.data, str, len + 1);
            clear_small_flag();  // Make sure flag is clear for large strings
        }
    }
    
    // Constructor from data pointer and length - handles null bytes properly
    GoTSString(const char* data, size_t len) {
        if (!data || len == 0) {
            *this = GoTSString();
            return;
        }
        
        if (len <= SSO_THRESHOLD && len <= 127) {  // Max 127 chars due to flag bit
            // Small string optimization
            memcpy(small.buffer, data, len);
            small.buffer[len] = '\0';  // Always null-terminate for safety
            small.size = static_cast<uint8_t>(len) | 0x80;  // Set flag bit + actual size
        } else {
            // Large string - allocate on heap
            large.size = len;
            large.capacity = ((len + 16) & ~15) | 1; // Round up to 16-byte boundary, set odd flag
            large.data = new char[large.capacity & ~1]; // Mask off the flag bit
            memcpy(large.data, data, len);
            large.data[len] = '\0';  // Always null-terminate for safety
            clear_small_flag();  // Make sure flag is clear for large strings
        }
    }
    
    // Copy constructor
    GoTSString(const GoTSString& other) {
        if (other.is_small()) {
            memcpy(&small, &other.small, sizeof(small));
        } else {
            large.size = other.large.size;
            large.capacity = other.large.capacity;
            size_t actual_capacity = large.capacity & ~1;
            large.data = new char[actual_capacity];
            memcpy(large.data, other.large.data, large.size + 1);
        }
    }
    
    // Move constructor
    GoTSString(GoTSString&& other) noexcept {
        if (other.is_small()) {
            // Safe copy for small strings
            memcpy(small.buffer, other.small.buffer, sizeof(small.buffer));
            small.size = other.small.size;
        } else {
            // Transfer ownership of large string
            large.data = other.large.data;
            large.size = other.large.size;
            large.capacity = other.large.capacity;
            clear_small_flag();
            
            // Reset other to empty small string
            other.large.data = nullptr;
        }
        
        // Reset other to empty small string
        other.small.buffer[0] = '\0';
        other.small.size = 0x80;  // Set flag bit + size 0
    }
    
    // Assignment operators
    GoTSString& operator=(const GoTSString& other) {
        if (this != &other) {
            this->~GoTSString();
            new (this) GoTSString(other);
        }
        return *this;
    }
    
    GoTSString& operator=(GoTSString&& other) noexcept {
        if (this != &other) {
            this->~GoTSString();
            if (other.is_small()) {
                // Safe copy for small strings
                memcpy(small.buffer, other.small.buffer, sizeof(small.buffer));
                small.size = other.small.size;
            } else {
                // Transfer ownership of large string
                large.data = other.large.data;
                large.size = other.large.size;
                large.capacity = other.large.capacity;
                clear_small_flag();
                
                // Reset other to empty small string
                other.large.data = nullptr;
            }
            
            // Reset other to empty small string
            other.small.buffer[0] = '\0';
            other.small.size = 0x80;  // Set flag bit + size 0
        }
        return *this;
    }
    
    // Destructor
    ~GoTSString() {
        if (!is_small()) {
            delete[] large.data;
        }
    }
    
    // Access methods - highly optimized
    inline const char* c_str() const {
        return is_small() ? small.buffer : large.data;
    }
    
    inline size_t size() const {
        return is_small() ? get_small_size() : large.size;
    }
    
    inline size_t length() const {
        return size();
    }
    
    inline bool empty() const {
        return size() == 0;
    }
    
    inline char operator[](size_t index) const {
        return is_small() ? small.buffer[index] : large.data[index];
    }
    
    // Concatenation - extremely optimized
    GoTSString operator+(const GoTSString& other) const {
        size_t total_size = size() + other.size();
        GoTSString result;
        
        if (total_size <= SSO_THRESHOLD && total_size <= 127) {
            // Result fits in small string
            memcpy(result.small.buffer, c_str(), size());
            memcpy(result.small.buffer + size(), other.c_str(), other.size());
            result.small.buffer[total_size] = '\0';
            result.small.size = static_cast<uint8_t>(total_size) | 0x80;  // Set flag bit + size
        } else {
            // Need large string
            result.large.size = total_size;
            result.large.capacity = ((total_size + 16) & ~15) | 1;
            result.large.data = new char[result.large.capacity & ~1];
            memcpy(result.large.data, c_str(), size());
            memcpy(result.large.data + size(), other.c_str(), other.size());
            result.large.data[total_size] = '\0';
            result.clear_small_flag();  // Make sure flag is clear for large strings
        }
        
        return result;
    }
    
    // Comparison operators - optimized for JIT
    bool operator==(const GoTSString& other) const {
        size_t len = size();
        if (len != other.size()) return false;
        if (len == 0) return true;
        
        // For small strings, we can use optimized comparison
        if (is_small() && other.is_small()) {
            return memcmp(small.buffer, other.small.buffer, len) == 0;
        }
        
        return memcmp(c_str(), other.c_str(), len) == 0;
    }
    
    bool operator!=(const GoTSString& other) const {
        return !(*this == other);
    }
    
    bool operator<(const GoTSString& other) const {
        int cmp = strcmp(c_str(), other.c_str());
        return cmp < 0;
    }
    
    bool operator>(const GoTSString& other) const {
        return other < *this;
    }
    
    bool operator<=(const GoTSString& other) const {
        return !(other < *this);
    }
    
    bool operator>=(const GoTSString& other) const {
        return !(*this < other);
    }
    
    // Raw data access for handling null bytes properly
    inline const char* data() const {
        return is_small() ? small.buffer : large.data;
    }
    
    // Static factory method for creating from data with length
    static GoTSString from_data(const char* data, size_t len) {
        return GoTSString(data, len);
    }
    
    // Static factory method for creating from C string
    static GoTSString from_cstr(const char* str) {
        return GoTSString(str);
    }
    
    // Concatenation operator that handles null bytes properly
    GoTSString& operator+=(const GoTSString& other) {
        size_t my_size = size();
        size_t other_size = other.size();
        size_t total_size = my_size + other_size;
        
        if (total_size <= SSO_THRESHOLD && total_size <= 127 && is_small()) {
            // Can extend small string
            memcpy(small.buffer + my_size, other.data(), other_size);
            small.buffer[total_size] = '\0';
            small.size = static_cast<uint8_t>(total_size) | 0x80;
        } else {
            // Need to create new large string or grow existing one
            char* new_data = new char[((total_size + 16) & ~15)];
            memcpy(new_data, data(), my_size);
            memcpy(new_data + my_size, other.data(), other_size);
            new_data[total_size] = '\0';
            
            if (!is_small()) {
                delete[] large.data;
            }
            
            large.data = new_data;
            large.size = total_size;
            large.capacity = ((total_size + 16) & ~15) | 1;
            clear_small_flag();
        }
        
        return *this;
    }
};

// String interning system for literal optimization
class StringPool {
private:
    std::unordered_map<std::string, GoTSString*> interned_strings;
    std::mutex pool_mutex;
    
public:
    // Get or create an interned string
    GoTSString* intern(const char* str) {
        if (!str) return nullptr;
        
        std::lock_guard<std::mutex> lock(pool_mutex);
        
        std::string key(str);  // Create proper string key
        auto it = interned_strings.find(key);
        if (it != interned_strings.end()) {
            return it->second;
        }
        
        GoTSString* new_string = new GoTSString(str);
        interned_strings[key] = new_string;  // Use string key, not char* key
        return new_string;
    }
    
    // Cleanup - called on program exit
    ~StringPool() {
        for (auto& pair : interned_strings) {
            delete pair.second;
        }
    }
};

// Global string pool instance
extern StringPool global_string_pool;

struct Promise {
    std::atomic<bool> resolved{false};
    std::shared_ptr<void> value;
    std::vector<std::function<void()>> callbacks;
    std::mutex callback_mutex;
    
    template<typename T>
    void resolve(T&& val) {
        {
            std::lock_guard<std::mutex> lock(callback_mutex);
            using ValueType = typename std::remove_reference<T>::type;
            value = std::make_shared<ValueType>(std::forward<T>(val));
            resolved.store(true);
        }
        
        for (auto& callback : callbacks) {
            callback();
        }
    }
    
    template<typename T>
    T await() {
        while (!resolved.load()) {
            std::this_thread::yield();
        }
        using ValueType = typename std::remove_reference<T>::type;
        return *static_cast<ValueType*>(value.get());
    }
    
    void then(std::function<void()> callback) {
        std::lock_guard<std::mutex> lock(callback_mutex);
        if (resolved.load()) {
            callback();
        } else {
            callbacks.push_back(callback);
        }
    }
};

// Forward declaration - using new goroutine system
class GoroutineScheduler;

class ThreadPool {
    
private:
    std::vector<std::thread> workers;
    std::queue<std::function<void()>> tasks;
    std::mutex queue_mutex;
    std::condition_variable condition;
    std::atomic<bool> stop{false};
    
public:
    ThreadPool(size_t num_threads = std::thread::hardware_concurrency());
    ~ThreadPool();
    
    template<typename F, typename... Args>
    auto enqueue(F&& f, Args&&... args) -> std::future<typename std::result_of<F(Args...)>::type>;
    
    // Simple non-template enqueue for basic tasks
    void enqueue_simple(std::function<void()> task);
    
    void shutdown();
};

// Old GoroutineScheduler removed - using new system from goroutine_system.h

class SharedMemory {
private:
    std::unordered_map<std::string, std::shared_ptr<void>> global_vars;
    std::unordered_map<std::string, std::mutex> var_mutexes;
    std::mutex memory_mutex;
    
public:
    template<typename T>
    void set_global(const std::string& name, T&& value);
    
    template<typename T>
    T get_global(const std::string& name);
    
    template<typename T>
    void atomic_update(const std::string& name, std::function<T(const T&)> updater);
};

// Object instance structure removed - property access system redesigned according to CLAUDE.md
// The new system will support zero-cost property access with direct offsets

// High-Performance Date Implementation
// JavaScript-compatible Date class with optimized internal representation
class GoTSDate {
private:
    // Internal representation: milliseconds since Unix epoch (UTC)
    // This matches JavaScript's Date internal representation for perfect compatibility
    int64_t time_value;
    
    // Cached timezone offset to avoid repeated system calls
    mutable int64_t cached_timezone_offset;
    mutable bool timezone_offset_cached;
    
    // Internal helper methods
    void update_timezone_cache() const;
    void normalize_time_components(int64_t& year, int64_t& month, int64_t& day, 
                                  int64_t& hour, int64_t& minute, int64_t& second) const;
    int64_t days_since_epoch(int64_t year, int64_t month, int64_t day) const;
    void time_to_components(int64_t time, bool use_utc, int64_t& year, int64_t& month, 
                           int64_t& day, int64_t& hour, int64_t& minute, int64_t& second, 
                           int64_t& millisecond) const;
    int64_t components_to_time(int64_t year, int64_t month, int64_t day, 
                              int64_t hour, int64_t minute, int64_t second, 
                              int64_t millisecond, bool use_utc) const;
    
public:
    // Static helper methods (made public for external access)
    static bool is_leap_year(int64_t year);
    static int64_t days_in_month(int64_t year, int64_t month);
    static int64_t day_of_week(int64_t year, int64_t month, int64_t day);
    
private:
    static int64_t parse_iso_string(const char* str);
    static GoTSString* format_iso_string(int64_t time);
    static GoTSString* format_date_string(int64_t time, bool use_utc);
    
public:
    // Constructors
    GoTSDate();
    explicit GoTSDate(int64_t millis);
    GoTSDate(int64_t year, int64_t month, int64_t day = 1, 
             int64_t hour = 0, int64_t minute = 0, int64_t second = 0, 
             int64_t millisecond = 0);
    explicit GoTSDate(const char* dateString);
    GoTSDate(const char* dateString, const char* timezone);
    
    // Copy constructor and assignment
    GoTSDate(const GoTSDate& other) = default;
    GoTSDate& operator=(const GoTSDate& other) = default;
    
    // Destructor
    ~GoTSDate() = default;
    
    // Core time methods
    int64_t getTime() const { return time_value; }
    int64_t setTime(int64_t time);
    int64_t valueOf() const { return time_value; }
    
    // Local time getters
    int64_t getFullYear() const;
    int64_t getMonth() const;
    int64_t getDate() const;
    int64_t getDay() const;
    int64_t getHours() const;
    int64_t getMinutes() const;
    int64_t getSeconds() const;
    int64_t getMilliseconds() const;
    int64_t getTimezoneOffset() const;
    
    // UTC time getters
    int64_t getUTCFullYear() const;
    int64_t getUTCMonth() const;
    int64_t getUTCDate() const;
    int64_t getUTCDay() const;
    int64_t getUTCHours() const;
    int64_t getUTCMinutes() const;
    int64_t getUTCSeconds() const;
    int64_t getUTCMilliseconds() const;
    
    // Local time setters
    int64_t setFullYear(int64_t year, int64_t month = -1, int64_t day = -1);
    int64_t setMonth(int64_t month, int64_t day = -1);
    int64_t setDate(int64_t day);
    int64_t setHours(int64_t hours, int64_t minutes = -1, int64_t seconds = -1, int64_t milliseconds = -1);
    int64_t setMinutes(int64_t minutes, int64_t seconds = -1, int64_t milliseconds = -1);
    int64_t setSeconds(int64_t seconds, int64_t milliseconds = -1);
    int64_t setMilliseconds(int64_t milliseconds);
    
    // UTC time setters
    int64_t setUTCFullYear(int64_t year, int64_t month = -1, int64_t day = -1);
    int64_t setUTCMonth(int64_t month, int64_t day = -1);
    int64_t setUTCDate(int64_t day);
    int64_t setUTCHours(int64_t hours, int64_t minutes = -1, int64_t seconds = -1, int64_t milliseconds = -1);
    int64_t setUTCMinutes(int64_t minutes, int64_t seconds = -1, int64_t milliseconds = -1);
    int64_t setUTCSeconds(int64_t seconds, int64_t milliseconds = -1);
    int64_t setUTCMilliseconds(int64_t milliseconds);
    
    // String conversion methods
    GoTSString* toString() const;
    GoTSString* toISOString() const;
    GoTSString* toUTCString() const;
    GoTSString* toDateString() const;
    GoTSString* toTimeString() const;
    GoTSString* toLocaleDateString() const;
    GoTSString* toLocaleTimeString() const;
    GoTSString* toLocaleString() const;
    GoTSString* toJSON() const;
    
    // Deprecated methods (required for ECMAScript compliance)
    int64_t getYear() const;
    int64_t setYear(int64_t year);
    GoTSString* toGMTString() const;
    
    // Moment.js-like methods for enhanced date manipulation
    GoTSDate add(int64_t value, const char* unit) const;
    GoTSDate subtract(int64_t value, const char* unit) const;
    bool isBefore(const GoTSDate& other) const;
    bool isAfter(const GoTSDate& other) const;
    GoTSDate clone() const;
    GoTSString* format(const char* formatStr) const;
    
    // Static methods
    static int64_t now();
    static int64_t parse(const char* dateString);
    static int64_t UTC(int64_t year, int64_t month, int64_t day = 1, 
                       int64_t hour = 0, int64_t minute = 0, int64_t second = 0, 
                       int64_t millisecond = 0);
    
    // Comparison operators
    bool operator==(const GoTSDate& other) const { return time_value == other.time_value; }
    bool operator!=(const GoTSDate& other) const { return time_value != other.time_value; }
    bool operator<(const GoTSDate& other) const { return time_value < other.time_value; }
    bool operator<=(const GoTSDate& other) const { return time_value <= other.time_value; }
    bool operator>(const GoTSDate& other) const { return time_value > other.time_value; }
    bool operator>=(const GoTSDate& other) const { return time_value >= other.time_value; }
    
    // Arithmetic operators (returns new Date)
    GoTSDate operator+(int64_t milliseconds) const;
    GoTSDate operator-(int64_t milliseconds) const;
    int64_t operator-(const GoTSDate& other) const;
    
    // In-place arithmetic
    GoTSDate& operator+=(int64_t milliseconds);
    GoTSDate& operator-=(int64_t milliseconds);
    
    // Validation
    bool isValid() const;
    static bool isValidDate(int64_t year, int64_t month, int64_t day);
    static bool isValidTime(int64_t hour, int64_t minute, int64_t second, int64_t millisecond);
};

// Global object registry
// Object registry removed - property access system redesigned according to CLAUDE.md
extern std::atomic<int64_t> next_object_id;

// High-Performance Function Registry System
// Replaces slow string-based lookups with direct ID-based access
struct FunctionEntry {
    void* func_ptr;
    uint16_t arg_count;
    uint8_t calling_convention;  // 0=void(), 1=int64_t(int64_t), etc.
    uint8_t flags;               // Future use
};

// Direct array access - O(1) lookup instead of O(log n) hash table
// Maximum 65536 functions should be enough for any reasonable program
constexpr size_t MAX_FUNCTIONS = 65536;
extern FunctionEntry g_function_table[MAX_FUNCTIONS];
extern std::atomic<uint16_t> g_next_function_id;

extern "C" {
    // High-Performance Function Registration - O(1) access
    uint16_t __register_function_fast(void* func_ptr, uint16_t arg_count, uint8_t calling_convention);
    void* __lookup_function_fast(uint16_t func_id);
    
    // Optimized goroutine spawn with direct function IDs
    void* __goroutine_spawn_fast(uint16_t func_id);
    void* __goroutine_spawn_fast_arg1(uint16_t func_id, int64_t arg1);
    void* __goroutine_spawn_fast_arg2(uint16_t func_id, int64_t arg1, int64_t arg2);
    
    // Core system functions
    void __set_goroutine_context(int64_t is_goroutine);
    void __promise_resolve(void* promise, void* value);
    void* __promise_await(void* promise);
    void __promise_destroy(void* promise);
    void __runtime_init();
    void __runtime_cleanup();
    void __runtime_timer_cleanup();
    void __set_executable_memory(void* ptr, size_t size);
    void* __get_executable_memory_base();
    
    // Main thread JIT execution queue for thread safety
    struct JITExecutionRequest {
        void* func_ptr;
        std::vector<int64_t> args;
        std::promise<int64_t> result_promise;
    };
    
    void __execute_jit_on_main_thread(void* func_ptr, const std::vector<int64_t>& args, std::promise<int64_t>& result_promise);
    void __process_jit_queue();
    
    // Console functions
    void __console_log(const char* message);
    void __console_log_gots(void* gots_string_ptr);
    void __console_log_newline();
    void __console_log_space();
    void __console_log_array(int64_t* array, int64_t size);
    void __console_log_number(int64_t value);
    void __console_log_double_bits(int64_t double_bits);
    void __console_log_universal(int64_t value);
    void __console_log_auto(int64_t value);
    void __console_log_smart(int64_t value);
    const char* __gots_string_to_cstr(void* gots_string_ptr);
    bool __is_array_pointer(int64_t value);
    void __console_time(void* label_ptr);
    void __console_timeEnd(void* label_ptr);
    
    // String functions - updated to use GoTSString
    void* __string_create(const char* str);
    void* __string_intern(const char* str);
    void* __string_create_with_length(const char* data, size_t length);
    void* __string_create_from_std_string(const std::string& str);
    size_t __string_length(void* string_ptr);
    const char* __string_data(void* string_ptr);
    void* __string_concat(void* str1, void* str2);
    void __console_log_string(void* string_ptr);
    
    // Type-specific console.log functions - new type-aware system
    void __console_log_int8(int8_t value);
    void __console_log_int16(int16_t value);
    void __console_log_int32(int32_t value);
    void __console_log_int64(int64_t value);
    void __console_log_uint8(uint8_t value);
    void __console_log_uint16(uint16_t value);
    void __console_log_uint32(uint32_t value);
    void __console_log_uint64(uint64_t value);
    void __console_log_float32(float value);
    void __console_log_float64(double value);
    void __console_log_boolean(bool value);
    void __console_log_string_ptr(void* string_ptr);
    void __console_log_array_ptr(void* array_ptr);
    void __console_log_object_ptr(void* object_ptr);
    void __console_log_function_ptr(void* function_ptr);
    void __console_log_space_separator();
    void __console_log_final_newline();
    void __console_log_any_value_inspect(void* dynamic_value_ptr);
    
    // DynamicValue allocation functions for ANY type variables
    void* __dynamic_value_create_from_double(int64_t double_bits);
    void* __dynamic_value_create_from_int64(int64_t value);
    void* __dynamic_value_create_from_bool(bool value);
    void* __dynamic_value_create_from_string(void* string_ptr);
    
    // Promise functions
    void* __promise_all(void* promises_array);
    
    // Legacy Array functions
    void* __array_create(int64_t initial_capacity);
    void __array_push(void* array, int64_t value);
    int64_t __array_pop(void* array);
    int64_t __array_size(void* array);
    int64_t __array_access(void* array, int64_t index);
    
    // Typed array access functions for optimal performance
    int64_t __array_access_int64(void* array, int64_t index);
    int64_t __array_access_float64(void* array, int64_t index);
    int64_t __array_access_int32(void* array, int64_t index);
    int64_t __array_access_float32(void* array, int64_t index);
    
    int64_t* __array_data(void* array);
    
    // Type-aware array creation functions - know type at creation time
    void* __array_create_dynamic(int64_t size);
    void* __array_create_int64(int64_t size);
    void* __array_create_float64(int64_t size);
    void* __array_create_int32(int64_t size);
    void* __array_create_float32(int64_t size);
    
    // Type-aware array push functions - no type guessing needed
    void __array_push_dynamic(void* array, int64_t value_bits);
    void __array_push_int64_typed(void* array, int64_t value);
    void __array_push_float64_typed(void* array, double value);
    void __array_push_int32_typed(void* array, int32_t value);
    void __array_push_float32_typed(void* array, float value);
    
    // Array factory functions for specific use cases
    void* __array_zeros_typed(int64_t size, void* dtype_string);
    void* __array_ones_dynamic(int64_t size);
    void* __array_ones_int64(int64_t size);
    void* __array_ones_float64(int64_t size);
    void* __array_ones_int32(int64_t size);
    void* __array_ones_float32(int64_t size);
    
    // Typed Array functions for maximum performance
    void* __typed_array_create_int32(int64_t initial_capacity);
    void* __typed_array_create_int64(int64_t initial_capacity);
    void* __typed_array_create_float32(int64_t initial_capacity);
    void* __typed_array_create_float64(int64_t initial_capacity);
    void* __typed_array_create_uint8(int64_t initial_capacity);
    void* __typed_array_create_uint16(int64_t initial_capacity);
    void* __typed_array_create_uint32(int64_t initial_capacity);
    void* __typed_array_create_uint64(int64_t initial_capacity);
    
    // Push operations for each type
    void __typed_array_push_int32(void* array, int32_t value);
    void __typed_array_push_int64(void* array, int64_t value);
    void __typed_array_push_float32(void* array, float value);
    void __typed_array_push_float64(void* array, double value);
    void __typed_array_push_uint8(void* array, uint8_t value);
    void __typed_array_push_uint16(void* array, uint16_t value);
    void __typed_array_push_uint32(void* array, uint32_t value);
    void __typed_array_push_uint64(void* array, uint64_t value);
    
    // Pop operations for each type
    int32_t __typed_array_pop_int32(void* array);
    int64_t __typed_array_pop_int64(void* array);
    float __typed_array_pop_float32(void* array);
    double __typed_array_pop_float64(void* array);
    uint8_t __typed_array_pop_uint8(void* array);
    uint16_t __typed_array_pop_uint16(void* array);
    uint32_t __typed_array_pop_uint32(void* array);
    uint64_t __typed_array_pop_uint64(void* array);
    
    // Array access operations for each type (direct indexing)
    int32_t __typed_array_get_int32(void* array, int64_t index);
    int64_t __typed_array_get_int64(void* array, int64_t index);
    float __typed_array_get_float32(void* array, int64_t index);
    double __typed_array_get_float64(void* array, int64_t index);
    uint8_t __typed_array_get_uint8(void* array, int64_t index);
    uint16_t __typed_array_get_uint16(void* array, int64_t index);
    uint32_t __typed_array_get_uint32(void* array, int64_t index);
    uint64_t __typed_array_get_uint64(void* array, int64_t index);
    
    void __typed_array_set_int32(void* array, int64_t index, int32_t value);
    void __typed_array_set_int64(void* array, int64_t index, int64_t value);
    void __typed_array_set_float32(void* array, int64_t index, float value);
    void __typed_array_set_float64(void* array, int64_t index, double value);
    void __typed_array_set_uint8(void* array, int64_t index, uint8_t value);
    void __typed_array_set_uint16(void* array, int64_t index, uint16_t value);
    void __typed_array_set_uint32(void* array, int64_t index, uint32_t value);
    void __typed_array_set_uint64(void* array, int64_t index, uint64_t value);
    
    // Size and data access
    int64_t __typed_array_size(void* array);
    void* __typed_array_raw_data(void* array);
    
    // Console logging for typed arrays
    void __console_log_typed_array_int32(void* array);
    void __console_log_typed_array_int64(void* array);
    void __console_log_typed_array_float32(void* array);
    void __console_log_typed_array_float64(void* array);
    
    // Object management functions
    int64_t __object_create(void* class_name_ptr, int64_t property_count);
    void __object_set_property(int64_t object_id, int64_t property_index, int64_t value);
    int64_t __object_get_property(int64_t object_id, int64_t property_index);
    void __object_destroy(int64_t object_id);
    void* __jit_object_create(void* class_name_ptr);
    void* __jit_object_create_sized(void* class_name_ptr, size_t size);
    
    // Dynamic property access (for runtime property access)
    void __dynamic_set_property(int64_t object_id, const char* property_name, int64_t value);
    void* __dynamic_get_property(void* dynamic_value_ptr, const char* property_name);
    
    // Property name management for iteration
    void __object_set_property_name(int64_t object_id, int64_t property_index, const char* property_name);
    const char* __object_get_property_name(int64_t object_id, int64_t property_index);
    
    // Method calling
    int64_t __object_call_method(int64_t object_id, const char* method_name, int64_t* args, int64_t arg_count);
    
    // Static property management
    void __static_set_property(const char* class_name, const char* property_name, int64_t value);
    int64_t __static_get_property(const char* class_name, const char* property_name);
    
    // Inheritance support
    void __register_class_inheritance(const char* child_class, const char* parent_class);
    void __super_constructor_call(int64_t object_id, int64_t arg1, int64_t arg2, int64_t arg3, int64_t arg4, int64_t arg5);
    
    // Mathematical functions
    int64_t __runtime_pow(int64_t base, int64_t exponent);
    int64_t __runtime_js_equal(int64_t left_value, int64_t left_type, int64_t right_value, int64_t right_type);
    
    // High-Performance String Runtime Functions
    void* __string_create(const char* str);
    void* __string_create_empty();
    void __string_destroy(void* string_ptr);
    
    // String operations - extremely optimized
    void* __string_concat(void* str1, void* str2);
    void* __string_concat_cstr(void* str1, const char* str2);
    void* __string_concat_cstr_left(const char* str1, void* str2);
    
    // String comparison - JIT optimized
    bool __string_equals(void* str1, void* str2);
    bool __string_equals_cstr(void* str1, const char* str2);
    
    // Array functions
    void* __array_create(int64_t initial_capacity);
    void __array_push(void* array, int64_t value);
    int64_t __array_pop(void* array);
    int64_t __array_size(void* array);
    int64_t __array_access(void* array, int64_t index);
    int64_t* __array_data(void* array);
    int64_t __string_compare(void* str1, void* str2);
    
    // String access (legacy - use new string functions above)
    const char* __string_c_str(void* string_ptr);
    char __string_char_at(void* string_ptr, int64_t index);
    
    // String pool functions for literal optimization
    void* __string_intern(const char* str);
    void __string_pool_cleanup();
    
    // Console logging optimized for strings
    void __console_log_string(void* string_ptr);
    void __console_log_object(int64_t object_id);
    
    // Date/Time functions
    int64_t __date_now();
    
    // Date class functions
    void* __date_create();
    void* __date_create_from_millis(int64_t millis);
    void* __date_create_from_components(int64_t year, int64_t month, int64_t day,
                                       int64_t hour, int64_t minute, int64_t second, int64_t millisecond);
    void* __date_create_from_string(const char* dateString);
    void* __date_create_from_string_with_timezone(const char* dateString, const char* timezone);
    void __date_destroy(void* date_ptr);
    
    // Date getter methods
    int64_t __date_getTime(void* date_ptr);
    int64_t __date_getFullYear(void* date_ptr);
    int64_t __date_getMonth(void* date_ptr);
    int64_t __date_getDate(void* date_ptr);
    int64_t __date_getDay(void* date_ptr);
    int64_t __date_getHours(void* date_ptr);
    int64_t __date_getMinutes(void* date_ptr);
    int64_t __date_getSeconds(void* date_ptr);
    int64_t __date_getMilliseconds(void* date_ptr);
    int64_t __date_getTimezoneOffset(void* date_ptr);
    
    // Date UTC getter methods
    int64_t __date_getUTCFullYear(void* date_ptr);
    int64_t __date_getUTCMonth(void* date_ptr);
    int64_t __date_getUTCDate(void* date_ptr);
    int64_t __date_getUTCDay(void* date_ptr);
    int64_t __date_getUTCHours(void* date_ptr);
    int64_t __date_getUTCMinutes(void* date_ptr);
    int64_t __date_getUTCSeconds(void* date_ptr);
    int64_t __date_getUTCMilliseconds(void* date_ptr);
    
    // Date setter methods
    int64_t __date_setTime(void* date_ptr, int64_t time);
    int64_t __date_setFullYear(void* date_ptr, int64_t year, int64_t month, int64_t day);
    int64_t __date_setMonth(void* date_ptr, int64_t month, int64_t day);
    int64_t __date_setDate(void* date_ptr, int64_t day);
    int64_t __date_setHours(void* date_ptr, int64_t hours, int64_t minutes, int64_t seconds, int64_t milliseconds);
    int64_t __date_setMinutes(void* date_ptr, int64_t minutes, int64_t seconds, int64_t milliseconds);
    int64_t __date_setSeconds(void* date_ptr, int64_t seconds, int64_t milliseconds);
    int64_t __date_setMilliseconds(void* date_ptr, int64_t milliseconds);
    
    // Date UTC setter methods
    int64_t __date_setUTCFullYear(void* date_ptr, int64_t year, int64_t month, int64_t day);
    int64_t __date_setUTCMonth(void* date_ptr, int64_t month, int64_t day);
    int64_t __date_setUTCDate(void* date_ptr, int64_t day);
    int64_t __date_setUTCHours(void* date_ptr, int64_t hours, int64_t minutes, int64_t seconds, int64_t milliseconds);
    int64_t __date_setUTCMinutes(void* date_ptr, int64_t minutes, int64_t seconds, int64_t milliseconds);
    int64_t __date_setUTCSeconds(void* date_ptr, int64_t seconds, int64_t milliseconds);
    int64_t __date_setUTCMilliseconds(void* date_ptr, int64_t milliseconds);
    
    // Date string methods
    void* __date_toString(void* date_ptr);
    void* __date_toISOString(void* date_ptr);
    void* __date_toUTCString(void* date_ptr);
    void* __date_toDateString(void* date_ptr);
    void* __date_toTimeString(void* date_ptr);
    void* __date_toLocaleDateString(void* date_ptr);
    void* __date_toLocaleTimeString(void* date_ptr);
    void* __date_toLocaleString(void* date_ptr);
    void* __date_toJSON(void* date_ptr);
    
    // Deprecated Date methods (required for ECMAScript compliance)
    int64_t __date_getYear(void* date_ptr);
    int64_t __date_setYear(void* date_ptr, int64_t year);
    void* __date_toGMTString(void* date_ptr);
    
    // Date function call (not constructor) - returns string representation
    void* __date_call();
    
    // Date constructor functions for JIT integration
    void* __constructor_Date();
    void* __constructor_Date_1(int64_t arg1);
    void* __constructor_Date_3(int64_t year, int64_t month, int64_t day);
    void* __constructor_Date_7(int64_t year, int64_t month, int64_t day, int64_t hour, int64_t minute, int64_t second, int64_t millisecond);
    
    // Moment.js-like methods for enhanced date manipulation
    void* __date_add(void* date_ptr, int64_t value, const char* unit);
    void* __date_subtract(void* date_ptr, int64_t value, const char* unit);
    bool __date_isBefore(void* date_ptr, void* other_ptr);
    bool __date_isAfter(void* date_ptr, void* other_ptr);
    void* __date_clone(void* date_ptr);
    void* __date_format(void* date_ptr, const char* formatStr);
    
    // Date static methods
    int64_t __date_parse(const char* dateString);
    int64_t __date_UTC(int64_t year, int64_t month, int64_t day, int64_t hour, int64_t minute, int64_t second, int64_t millisecond);
    
    // Date comparison and arithmetic
    int64_t __date_valueOf(void* date_ptr);
    bool __date_equals(void* date1_ptr, void* date2_ptr);
    int64_t __date_compare(void* date1_ptr, void* date2_ptr);
    
    // Regex functions
    void* __regex_create(const char* pattern, const char* flags);
    void* __regex_create_simple(const char* pattern);
    void* __regex_create_by_id(int64_t pattern_id);
    void __regex_destroy(void* regex_ptr);
    bool __regex_test(void* regex_ptr, const char* text);
    void* __regex_exec(void* regex_ptr, const char* text);
    void* __regex_match_all(void* regex_ptr, const char* text);
    
    // String method functions
    void* __string_match(void* string_ptr, void* regex_ptr);
    void* __string_replace(void* string_ptr, void* pattern_ptr, void* replacement_ptr);
    int64_t __string_search(void* string_ptr, void* regex_ptr);
    void* __string_split(void* string_ptr, void* delimiter_ptr);
    
    // Regex property functions
    void* __regex_get_source(void* regex_ptr);
    bool __regex_get_global(void* regex_ptr);
    bool __regex_get_ignore_case(void* regex_ptr);
    
    // Debug functions
    void __debug_print_pointer(void* ptr);
    
    // JSON functions
    void* __static_stringify(void* value, int64_t type);
    
    // Pattern registry
    void* __register_regex_pattern(void* pattern_ptr);
}

template<typename F, typename... Args>
auto ThreadPool::enqueue(F&& f, Args&&... args) -> std::future<typename std::result_of<F(Args...)>::type> {
    using return_type = typename std::result_of<F(Args...)>::type;
    
    // Create a simple task wrapper instead of packaged_task
    auto promise = std::make_shared<std::promise<return_type>>();
    auto future = promise->get_future();
    
    // Fix: Create a bound function to avoid problematic variadic capture
    auto bound_func = std::bind(std::forward<F>(f), std::forward<Args>(args)...);
    
    // Create a simpler task that uses std::bind instead of lambda capture
    auto simple_task = [promise, bound_func = std::move(bound_func)]() mutable {
        try {
            if constexpr (std::is_void_v<return_type>) {
                bound_func();
                promise->set_value();
            } else {
                auto result = bound_func();
                promise->set_value(result);
            }
        } catch (...) {
            promise->set_exception(std::current_exception());
        }
    };
    
    {
        std::unique_lock<std::mutex> lock(queue_mutex);
        
        if (stop.load()) {
            throw std::runtime_error("ThreadPool is stopped");
        }
        
        tasks.emplace(std::move(simple_task));
    }
    
    condition.notify_one();
    return future;
}

// Old GoroutineScheduler template implementations removed - using new system

template<typename T>
void SharedMemory::set_global(const std::string& name, T&& value) {
    std::lock_guard<std::mutex> lock(memory_mutex);
    global_vars[name] = std::make_shared<T>(std::forward<T>(value));
}

template<typename T>
T SharedMemory::get_global(const std::string& name) {
    std::lock_guard<std::mutex> lock(memory_mutex);
    auto it = global_vars.find(name);
    if (it != global_vars.end()) {
        return *static_cast<T*>(it->second.get());
    }
    return T{};
}

template<typename T>
void SharedMemory::atomic_update(const std::string& name, std::function<T(const T&)> updater) {
    std::lock_guard<std::mutex> lock(memory_mutex);
    std::lock_guard<std::mutex> var_lock(var_mutexes[name]);
    
    auto it = global_vars.find(name);
    if (it != global_vars.end()) {
        T current_value = *static_cast<T*>(it->second.get());
        T new_value = updater(current_value);
        it->second = std::make_shared<T>(new_value);
    }
}

// C function declarations for x86_codegen.cpp
extern "C" {
    // String functions
    void* __string_match(void* string_ptr, void* regex_ptr);
    void* __string_replace(void* string_ptr, void* pattern_ptr, void* replacement_ptr);
    int64_t __string_search(void* string_ptr, void* regex_ptr);
    
    // Match result property accessors
    int64_t __match_result_get_index(void* match_array_ptr);
    void* __match_result_get_input(void* match_array_ptr);
    void* __match_result_get_groups(void* match_array_ptr);
}