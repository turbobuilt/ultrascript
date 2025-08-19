// Ultra-Fast Runtime Functions - What the JIT calls for maximum performance
// These functions are called by the generated code for typed arrays

#include "ultra_performance_array.h"
#include <immintrin.h>  // For SIMD intrinsics
#include <cstring>      // For memcpy, memset

namespace runtime {

// ============================================================================
// Ultra-Fast Factory Functions - Called by Generated Code
// ============================================================================

// Generate zeros array - SIMD optimized
template<typename T>
void* create_zeros_typed_array(size_t size) {
    TypedArray<T>* arr = new TypedArray<T>(size);
    
    // Use SIMD for fast zero initialization
    if constexpr (sizeof(T) == 4) {  // float32 or int32
        size_t simd_count = size / 8;  // 8 elements per 256-bit SIMD
        size_t remainder = size % 8;
        
        __m256 zero_vec = _mm256_setzero_ps();
        T* data = arr->data();
        
        for (size_t i = 0; i < simd_count; ++i) {
            _mm256_storeu_ps(reinterpret_cast<float*>(data + i * 8), zero_vec);
        }
        
        // Handle remainder
        std::memset(data + simd_count * 8, 0, remainder * sizeof(T));
    } else if constexpr (sizeof(T) == 8) {  // float64 or int64
        size_t simd_count = size / 4;  // 4 elements per 256-bit SIMD
        size_t remainder = size % 4;
        
        __m256d zero_vec = _mm256_setzero_pd();
        T* data = arr->data();
        
        for (size_t i = 0; i < simd_count; ++i) {
            _mm256_storeu_pd(reinterpret_cast<double*>(data + i * 4), zero_vec);
        }
        
        std::memset(data + simd_count * 4, 0, remainder * sizeof(T));
    } else {
        // Fallback for other sizes
        std::memset(arr->data(), 0, size * sizeof(T));
    }
    
    return arr;
}

// Generate ones array - SIMD optimized  
template<typename T>
void* create_ones_typed_array(size_t size) {
    TypedArray<T>* arr = new TypedArray<T>(size);
    T* data = arr->data();
    
    if constexpr (std::is_same_v<T, float>) {
        size_t simd_count = size / 8;
        size_t remainder = size % 8;
        
        __m256 ones_vec = _mm256_set1_ps(1.0f);
        
        for (size_t i = 0; i < simd_count; ++i) {
            _mm256_storeu_ps(data + i * 8, ones_vec);
        }
        
        for (size_t i = simd_count * 8; i < size; ++i) {
            data[i] = T(1);
        }
    } else if constexpr (std::is_same_v<T, double>) {
        size_t simd_count = size / 4;
        size_t remainder = size % 4;
        
        __m256d ones_vec = _mm256_set1_pd(1.0);
        
        for (size_t i = 0; i < simd_count; ++i) {
            _mm256_storeu_pd(data + i * 4, ones_vec);
        }
        
        for (size_t i = simd_count * 4; i < size; ++i) {
            data[i] = T(1);
        }
    } else {
        // Integer types
        for (size_t i = 0; i < size; ++i) {
            data[i] = T(1);
        }
    }
    
    return arr;
}

// Generate full array with specific value - SIMD optimized
template<typename T>
void* create_full_typed_array(size_t size, T fill_value) {
    TypedArray<T>* arr = new TypedArray<T>(size);
    T* data = arr->data();
    
    if constexpr (std::is_same_v<T, float>) {
        size_t simd_count = size / 8;
        __m256 fill_vec = _mm256_set1_ps(fill_value);
        
        for (size_t i = 0; i < simd_count; ++i) {
            _mm256_storeu_ps(data + i * 8, fill_vec);
        }
        
        for (size_t i = simd_count * 8; i < size; ++i) {
            data[i] = fill_value;
        }
    } else if constexpr (std::is_same_v<T, double>) {
        size_t simd_count = size / 4;
        __m256d fill_vec = _mm256_set1_pd(fill_value);
        
        for (size_t i = 0; i < simd_count; ++i) {
            _mm256_storeu_pd(data + i * 4, fill_vec);
        }
        
        for (size_t i = simd_count * 4; i < size; ++i) {
            data[i] = fill_value;
        }
    } else {
        // Integer types - could be optimized with SIMD too
        for (size_t i = 0; i < size; ++i) {
            data[i] = fill_value;
        }
    }
    
    return arr;
}

// ============================================================================
// Ultra-Fast Access Functions - Zero Overhead
// ============================================================================

// Direct element access - no bounds checking for maximum speed
template<typename T>
inline T typed_array_get_unchecked(void* array_ptr, size_t index) {
    TypedArray<T>* arr = static_cast<TypedArray<T>*>(array_ptr);
    return arr->data()[index];  // Direct memory access, no checks
}

template<typename T>
inline void typed_array_set_unchecked(void* array_ptr, size_t index, T value) {
    TypedArray<T>* arr = static_cast<TypedArray<T>*>(array_ptr);
    arr->data()[index] = value;  // Direct memory write, no checks
}

// Safe access with bounds checking (when needed)
template<typename T>
T typed_array_get_checked(void* array_ptr, size_t index) {
    TypedArray<T>* arr = static_cast<TypedArray<T>*>(array_ptr);
    if (index >= arr->size()) {
        throw std::out_of_range("Array index out of bounds");
    }
    return arr->data()[index];
}

template<typename T>
void typed_array_set_checked(void* array_ptr, size_t index, T value) {
    TypedArray<T>* arr = static_cast<TypedArray<T>*>(array_ptr);
    if (index >= arr->size()) {
        throw std::out_of_range("Array index out of bounds");
    }
    arr->data()[index] = value;
}

// ============================================================================
// Ultra-Fast Mathematical Operations - SIMD Optimized
// ============================================================================

// Element-wise addition - SIMD vectorized
template<typename T>
void* typed_array_add(void* left_ptr, void* right_ptr) {
    TypedArray<T>* left = static_cast<TypedArray<T>*>(left_ptr);
    TypedArray<T>* right = static_cast<TypedArray<T>*>(right_ptr);
    
    if (left->size() != right->size()) {
        throw std::invalid_argument("Array size mismatch");
    }
    
    size_t size = left->size();
    TypedArray<T>* result = new TypedArray<T>(size);
    
    T* left_data = left->data();
    T* right_data = right->data();
    T* result_data = result->data();
    
    if constexpr (std::is_same_v<T, float>) {
        size_t simd_count = size / 8;
        
        for (size_t i = 0; i < simd_count; ++i) {
            __m256 left_vec = _mm256_loadu_ps(left_data + i * 8);
            __m256 right_vec = _mm256_loadu_ps(right_data + i * 8);
            __m256 result_vec = _mm256_add_ps(left_vec, right_vec);
            _mm256_storeu_ps(result_data + i * 8, result_vec);
        }
        
        // Handle remainder
        for (size_t i = simd_count * 8; i < size; ++i) {
            result_data[i] = left_data[i] + right_data[i];
        }
    } else if constexpr (std::is_same_v<T, double>) {
        size_t simd_count = size / 4;
        
        for (size_t i = 0; i < simd_count; ++i) {
            __m256d left_vec = _mm256_loadu_pd(left_data + i * 4);
            __m256d right_vec = _mm256_loadu_pd(right_data + i * 4);
            __m256d result_vec = _mm256_add_pd(left_vec, right_vec);
            _mm256_storeu_pd(result_data + i * 4, result_vec);
        }
        
        for (size_t i = simd_count * 4; i < size; ++i) {
            result_data[i] = left_data[i] + right_data[i];
        }
    } else {
        // Integer types - fallback (could be SIMD optimized too)
        for (size_t i = 0; i < size; ++i) {
            result_data[i] = left_data[i] + right_data[i];
        }
    }
    
    return result;
}

// Element-wise multiplication - SIMD vectorized
template<typename T>
void* typed_array_multiply(void* left_ptr, void* right_ptr) {
    TypedArray<T>* left = static_cast<TypedArray<T>*>(left_ptr);
    TypedArray<T>* right = static_cast<TypedArray<T>*>(right_ptr);
    
    size_t size = left->size();
    TypedArray<T>* result = new TypedArray<T>(size);
    
    T* left_data = left->data();
    T* right_data = right->data();
    T* result_data = result->data();
    
    if constexpr (std::is_same_v<T, float>) {
        size_t simd_count = size / 8;
        
        for (size_t i = 0; i < simd_count; ++i) {
            __m256 left_vec = _mm256_loadu_ps(left_data + i * 8);
            __m256 right_vec = _mm256_loadu_ps(right_data + i * 8);
            __m256 result_vec = _mm256_mul_ps(left_vec, right_vec);
            _mm256_storeu_ps(result_data + i * 8, result_vec);
        }
        
        for (size_t i = simd_count * 8; i < size; ++i) {
            result_data[i] = left_data[i] * right_data[i];
        }
    } else if constexpr (std::is_same_v<T, double>) {
        size_t simd_count = size / 4;
        
        for (size_t i = 0; i < simd_count; ++i) {
            __m256d left_vec = _mm256_loadu_pd(left_data + i * 4);
            __m256d right_vec = _mm256_loadu_pd(right_data + i * 4);
            __m256d result_vec = _mm256_mul_pd(left_vec, right_vec);
            _mm256_storeu_pd(result_data + i * 4, result_vec);
        }
        
        for (size_t i = simd_count * 4; i < size; ++i) {
            result_data[i] = left_data[i] * right_data[i];
        }
    } else {
        for (size_t i = 0; i < size; ++i) {
            result_data[i] = left_data[i] * right_data[i];
        }
    }
    
    return result;
}

// Sum reduction - SIMD optimized
template<typename T>
T typed_array_sum(void* array_ptr) {
    TypedArray<T>* arr = static_cast<TypedArray<T>*>(array_ptr);
    T* data = arr->data();
    size_t size = arr->size();
    
    if constexpr (std::is_same_v<T, float>) {
        size_t simd_count = size / 8;
        __m256 sum_vec = _mm256_setzero_ps();
        
        for (size_t i = 0; i < simd_count; ++i) {
            __m256 data_vec = _mm256_loadu_ps(data + i * 8);
            sum_vec = _mm256_add_ps(sum_vec, data_vec);
        }
        
        // Horizontal sum of SIMD register
        float sum_array[8];
        _mm256_storeu_ps(sum_array, sum_vec);
        T result = T(0);
        for (int i = 0; i < 8; ++i) {
            result += T(sum_array[i]);
        }
        
        // Add remainder
        for (size_t i = simd_count * 8; i < size; ++i) {
            result += data[i];
        }
        
        return result;
    } else if constexpr (std::is_same_v<T, double>) {
        size_t simd_count = size / 4;
        __m256d sum_vec = _mm256_setzero_pd();
        
        for (size_t i = 0; i < simd_count; ++i) {
            __m256d data_vec = _mm256_loadu_pd(data + i * 4);
            sum_vec = _mm256_add_pd(sum_vec, data_vec);
        }
        
        double sum_array[4];
        _mm256_storeu_pd(sum_array, sum_vec);
        T result = T(0);
        for (int i = 0; i < 4; ++i) {
            result += T(sum_array[i]);
        }
        
        for (size_t i = simd_count * 4; i < size; ++i) {
            result += data[i];
        }
        
        return result;
    } else {
        // Integer types
        T result = T(0);
        for (size_t i = 0; i < size; ++i) {
            result += data[i];
        }
        return result;
    }
}

// ============================================================================
// Function Pointer Tables for JIT - Ultra Fast Dispatch
// ============================================================================

// Function pointer types
using CreateZerosFunc = void*(*)(size_t);
using CreateOnesFunc = void*(*)(size_t);
using GetElementFunc = void*(*)(void*, size_t);
using SetElementFunc = void(*)(void*, size_t, void*);
using ArrayAddFunc = void*(*)(void*, void*);
using ArraySumFunc = void*(*)(void*);

// Function tables indexed by DataType
constexpr CreateZerosFunc CREATE_ZEROS_FUNCS[] = {
    nullptr,  // UNKNOWN
    create_zeros_typed_array<int8_t>,    // INT8
    create_zeros_typed_array<int16_t>,   // INT16
    create_zeros_typed_array<int32_t>,   // INT32
    create_zeros_typed_array<int64_t>,   // INT64
    create_zeros_typed_array<uint8_t>,   // UINT8
    create_zeros_typed_array<uint16_t>,  // UINT16
    create_zeros_typed_array<uint32_t>,  // UINT32
    create_zeros_typed_array<uint64_t>,  // UINT64
    create_zeros_typed_array<float>,     // FLOAT32
    create_zeros_typed_array<double>,    // FLOAT64
};

constexpr CreateOnesFunc CREATE_ONES_FUNCS[] = {
    nullptr,  // UNKNOWN
    create_ones_typed_array<int8_t>,    // INT8
    create_ones_typed_array<int16_t>,   // INT16
    create_ones_typed_array<int32_t>,   // INT32
    create_ones_typed_array<int64_t>,   // INT64
    create_ones_typed_array<uint8_t>,   // UINT8
    create_ones_typed_array<uint16_t>,  // UINT16
    create_ones_typed_array<uint32_t>,  // UINT32
    create_ones_typed_array<uint64_t>,  // UINT64
    create_ones_typed_array<float>,     // FLOAT32
    create_ones_typed_array<double>,    // FLOAT64
};

// ============================================================================
// JIT Generated Code Examples
// ============================================================================

// What the JIT would generate for: Array.zeros([1000], { dtype: "float32" })
void* jit_generated_zeros_float32() {
    // Direct function call - no runtime dispatch
    return create_zeros_typed_array<float>(1000);
}

// What the JIT would generate for: arr.sum() where arr is known [int64] type
int64_t jit_generated_sum_int64(void* arr_ptr) {
    // Direct function call - no runtime dispatch
    return typed_array_sum<int64_t>(arr_ptr);
}

// What the JIT would generate for: arr1 + arr2 where both are [float32]
void* jit_generated_add_float32(void* left_ptr, void* right_ptr) {
    // Direct function call - no runtime dispatch
    return typed_array_add<float>(left_ptr, right_ptr);
}

// What the JIT would generate for: arr[index] where arr is [int32] and bounds checking disabled
int32_t jit_generated_get_int32_unchecked(void* arr_ptr, size_t index) {
    // Direct memory access - ultimate performance
    return static_cast<TypedArray<int32_t>*>(arr_ptr)->data()[index];
}

} // namespace runtime
