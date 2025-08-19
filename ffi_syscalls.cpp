#include "ffi_syscalls.h"
#include <dlfcn.h>
#include <cstring>
#include <iostream>
#include <vector>
#include <mutex>


// FFI argument stack for flexible calls
static std::vector<void*> g_ffi_args;
static std::mutex g_ffi_mutex;

extern "C" {

// Dynamic library management
void* ffi_dlopen(const char* path) {
    void* handle = dlopen(path, RTLD_LAZY | RTLD_LOCAL);
    if (!handle) {
        std::cerr << "FFI dlopen error: " << dlerror() << std::endl;
    }
    return handle;
}

void* ffi_dlsym(void* handle, const char* name) {
    if (!handle || !name) {
        return nullptr;
    }
    
    // Clear any existing error
    dlerror();
    
    void* symbol = dlsym(handle, name);
    const char* error = dlerror();
    if (error) {
        std::cerr << "FFI dlsym error: " << error << std::endl;
        return nullptr;
    }
    
    return symbol;
}

bool ffi_dlclose(void* handle) {
    if (!handle) {
        return false;
    }
    return dlclose(handle) == 0;
}

// Argument management
void ffi_clear_args() {
    std::lock_guard<std::mutex> lock(g_ffi_mutex);
    g_ffi_args.clear();
}

void ffi_set_arg_int64(int64_t index, int64_t value) {
    std::lock_guard<std::mutex> lock(g_ffi_mutex);
    if (index >= 0) {
        if (static_cast<size_t>(index) >= g_ffi_args.size()) {
            g_ffi_args.resize(index + 1, nullptr);
        }
        g_ffi_args[index] = reinterpret_cast<void*>(value);
    }
}

void ffi_set_arg_double(int64_t index, double value) {
    std::lock_guard<std::mutex> lock(g_ffi_mutex);
    if (index >= 0) {
        if (static_cast<size_t>(index) >= g_ffi_args.size()) {
            g_ffi_args.resize(index + 1, nullptr);
        }
        // Store double as int64 for simplicity
        union { double d; int64_t i; } u;
        u.d = value;
        g_ffi_args[index] = reinterpret_cast<void*>(u.i);
    }
}

void ffi_set_arg_ptr(int64_t index, void* value) {
    std::lock_guard<std::mutex> lock(g_ffi_mutex);
    if (index >= 0) {
        if (static_cast<size_t>(index) >= g_ffi_args.size()) {
            g_ffi_args.resize(index + 1, nullptr);
        }
        g_ffi_args[index] = value;
    }
}

// Legacy flexible function calls (uses argument stack)
void ffi_call_void(void* symbol) {
    if (!symbol) return;
    
    typedef void (*func0_t)();
    typedef void (*func1_t)(void*);
    typedef void (*func2_t)(void*, void*);
    typedef void (*func3_t)(void*, void*, void*);
    
    std::lock_guard<std::mutex> lock(g_ffi_mutex);
    
    switch (g_ffi_args.size()) {
        case 0: reinterpret_cast<func0_t>(symbol)(); break;
        case 1: reinterpret_cast<func1_t>(symbol)(g_ffi_args[0]); break;
        case 2: reinterpret_cast<func2_t>(symbol)(g_ffi_args[0], g_ffi_args[1]); break;
        case 3: reinterpret_cast<func3_t>(symbol)(g_ffi_args[0], g_ffi_args[1], g_ffi_args[2]); break;
        default:
            std::cerr << "FFI: Too many arguments for ffi_call_void (max 3)" << std::endl;
            break;
    }
}

int64_t ffi_call_int64(void* symbol) {
    if (!symbol) return 0;
    
    typedef int64_t (*func0_t)();
    typedef int64_t (*func1_t)(void*);
    typedef int64_t (*func2_t)(void*, void*);
    typedef int64_t (*func3_t)(void*, void*, void*);
    
    std::lock_guard<std::mutex> lock(g_ffi_mutex);
    
    switch (g_ffi_args.size()) {
        case 0: return reinterpret_cast<func0_t>(symbol)();
        case 1: return reinterpret_cast<func1_t>(symbol)(g_ffi_args[0]);
        case 2: return reinterpret_cast<func2_t>(symbol)(g_ffi_args[0], g_ffi_args[1]);
        case 3: return reinterpret_cast<func3_t>(symbol)(g_ffi_args[0], g_ffi_args[1], g_ffi_args[2]);
        default:
            std::cerr << "FFI: Too many arguments for ffi_call_int64 (max 3)" << std::endl;
            return 0;
    }
}

double ffi_call_double(void* symbol) {
    if (!symbol) return 0.0;
    
    typedef double (*func0_t)();
    typedef double (*func1_t)(void*);
    typedef double (*func2_t)(void*, void*);
    typedef double (*func3_t)(void*, void*, void*);
    
    std::lock_guard<std::mutex> lock(g_ffi_mutex);
    
    switch (g_ffi_args.size()) {
        case 0: return reinterpret_cast<func0_t>(symbol)();
        case 1: return reinterpret_cast<func1_t>(symbol)(g_ffi_args[0]);
        case 2: return reinterpret_cast<func2_t>(symbol)(g_ffi_args[0], g_ffi_args[1]);
        case 3: return reinterpret_cast<func3_t>(symbol)(g_ffi_args[0], g_ffi_args[1], g_ffi_args[2]);
        default:
            std::cerr << "FFI: Too many arguments for ffi_call_double (max 3)" << std::endl;
            return 0.0;
    }
}

void* ffi_call_ptr(void* symbol) {
    if (!symbol) return nullptr;
    
    typedef void* (*func0_t)();
    typedef void* (*func1_t)(void*);
    typedef void* (*func2_t)(void*, void*);
    typedef void* (*func3_t)(void*, void*, void*);
    
    std::lock_guard<std::mutex> lock(g_ffi_mutex);
    
    switch (g_ffi_args.size()) {
        case 0: return reinterpret_cast<func0_t>(symbol)();
        case 1: return reinterpret_cast<func1_t>(symbol)(g_ffi_args[0]);
        case 2: return reinterpret_cast<func2_t>(symbol)(g_ffi_args[0], g_ffi_args[1]);
        case 3: return reinterpret_cast<func3_t>(symbol)(g_ffi_args[0], g_ffi_args[1], g_ffi_args[2]);
        default:
            std::cerr << "FFI: Too many arguments for ffi_call_ptr (max 3)" << std::endl;
            return nullptr;
    }
}

// High-performance direct calls - no argument marshalling

// Direct void calls
void ffi_call_direct_void(void* symbol) {
    if (!symbol) return;
    reinterpret_cast<void(*)()>(symbol)();
}

void ffi_call_direct_void_i64(void* symbol, int64_t arg0) {
    if (!symbol) return;
    reinterpret_cast<void(*)(int64_t)>(symbol)(arg0);
}

void ffi_call_direct_void_i64_i64(void* symbol, int64_t arg0, int64_t arg1) {
    if (!symbol) return;
    reinterpret_cast<void(*)(int64_t, int64_t)>(symbol)(arg0, arg1);
}

void ffi_call_direct_void_i64_i64_i64(void* symbol, int64_t arg0, int64_t arg1, int64_t arg2) {
    if (!symbol) return;
    reinterpret_cast<void(*)(int64_t, int64_t, int64_t)>(symbol)(arg0, arg1, arg2);
}

void ffi_call_direct_void_ptr(void* symbol, void* arg0) {
    if (!symbol) return;
    reinterpret_cast<void(*)(void*)>(symbol)(arg0);
}

void ffi_call_direct_void_ptr_ptr(void* symbol, void* arg0, void* arg1) {
    if (!symbol) return;
    reinterpret_cast<void(*)(void*, void*)>(symbol)(arg0, arg1);
}

void ffi_call_direct_void_ptr_i64(void* symbol, void* arg0, int64_t arg1) {
    if (!symbol) return;
    reinterpret_cast<void(*)(void*, int64_t)>(symbol)(arg0, arg1);
}

// Direct int64 calls
int64_t ffi_call_direct_int64(void* symbol) {
    if (!symbol) return 0;
    return reinterpret_cast<int64_t(*)()>(symbol)();
}

int64_t ffi_call_direct_int64_i64(void* symbol, int64_t arg0) {
    if (!symbol) return 0;
    return reinterpret_cast<int64_t(*)(int64_t)>(symbol)(arg0);
}

int64_t ffi_call_direct_int64_i64_i64(void* symbol, int64_t arg0, int64_t arg1) {
    if (!symbol) return 0;
    return reinterpret_cast<int64_t(*)(int64_t, int64_t)>(symbol)(arg0, arg1);
}

int64_t ffi_call_direct_int64_i64_i64_i64(void* symbol, int64_t arg0, int64_t arg1, int64_t arg2) {
    if (!symbol) return 0;
    return reinterpret_cast<int64_t(*)(int64_t, int64_t, int64_t)>(symbol)(arg0, arg1, arg2);
}

int64_t ffi_call_direct_int64_ptr(void* symbol, void* arg0) {
    if (!symbol) return 0;
    return reinterpret_cast<int64_t(*)(void*)>(symbol)(arg0);
}

int64_t ffi_call_direct_int64_ptr_ptr(void* symbol, void* arg0, void* arg1) {
    if (!symbol) return 0;
    return reinterpret_cast<int64_t(*)(void*, void*)>(symbol)(arg0, arg1);
}

int64_t ffi_call_direct_int64_ptr_i64(void* symbol, void* arg0, int64_t arg1) {
    if (!symbol) return 0;
    return reinterpret_cast<int64_t(*)(void*, int64_t)>(symbol)(arg0, arg1);
}

int64_t ffi_call_direct_int64_ptr_ptr_i64(void* symbol, void* arg0, void* arg1, int64_t arg2) {
    if (!symbol) return 0;
    return reinterpret_cast<int64_t(*)(void*, void*, int64_t)>(symbol)(arg0, arg1, arg2);
}

// Direct double calls
double ffi_call_direct_double(void* symbol) {
    if (!symbol) return 0.0;
    return reinterpret_cast<double(*)()>(symbol)();
}

double ffi_call_direct_double_double(void* symbol, double arg0) {
    if (!symbol) return 0.0;
    return reinterpret_cast<double(*)(double)>(symbol)(arg0);
}

double ffi_call_direct_double_double_double(void* symbol, double arg0, double arg1) {
    if (!symbol) return 0.0;
    return reinterpret_cast<double(*)(double, double)>(symbol)(arg0, arg1);
}

double ffi_call_direct_double_ptr(void* symbol, void* arg0) {
    if (!symbol) return 0.0;
    return reinterpret_cast<double(*)(void*)>(symbol)(arg0);
}

// Direct pointer calls
void* ffi_call_direct_ptr(void* symbol) {
    if (!symbol) return nullptr;
    return reinterpret_cast<void*(*)()>(symbol)();
}

void* ffi_call_direct_ptr_ptr(void* symbol, void* arg0) {
    if (!symbol) return nullptr;
    return reinterpret_cast<void*(*)(void*)>(symbol)(arg0);
}

void* ffi_call_direct_ptr_ptr_ptr(void* symbol, void* arg0, void* arg1) {
    if (!symbol) return nullptr;
    return reinterpret_cast<void*(*)(void*, void*)>(symbol)(arg0, arg1);
}

void* ffi_call_direct_ptr_ptr_i64(void* symbol, void* arg0, int64_t arg1) {
    if (!symbol) return nullptr;
    return reinterpret_cast<void*(*)(void*, int64_t)>(symbol)(arg0, arg1);
}

void* ffi_call_direct_ptr_ptr_ptr_i64(void* symbol, void* arg0, void* arg1, int64_t arg2) {
    if (!symbol) return nullptr;
    return reinterpret_cast<void*(*)(void*, void*, int64_t)>(symbol)(arg0, arg1, arg2);
}

void* ffi_call_direct_ptr_i64(void* symbol, int64_t arg0) {
    if (!symbol) return nullptr;
    return reinterpret_cast<void*(*)(int64_t)>(symbol)(arg0);
}

void* ffi_call_direct_ptr_i64_i64(void* symbol, int64_t arg0, int64_t arg1) {
    if (!symbol) return nullptr;
    return reinterpret_cast<void*(*)(int64_t, int64_t)>(symbol)(arg0, arg1);
}

// Memory management (delegate to standard libc)
void* ffi_malloc(int64_t size) {
    return std::malloc(static_cast<size_t>(size));
}

void ffi_free(void* ptr) {
    std::free(ptr);
}

void* ffi_realloc(void* ptr, int64_t size) {
    return std::realloc(ptr, static_cast<size_t>(size));
}

// Memory utilities
void ffi_memcpy(void* dest, const void* src, int64_t size) {
    std::memcpy(dest, src, static_cast<size_t>(size));
}

void ffi_memset(void* ptr, int value, int64_t size) {
    std::memset(ptr, value, static_cast<size_t>(size));
}

int ffi_memcmp(const void* ptr1, const void* ptr2, int64_t size) {
    return std::memcmp(ptr1, ptr2, static_cast<size_t>(size));
}

// String utilities for FFI
void* ffi_string_to_cstring(const char* str) {
    if (!str) return nullptr;
    size_t len = strlen(str);
    char* cstr = static_cast<char*>(std::malloc(len + 1));
    if (cstr) {
        strcpy(cstr, str);
    }
    return cstr;
}

void* ffi_cstring_to_string(const char* cstr) {
    // This would need to integrate with UltraScript's string system
    // For now, just return the cstring
    return const_cast<char*>(cstr);
}

// Advanced calling conventions (stub for future use)
void ffi_set_calling_convention(int64_t convention) {
    // TODO: Implement different calling conventions
}

void ffi_set_arg_count(int64_t count) {
    std::lock_guard<std::mutex> lock(g_ffi_mutex);
    g_ffi_args.reserve(static_cast<size_t>(count));
}

// Error handling
static thread_local char g_last_error[256] = {0};

const char* ffi_last_error() {
    const char* dl_error = dlerror();
    if (dl_error) {
        strncpy(g_last_error, dl_error, sizeof(g_last_error) - 1);
        g_last_error[sizeof(g_last_error) - 1] = '\0';
        return g_last_error;
    }
    return g_last_error[0] ? g_last_error : nullptr;
}

void ffi_clear_error() {
    g_last_error[0] = '\0';
    dlerror(); // Clear dlopen errors
}

} // extern "C"

