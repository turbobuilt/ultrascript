#include <iostream>
#include <cassert>

// Test program for the torch_c_wrapper
// This verifies that our C wrapper functions work correctly

extern "C" {
    // Function declarations for our C wrapper
    void* torch_ones_2d(long long size0, long long size1);
    void* torch_zeros_2d(long long size0, long long size1);
    void* torch_randn_2d(long long size0, long long size1);
    void* torch_add(void* tensor_a, void* tensor_b);
    void* torch_sub(void* tensor_a, void* tensor_b);
    void* torch_mul(void* tensor_a, void* tensor_b);
    void* torch_matmul(void* tensor_a, void* tensor_b);
    long long torch_tensor_ndim(void* tensor);
    long long torch_tensor_size(void* tensor, long long dim);
    long long torch_tensor_numel(void* tensor);
    void torch_tensor_print(void* tensor);
    void* torch_tensor_clone(void* tensor);
    void torch_tensor_free(void* tensor);
    void torch_manual_seed(long long seed);
    bool torch_cuda_is_available();
    long long torch_cuda_device_count();
    const char* torch_get_last_error();
    void torch_clear_error();
}

int main() {
    std::cout << "=== Testing Torch C Wrapper ===" << std::endl;
    
    // Set seed for reproducible results
    torch_manual_seed(42);
    
    // Test tensor creation
    std::cout << "\n1. Testing tensor creation:" << std::endl;
    void* a = torch_ones_2d(2, 3);
    void* b = torch_zeros_2d(2, 3);
    void* c = torch_randn_2d(2, 3);
    
    if (!a || !b || !c) {
        std::cout << "✗ Tensor creation failed: " << torch_get_last_error() << std::endl;
        return 1;
    }
    std::cout << "✓ Created tensors successfully" << std::endl;
    
    // Test tensor properties
    std::cout << "\n2. Testing tensor properties:" << std::endl;
    long long ndim = torch_tensor_ndim(a);
    long long size0 = torch_tensor_size(a, 0);
    long long size1 = torch_tensor_size(a, 1);
    long long numel = torch_tensor_numel(a);
    
    std::cout << "Tensor 'a' properties:" << std::endl;
    std::cout << "  ndim: " << ndim << std::endl;
    std::cout << "  size(0): " << size0 << std::endl;
    std::cout << "  size(1): " << size1 << std::endl;
    std::cout << "  numel: " << numel << std::endl;
    
    assert(ndim == 2);
    assert(size0 == 2);
    assert(size1 == 3);
    assert(numel == 6);
    std::cout << "✓ Tensor properties correct" << std::endl;
    
    // Test tensor operations
    std::cout << "\n3. Testing tensor operations:" << std::endl;
    void* sum = torch_add(a, b);
    void* diff = torch_sub(a, b);
    void* prod = torch_mul(a, c);
    
    if (!sum || !diff || !prod) {
        std::cout << "✗ Tensor operations failed: " << torch_get_last_error() << std::endl;
        return 1;
    }
    std::cout << "✓ Tensor operations successful" << std::endl;
    
    // Test matrix multiplication
    std::cout << "\n4. Testing matrix multiplication:" << std::endl;
    void* x = torch_ones_2d(2, 3);
    void* y = torch_ones_2d(3, 4);
    void* z = torch_matmul(x, y);
    
    if (!z) {
        std::cout << "✗ Matrix multiplication failed: " << torch_get_last_error() << std::endl;
        return 1;
    }
    
    long long z_rows = torch_tensor_size(z, 0);
    long long z_cols = torch_tensor_size(z, 1);
    std::cout << "Matrix multiplication result shape: [" << z_rows << ", " << z_cols << "]" << std::endl;
    assert(z_rows == 2);
    assert(z_cols == 4);
    std::cout << "✓ Matrix multiplication correct" << std::endl;
    
    // Test tensor printing
    std::cout << "\n5. Testing tensor printing:" << std::endl;
    std::cout << "Tensor 'a' (ones):" << std::endl;
    torch_tensor_print(a);
    
    std::cout << "Tensor 'b' (zeros):" << std::endl;
    torch_tensor_print(b);
    
    std::cout << "Sum result:" << std::endl;
    torch_tensor_print(sum);
    
    // Test cloning
    std::cout << "\n6. Testing tensor cloning:" << std::endl;
    void* a_clone = torch_tensor_clone(a);
    if (!a_clone) {
        std::cout << "✗ Tensor cloning failed: " << torch_get_last_error() << std::endl;
        return 1;
    }
    std::cout << "✓ Tensor cloning successful" << std::endl;
    
    // Test CUDA availability
    std::cout << "\n7. Testing CUDA availability:" << std::endl;
    bool cuda_available = torch_cuda_is_available();
    long long cuda_devices = torch_cuda_device_count();
    std::cout << "CUDA available: " << (cuda_available ? "Yes" : "No") << std::endl;
    std::cout << "CUDA devices: " << cuda_devices << std::endl;
    
    // Clean up memory
    std::cout << "\n8. Cleaning up memory:" << std::endl;
    torch_tensor_free(a);
    torch_tensor_free(b);
    torch_tensor_free(c);
    torch_tensor_free(sum);
    torch_tensor_free(diff);
    torch_tensor_free(prod);
    torch_tensor_free(x);
    torch_tensor_free(y);
    torch_tensor_free(z);
    torch_tensor_free(a_clone);
    std::cout << "✓ Memory cleaned up" << std::endl;
    
    std::cout << "\n=== All tests passed! ===" << std::endl;
    return 0;
}
