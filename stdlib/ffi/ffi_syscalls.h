#pragma once

#include <cstdint>

namespace ultraScript {

// High-performance FFI syscalls for dynamic library loading and function calling
// Optimized for zero-overhead calls with JIT specialization
extern "C" {
    
    // Dynamic library management
    void* ffi_dlopen(const char* path);
    void* ffi_dlsym(void* handle, const char* name);
    bool ffi_dlclose(void* handle);
    
    // Legacy function call interface with argument stack (slower but flexible)
    void ffi_clear_args();
    void ffi_set_arg_int64(int64_t index, int64_t value);
    void ffi_set_arg_double(int64_t index, double value);
    void ffi_set_arg_ptr(int64_t index, void* value);
    
    // Legacy function calls (slower but flexible)
    void ffi_call_void(void* symbol);
    int64_t ffi_call_int64(void* symbol);
    double ffi_call_double(void* symbol);
    void* ffi_call_ptr(void* symbol);
    
    // High-performance direct calls - JIT optimizable
    // These bypass argument marshalling for maximum speed
    
    // Direct void calls
    void ffi_call_direct_void(void* symbol);
    void ffi_call_direct_void_i64(void* symbol, int64_t arg0);
    void ffi_call_direct_void_i64_i64(void* symbol, int64_t arg0, int64_t arg1);
    void ffi_call_direct_void_i64_i64_i64(void* symbol, int64_t arg0, int64_t arg1, int64_t arg2);
    void ffi_call_direct_void_ptr(void* symbol, void* arg0);
    void ffi_call_direct_void_ptr_ptr(void* symbol, void* arg0, void* arg1);
    void ffi_call_direct_void_ptr_i64(void* symbol, void* arg0, int64_t arg1);
    
    // Direct int64 calls
    int64_t ffi_call_direct_int64(void* symbol);
    int64_t ffi_call_direct_int64_i64(void* symbol, int64_t arg0);
    int64_t ffi_call_direct_int64_i64_i64(void* symbol, int64_t arg0, int64_t arg1);
    int64_t ffi_call_direct_int64_i64_i64_i64(void* symbol, int64_t arg0, int64_t arg1, int64_t arg2);
    int64_t ffi_call_direct_int64_ptr(void* symbol, void* arg0);
    int64_t ffi_call_direct_int64_ptr_ptr(void* symbol, void* arg0, void* arg1);
    int64_t ffi_call_direct_int64_ptr_i64(void* symbol, void* arg0, int64_t arg1);
    int64_t ffi_call_direct_int64_ptr_ptr_i64(void* symbol, void* arg0, void* arg1, int64_t arg2);
    
    // Direct double calls
    double ffi_call_direct_double(void* symbol);
    double ffi_call_direct_double_double(void* symbol, double arg0);
    double ffi_call_direct_double_double_double(void* symbol, double arg0, double arg1);
    double ffi_call_direct_double_ptr(void* symbol, void* arg0);
    
    // Direct pointer calls
    void* ffi_call_direct_ptr(void* symbol);
    void* ffi_call_direct_ptr_ptr(void* symbol, void* arg0);
    void* ffi_call_direct_ptr_ptr_ptr(void* symbol, void* arg0, void* arg1);
    void* ffi_call_direct_ptr_ptr_i64(void* symbol, void* arg0, int64_t arg1);
    void* ffi_call_direct_ptr_ptr_ptr_i64(void* symbol, void* arg0, void* arg1, int64_t arg2);
    void* ffi_call_direct_ptr_i64(void* symbol, int64_t arg0);
    void* ffi_call_direct_ptr_i64_i64(void* symbol, int64_t arg0, int64_t arg1);
    
    // Memory management for FFI
    void* malloc(int64_t size);
    void free(void* ptr);
    void* realloc(void* ptr, int64_t size);
    
    // Memory utilities
    void memcpy(void* dest, const void* src, int64_t size);
    void memset(void* ptr, int value, int64_t size);
    int memcmp(const void* ptr1, const void* ptr2, int64_t size);
    
    // String utilities for FFI
    void* string_to_cstring(const char* str);
    void* cstring_to_string(const char* cstr);
    
    // Advanced calling conventions (for future use)
    void ffi_set_calling_convention(int64_t convention); // 0=cdecl, 1=stdcall, etc.
    void ffi_set_arg_count(int64_t count);
    
    // Error handling
    const char* ffi_last_error();
    void ffi_clear_error();
}

} // namespace ultraScript
