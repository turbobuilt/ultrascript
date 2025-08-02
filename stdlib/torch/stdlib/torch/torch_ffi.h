#pragma once

#include <cstdint>
#include <cstddef>

namespace ultraScript {

// Forward declarations for opaque handle types
typedef struct TorchTensor_t* TorchTensor;
typedef struct TorchDevice_t* TorchDevice;
typedef struct TorchDtype_t* TorchDtype;

// FFI interface for LibTorch integration
// This provides the low-level C interface that UltraScript can call with perfect performance
extern "C" {
    
    // Library management
    bool torch_init();
    void torch_cleanup();
    const char* torch_version();
    
    // Device management
    TorchDevice torch_device_cpu();
    TorchDevice torch_device_cuda(int64_t device_id);
    bool torch_cuda_is_available();
    int64_t torch_cuda_device_count();
    void torch_cuda_empty_cache();
    
    // Data types
    TorchDtype torch_dtype_float32();
    TorchDtype torch_dtype_float64();
    TorchDtype torch_dtype_int32();
    TorchDtype torch_dtype_int64();
    TorchDtype torch_dtype_bool();
    
    // Tensor creation
    TorchTensor torch_tensor_empty(int64_t* sizes, int64_t ndim, TorchDtype dtype, TorchDevice device);
    TorchTensor torch_tensor_zeros(int64_t* sizes, int64_t ndim, TorchDtype dtype, TorchDevice device);
    TorchTensor torch_tensor_ones(int64_t* sizes, int64_t ndim, TorchDtype dtype, TorchDevice device);
    TorchTensor torch_tensor_randn(int64_t* sizes, int64_t ndim, TorchDtype dtype, TorchDevice device);
    TorchTensor torch_tensor_rand(int64_t* sizes, int64_t ndim, TorchDtype dtype, TorchDevice device);
    
    // Tensor from data
    TorchTensor torch_tensor_from_blob(void* data, int64_t* sizes, int64_t ndim, TorchDtype dtype);
    TorchTensor torch_tensor_from_array_float32(float* data, int64_t* sizes, int64_t ndim);
    TorchTensor torch_tensor_from_array_float64(double* data, int64_t* sizes, int64_t ndim);
    TorchTensor torch_tensor_from_array_int32(int32_t* data, int64_t* sizes, int64_t ndim);
    TorchTensor torch_tensor_from_array_int64(int64_t* data, int64_t* sizes, int64_t ndim);
    
    // Tensor properties
    int64_t torch_tensor_ndim(TorchTensor tensor);
    int64_t torch_tensor_size(TorchTensor tensor, int64_t dim);
    int64_t torch_tensor_numel(TorchTensor tensor);
    TorchDtype torch_tensor_dtype(TorchTensor tensor);
    TorchDevice torch_tensor_device(TorchTensor tensor);
    void* torch_tensor_data_ptr(TorchTensor tensor);
    
    // Tensor operations - arithmetic
    TorchTensor torch_tensor_add(TorchTensor a, TorchTensor b);
    TorchTensor torch_tensor_sub(TorchTensor a, TorchTensor b);
    TorchTensor torch_tensor_mul(TorchTensor a, TorchTensor b);
    TorchTensor torch_tensor_div(TorchTensor a, TorchTensor b);
    TorchTensor torch_tensor_matmul(TorchTensor a, TorchTensor b);
    
    // Tensor operations - scalar
    TorchTensor torch_tensor_add_scalar(TorchTensor tensor, double scalar);
    TorchTensor torch_tensor_sub_scalar(TorchTensor tensor, double scalar);
    TorchTensor torch_tensor_mul_scalar(TorchTensor tensor, double scalar);
    TorchTensor torch_tensor_div_scalar(TorchTensor tensor, double scalar);
    
    // Tensor operations - comparison
    TorchTensor torch_tensor_eq(TorchTensor a, TorchTensor b);
    TorchTensor torch_tensor_ne(TorchTensor a, TorchTensor b);
    TorchTensor torch_tensor_lt(TorchTensor a, TorchTensor b);
    TorchTensor torch_tensor_le(TorchTensor a, TorchTensor b);
    TorchTensor torch_tensor_gt(TorchTensor a, TorchTensor b);
    TorchTensor torch_tensor_ge(TorchTensor a, TorchTensor b);
    
    // Tensor operations - mathematical functions
    TorchTensor torch_tensor_sin(TorchTensor tensor);
    TorchTensor torch_tensor_cos(TorchTensor tensor);
    TorchTensor torch_tensor_tan(TorchTensor tensor);
    TorchTensor torch_tensor_exp(TorchTensor tensor);
    TorchTensor torch_tensor_log(TorchTensor tensor);
    TorchTensor torch_tensor_sqrt(TorchTensor tensor);
    TorchTensor torch_tensor_abs(TorchTensor tensor);
    TorchTensor torch_tensor_neg(TorchTensor tensor);
    
    // Tensor operations - shape manipulation
    TorchTensor torch_tensor_reshape(TorchTensor tensor, int64_t* sizes, int64_t ndim);
    TorchTensor torch_tensor_view(TorchTensor tensor, int64_t* sizes, int64_t ndim);
    TorchTensor torch_tensor_transpose(TorchTensor tensor, int64_t dim0, int64_t dim1);
    TorchTensor torch_tensor_permute(TorchTensor tensor, int64_t* dims, int64_t ndim);
    TorchTensor torch_tensor_squeeze(TorchTensor tensor, int64_t dim);
    TorchTensor torch_tensor_unsqueeze(TorchTensor tensor, int64_t dim);
    
    // Tensor operations - indexing/slicing
    TorchTensor torch_tensor_slice(TorchTensor tensor, int64_t dim, int64_t start, int64_t end, int64_t step);
    TorchTensor torch_tensor_index_select(TorchTensor tensor, int64_t dim, TorchTensor indices);
    
    // Tensor operations - reductions
    TorchTensor torch_tensor_sum(TorchTensor tensor, int64_t* dims, int64_t ndim, bool keepdim);
    TorchTensor torch_tensor_mean(TorchTensor tensor, int64_t* dims, int64_t ndim, bool keepdim);
    TorchTensor torch_tensor_max(TorchTensor tensor, int64_t* dims, int64_t ndim, bool keepdim);
    TorchTensor torch_tensor_min(TorchTensor tensor, int64_t* dims, int64_t ndim, bool keepdim);
    
    // Tensor memory management
    void torch_tensor_free(TorchTensor tensor);
    TorchTensor torch_tensor_clone(TorchTensor tensor);
    TorchTensor torch_tensor_detach(TorchTensor tensor);
    TorchTensor torch_tensor_to(TorchTensor tensor, TorchDevice device, TorchDtype dtype);
    
    // Autograd
    void torch_tensor_backward(TorchTensor tensor);
    TorchTensor torch_tensor_grad(TorchTensor tensor);
    void torch_tensor_set_requires_grad(TorchTensor tensor, bool requires_grad);
    bool torch_tensor_requires_grad(TorchTensor tensor);
    
    // Neural network operations
    TorchTensor torch_nn_linear(TorchTensor input, TorchTensor weight, TorchTensor bias);
    TorchTensor torch_nn_conv2d(TorchTensor input, TorchTensor weight, TorchTensor bias, 
                               int64_t* stride, int64_t* padding, int64_t* dilation);
    TorchTensor torch_nn_relu(TorchTensor input);
    TorchTensor torch_nn_sigmoid(TorchTensor input);
    TorchTensor torch_nn_softmax(TorchTensor input, int64_t dim);
    TorchTensor torch_nn_cross_entropy(TorchTensor input, TorchTensor target);
    
    // Model serialization
    bool torch_save_tensor(TorchTensor tensor, const char* path);
    TorchTensor torch_load_tensor(const char* path);
    
    // Utilities
    void torch_set_seed(int64_t seed);
    void torch_manual_seed(int64_t seed);
    void torch_print_tensor(TorchTensor tensor);
    
    // Error handling
    const char* torch_last_error();
    void torch_clear_error();
}

// Runtime object structure for torch integration
struct TorchObject {
    static constexpr const char* OBJECT_NAME = "torch";
    
    // Core functions
    void* init;
    void* cleanup;
    void* version;
    void* set_seed;
    void* manual_seed;
    
    // Device functions
    void* device_cpu;
    void* device_cuda;
    void* cuda_is_available;
    void* cuda_device_count;
    void* cuda_empty_cache;
    
    // Tensor creation functions
    void* empty;
    void* zeros;
    void* ones;
    void* randn;
    void* rand;
    void* tensor_from_blob;
    void* tensor_from_array_float32;
    void* tensor_from_array_float64;
    void* tensor_from_array_int32;
    void* tensor_from_array_int64;
    
    // Tensor operations
    void* tensor_add;
    void* tensor_sub;
    void* tensor_mul;
    void* tensor_div;
    void* tensor_matmul;
    void* tensor_sin;
    void* tensor_cos;
    void* tensor_exp;
    void* tensor_log;
    void* tensor_sqrt;
    
    // Shape operations
    void* tensor_reshape;
    void* tensor_view;
    void* tensor_transpose;
    
    // Neural network operations
    void* nn_linear;
    void* nn_conv2d;
    void* nn_relu;
    void* nn_sigmoid;
    void* nn_softmax;
    
    // I/O operations
    void* save_tensor;
    void* load_tensor;
};

} // namespace ultraScript
