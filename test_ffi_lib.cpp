// Simple test library for FFI testing
#include <cstdio>
#include <cstdint>
#include <cstring>

extern "C" {

// Simple test functions with different signatures
int64_t test_add(int64_t a, int64_t b) {
    printf("test_add called with %ld, %ld\n", a, b);
    return a + b;
}

void test_print_hello() {
    printf("Hello from test library!\n");
}

void test_print_string(const char* str) {
    printf("test_print_string called with: %s\n", str ? str : "(null)");
}

double test_multiply_double(double a, double b) {
    printf("test_multiply_double called with %f, %f\n", a, b);
    return a * b;
}

void* test_return_ptr(void* input) {
    printf("test_return_ptr called with: %p\n", input);
    return input;
}

int64_t test_complex_call(int64_t a, const char* str, double d, void* ptr) {
    printf("test_complex_call called with: %ld, %s, %f, %p\n", 
           a, str ? str : "(null)", d, ptr);
    return a + (str ? (int64_t)strlen(str) : 0) + (int64_t)d;
}

} // extern "C"
