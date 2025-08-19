// FFI test program
#include "ffi_syscalls.h"
#include <iostream>
#include <cstring>

int main() {
    std::cout << "=== UltraScript FFI Test ===" << std::endl;
    
    // First, build the test library
    std::cout << "Building test library..." << std::endl;
    if (system("g++ -shared -fPIC test_ffi_lib.cpp -o libtest_ffi.so") != 0) {
        std::cerr << "Failed to build test library" << std::endl;
        return 1;
    }
    
    // Test 1: Load library
    std::cout << "\n1. Testing library loading..." << std::endl;
    void* lib = ffi_dlopen("./libtest_ffi.so");
    if (!lib) {
        std::cerr << "Failed to load library: " << ffi_last_error() << std::endl;
        return 1;
    }
    std::cout << "✓ Library loaded successfully" << std::endl;
    
    // Test 2: Load symbols
    std::cout << "\n2. Testing symbol loading..." << std::endl;
    void* test_add_sym = ffi_dlsym(lib, "test_add");
    void* test_print_hello_sym = ffi_dlsym(lib, "test_print_hello");
    void* test_print_string_sym = ffi_dlsym(lib, "test_print_string");
    void* test_multiply_double_sym = ffi_dlsym(lib, "test_multiply_double");
    void* test_return_ptr_sym = ffi_dlsym(lib, "test_return_ptr");
    
    if (!test_add_sym || !test_print_hello_sym || !test_print_string_sym || 
        !test_multiply_double_sym || !test_return_ptr_sym) {
        std::cerr << "Failed to load symbols: " << ffi_last_error() << std::endl;
        return 1;
    }
    std::cout << "✓ All symbols loaded successfully" << std::endl;
    
    // Test 3: Direct calls - void function
    std::cout << "\n3. Testing direct void call..." << std::endl;
    ffi_call_direct_void(test_print_hello_sym);
    std::cout << "✓ Direct void call successful" << std::endl;
    
    // Test 4: Direct calls - int64 return
    std::cout << "\n4. Testing direct int64 call..." << std::endl;
    int64_t result = ffi_call_direct_int64_i64_i64(test_add_sym, 42, 24);
    std::cout << "Result: " << result << " (expected: 66)" << std::endl;
    if (result == 66) {
        std::cout << "✓ Direct int64 call successful" << std::endl;
    } else {
        std::cerr << "✗ Direct int64 call failed" << std::endl;
    }
    
    // Test 5: Direct calls - string parameter
    std::cout << "\n5. Testing direct call with string..." << std::endl;
    const char* test_str = "Hello FFI!";
    ffi_call_direct_void_ptr(test_print_string_sym, const_cast<char*>(test_str));
    std::cout << "✓ Direct string call successful" << std::endl;
    
    // Test 6: Direct calls - double
    std::cout << "\n6. Testing direct double call..." << std::endl;
    double double_result = ffi_call_direct_double_double_double(test_multiply_double_sym, 3.14, 2.0);
    std::cout << "Result: " << double_result << " (expected: ~6.28)" << std::endl;
    if (double_result > 6.0 && double_result < 7.0) {
        std::cout << "✓ Direct double call successful" << std::endl;
    } else {
        std::cerr << "✗ Direct double call failed" << std::endl;
    }
    
    // Test 7: Direct calls - pointer
    std::cout << "\n7. Testing direct pointer call..." << std::endl;
    void* test_ptr = reinterpret_cast<void*>(0x12345678);
    void* ptr_result = ffi_call_direct_ptr_ptr(test_return_ptr_sym, test_ptr);
    std::cout << "Input: " << test_ptr << ", Result: " << ptr_result << std::endl;
    if (ptr_result == test_ptr) {
        std::cout << "✓ Direct pointer call successful" << std::endl;
    } else {
        std::cerr << "✗ Direct pointer call failed" << std::endl;
    }
    
    // Test 8: Legacy argument stack calls
    std::cout << "\n8. Testing legacy argument stack calls..." << std::endl;
    ffi_clear_args();
    ffi_set_arg_int64(0, 100);
    ffi_set_arg_int64(1, 200);
    int64_t legacy_result = ffi_call_int64(test_add_sym);
    std::cout << "Legacy result: " << legacy_result << " (expected: 300)" << std::endl;
    if (legacy_result == 300) {
        std::cout << "✓ Legacy argument stack call successful" << std::endl;
    } else {
        std::cerr << "✗ Legacy argument stack call failed" << std::endl;
    }
    
    // Test 9: Memory management
    std::cout << "\n9. Testing FFI memory management..." << std::endl;
    void* mem = ffi_malloc(1024);
    if (mem) {
        std::cout << "✓ ffi_malloc successful" << std::endl;
        ffi_memset(mem, 0x42, 100);
        std::cout << "✓ ffi_memset successful" << std::endl;
        
        void* mem2 = ffi_malloc(100);
        ffi_memcpy(mem2, mem, 100);
        if (ffi_memcmp(mem, mem2, 100) == 0) {
            std::cout << "✓ ffi_memcpy and ffi_memcmp successful" << std::endl;
        } else {
            std::cerr << "✗ ffi_memcpy or ffi_memcmp failed" << std::endl;
        }
        
        ffi_free(mem);
        ffi_free(mem2);
        std::cout << "✓ ffi_free successful" << std::endl;
    } else {
        std::cerr << "✗ ffi_malloc failed" << std::endl;
    }
    
    // Test 10: Close library
    std::cout << "\n10. Testing library cleanup..." << std::endl;
    if (ffi_dlclose(lib)) {
        std::cout << "✓ Library closed successfully" << std::endl;
    } else {
        std::cerr << "✗ Failed to close library" << std::endl;
    }
    
    std::cout << "\n=== FFI Test Complete ===" << std::endl;
    return 0;
}
