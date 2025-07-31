#pragma once

#include <vector>
#include <memory>
#include <initializer_list>
#include <cstring>
#include <stdexcept>
#include <functional>
#include <algorithm>
#include <random>
#include <cmath>
#include <sstream>
#include <variant>
#include <type_traits>

namespace ultraScript {

// Forward declare DataType from compiler.h
enum class DataType;

// Slice type for PyTorch-style slicing
struct Slice {
    int64_t start;
    int64_t end;
    int64_t step;
    bool start_specified;
    bool end_specified;
    bool step_specified;
    
    Slice() : start(0), end(-1), step(1), start_specified(false), end_specified(false), step_specified(false) {}
    
    Slice(int64_t start_val) 
        : start(start_val), end(-1), step(1), start_specified(true), end_specified(false), step_specified(false) {}
    
    Slice(int64_t start_val, int64_t end_val) 
        : start(start_val), end(end_val), step(1), start_specified(true), end_specified(true), step_specified(false) {}
    
    Slice(int64_t start_val, int64_t end_val, int64_t step_val) 
        : start(start_val), end(end_val), step(step_val), start_specified(true), end_specified(true), step_specified(true) {}
    
    // Resolve slice against actual dimension size
    void resolve(size_t dim_size) {
        if (!start_specified) start = 0;
        if (!end_specified) end = static_cast<int64_t>(dim_size);
        if (!step_specified) step = 1;
        
        // Handle negative indices
        if (start < 0) start += static_cast<int64_t>(dim_size);
        if (end < 0) end += static_cast<int64_t>(dim_size);
        
        // Clamp to valid range
        start = std::max(0L, std::min(start, static_cast<int64_t>(dim_size)));
        end = std::max(0L, std::min(end, static_cast<int64_t>(dim_size)));
    }
};

// Type-safe value holder for dynamic arrays
struct DynamicValue {
    std::variant<
        int8_t, int16_t, int32_t, int64_t,
        uint8_t, uint16_t, uint32_t, uint64_t,
        float, double,
        bool, std::string
    > value;
    DataType type;
    
    DynamicValue() : value(0.0), type(static_cast<DataType>(11)) {} // FLOAT64
    
    template<typename T>
    DynamicValue(T val) : value(val) {
        if constexpr (std::is_same_v<T, int8_t>) type = static_cast<DataType>(2);
        else if constexpr (std::is_same_v<T, int16_t>) type = static_cast<DataType>(3);
        else if constexpr (std::is_same_v<T, int32_t>) type = static_cast<DataType>(4);
        else if constexpr (std::is_same_v<T, int64_t>) type = static_cast<DataType>(5);
        else if constexpr (std::is_same_v<T, uint8_t>) type = static_cast<DataType>(6);
        else if constexpr (std::is_same_v<T, uint16_t>) type = static_cast<DataType>(7);
        else if constexpr (std::is_same_v<T, uint32_t>) type = static_cast<DataType>(8);
        else if constexpr (std::is_same_v<T, uint64_t>) type = static_cast<DataType>(9);
        else if constexpr (std::is_same_v<T, float>) type = static_cast<DataType>(10);
        else if constexpr (std::is_same_v<T, double>) type = static_cast<DataType>(11);
        else if constexpr (std::is_same_v<T, bool>) type = static_cast<DataType>(12);
        else if constexpr (std::is_same_v<T, std::string>) type = static_cast<DataType>(13);
        else type = static_cast<DataType>(11); // Default to FLOAT64
    }
    
    template<typename T>
    T as() const {
        return std::get<T>(value);
    }
    
    // Convert to double for untyped operations
    double to_number() const {
        return std::visit([](auto&& arg) -> double {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_arithmetic_v<T>) {
                return static_cast<double>(arg);
            } else {
                return 0.0; // String or other non-numeric types
            }
        }, value);
    }
    
    std::string to_string() const {
        return std::visit([](auto&& arg) -> std::string {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, std::string>) {
                return arg;
            } else if constexpr (std::is_arithmetic_v<T>) {
                return std::to_string(arg);
            } else {
                return "";
            }
        }, value);
    }
};

// ============================================================================
// TYPED ARRAYS - Ultra-High Performance, Compile-Time Type Specialized
// ============================================================================

template<typename T>
class TypedArray {
private:
    std::unique_ptr<T[]> data_;
    std::vector<size_t> shape_;
    std::vector<size_t> strides_;
    size_t capacity_;
    size_t size_;
    
    // Calculate strides for multi-dimensional access
    void calculate_strides() {
        strides_.resize(shape_.size());
        if (shape_.empty()) return;
        
        strides_[shape_.size() - 1] = 1;
        for (int i = static_cast<int>(shape_.size()) - 2; i >= 0; --i) {
            strides_[i] = strides_[i + 1] * shape_[i + 1];
        }
    }
    
    // Get flat index from multi-dimensional indices
    size_t get_flat_index(const std::vector<size_t>& indices) const {
        if (indices.size() != shape_.size()) {
            throw std::runtime_error("Dimension mismatch");
        }
        
        size_t flat_index = 0;
        for (size_t i = 0; i < indices.size(); ++i) {
            if (indices[i] >= shape_[i]) {
                throw std::runtime_error("Index out of bounds");
            }
            flat_index += indices[i] * strides_[i];
        }
        return flat_index;
    }
    
    // Ultra-fast capacity expansion - no branching, SIMD-friendly
    void ensure_capacity(size_t new_size) {
        if (__builtin_expect(new_size <= capacity_, 1)) return;
        
        size_t new_capacity = capacity_ == 0 ? 8 : capacity_;
        while (new_capacity < new_size) {
            new_capacity <<= 1;  // Bit shift for 2x growth
        }
        
        auto new_data = std::make_unique<T[]>(new_capacity);
        if (data_) {
            // Use fastest possible memory copy
            if constexpr (std::is_trivially_copyable_v<T>) {
                std::memcpy(new_data.get(), data_.get(), size_ * sizeof(T));
            } else {
                for (size_t i = 0; i < size_; ++i) {
                    new_data[i] = std::move(data_[i]);
                }
            }
        }
        data_ = std::move(new_data);
        capacity_ = new_capacity;
    }

public:
    // Constructors
    TypedArray() : capacity_(0), size_(0) {
        shape_ = {0};
    }
    
    // Create with specific shape
    explicit TypedArray(const std::vector<size_t>& shape) : shape_(shape), capacity_(0), size_(0) {
        size_t total_size = 1;
        for (size_t dim : shape) {
            total_size *= dim;
        }
        size_ = total_size;
        
        ensure_capacity(total_size);
        
        // Ultra-fast zero initialization
        if constexpr (std::is_trivially_constructible_v<T>) {
            std::memset(data_.get(), 0, total_size * sizeof(T));
        } else {
            std::fill_n(data_.get(), total_size, T{});
        }
        
        calculate_strides();
    }
    
    // Create with shape and values
    TypedArray(const std::vector<size_t>& shape, const std::vector<T>& values) 
        : shape_(shape), capacity_(0), size_(0) {
        size_t expected_size = 1;
        for (size_t dim : shape) {
            expected_size *= dim;
        }
        if (values.size() != expected_size) {
            throw std::runtime_error("Data size doesn't match shape");
        }
        
        size_ = expected_size;
        ensure_capacity(size_);
        
        // Ultra-fast copy
        if constexpr (std::is_trivially_copyable_v<T>) {
            std::memcpy(data_.get(), values.data(), size_ * sizeof(T));
        } else {
            std::copy(values.begin(), values.end(), data_.get());
        }
        
        calculate_strides();
    }
    
    // Create from initializer list
    TypedArray(std::initializer_list<T> values) : capacity_(0), size_(values.size()) {
        shape_ = {values.size()};
        ensure_capacity(size_);
        std::copy(values.begin(), values.end(), data_.get());
        calculate_strides();
    }
    
    // Properties - all inline for zero overhead
    const std::vector<size_t>& shape() const { return shape_; }
    size_t length() const { return shape_.empty() ? 0 : shape_[0]; }
    size_t size() const { return size_; }
    size_t ndim() const { return shape_.size(); }
    bool empty() const { return size_ == 0; }
    bool is_1d() const { return shape_.size() == 1; }
    
    // Ultra-fast element access - zero overhead
    T& operator[](size_t index) {
        // No bounds checking in release mode for maximum performance
        return data_[index];
    }
    
    const T& operator[](size_t index) const {
        return data_[index];
    }
    
    // Multi-dimensional access
    T& at(const std::vector<size_t>& indices) {
        return data_[get_flat_index(indices)];
    }
    
    const T& at(const std::vector<size_t>& indices) const {
        return data_[get_flat_index(indices)];
    }
    
    // Ultra-fast 1D push - specialized for each type
    template<typename U>
    void push(U value) {
        if (!is_1d()) {
            throw std::runtime_error("push() only works on 1D arrays");
        }
        
        ensure_capacity(size_ + 1);
        
        // Type conversion with compile-time optimization
        if constexpr (std::is_convertible_v<U, T>) {
            data_[size_++] = static_cast<T>(value);
        } else {
            throw std::runtime_error("Type conversion not supported");
        }
        
        shape_[0] = size_;
    }
    
    // Ultra-fast pop
    T pop() {
        if (!is_1d() || size_ == 0) {
            throw std::runtime_error("pop() only works on non-empty 1D arrays");
        }
        
        T value = std::move(data_[--size_]);
        shape_[0] = size_;
        return value;
    }
    
    // SIMD-optimized statistical operations
    double sum() const {
        if (size_ == 0) return 0.0;
        
        // Use the fastest accumulator type for the specific T
        if constexpr (std::is_integral_v<T>) {
            using AccumType = std::conditional_t<sizeof(T) <= 4, int64_t, T>;
            AccumType acc = 0;
            
            // SIMD vectorization hint for compiler
            #pragma GCC ivdep
            for (size_t i = 0; i < size_; ++i) {
                acc += data_[i];
            }
            return static_cast<double>(acc);
        } else {
            double acc = 0.0;
            #pragma GCC ivdep
            for (size_t i = 0; i < size_; ++i) {
                acc += static_cast<double>(data_[i]);
            }
            return acc;
        }
    }
    
    double mean() const {
        if (size_ == 0) throw std::runtime_error("Cannot compute mean of empty array");
        return sum() / static_cast<double>(size_);
    }
    
    T max() const {
        if (size_ == 0) throw std::runtime_error("Cannot compute max of empty array");
        return *std::max_element(data_.get(), data_.get() + size_);
    }
    
    T min() const {
        if (size_ == 0) throw std::runtime_error("Cannot compute min of empty array");
        return *std::min_element(data_.get(), data_.get() + size_);
    }
    
    // Direct data access for ultimate performance
    T* raw_data() { return data_.get(); }
    const T* raw_data() const { return data_.get(); }
    
    // Static factory methods
    static TypedArray<T> zeros(const std::vector<size_t>& shape) {
        return TypedArray<T>(shape);
    }
    
    static TypedArray<T> ones(const std::vector<size_t>& shape) {
        TypedArray<T> result(shape);
        std::fill_n(result.data_.get(), result.size_, T(1));
        return result;
    }
    
    static TypedArray<T> full(const std::vector<size_t>& shape, T value) {
        TypedArray<T> result(shape);
        std::fill_n(result.data_.get(), result.size_, value);
        return result;
    }
    
    static TypedArray<T> arange(T start, T stop, T step = T(1)) {
        size_t num_elements = static_cast<size_t>((stop - start) / step);
        TypedArray<T> result({num_elements});
        
        for (size_t i = 0; i < num_elements; ++i) {
            result.data_[i] = start + static_cast<T>(i) * step;
        }
        
        return result;
    }
    
    static TypedArray<T> linspace(T start, T stop, size_t num = 50) {
        TypedArray<T> result({num});
        if (num == 0) return result;
        
        if (num == 1) {
            result.data_[0] = start;
            return result;
        }
        
        T step = (stop - start) / static_cast<T>(num - 1);
        for (size_t i = 0; i < num; ++i) {
            result.data_[i] = start + static_cast<T>(i) * step;
        }
        
        return result;
    }
    
    // String representation
    std::string toString() const {
        std::ostringstream oss;
        oss << "[";
        
        for (size_t i = 0; i < std::min(size_, size_t(10)); ++i) {
            if (i > 0) oss << ", ";
            oss << data_[i];
        }
        
        if (size_ > 10) {
            oss << ", ...";
        }
        
        oss << "]";
        return oss.str();
    }
};

// ============================================================================
// DYNAMIC ARRAY - Flexible but still optimized
// ============================================================================

class DynamicArray {
private:
    std::vector<DynamicValue> data_;
    std::vector<size_t> shape_;
    std::vector<size_t> strides_;
    
    // Calculate strides for multi-dimensional access
    void calculate_strides() {
        strides_.resize(shape_.size());
        if (shape_.empty()) return;
        
        strides_[shape_.size() - 1] = 1;
        for (int i = static_cast<int>(shape_.size()) - 2; i >= 0; --i) {
            strides_[i] = strides_[i + 1] * shape_[i + 1];
        }
    }
    
    // Get flat index from multi-dimensional indices
    size_t get_flat_index(const std::vector<size_t>& indices) const {
        if (indices.size() != shape_.size()) {
            throw std::runtime_error("Dimension mismatch");
        }
        
        size_t flat_index = 0;
        for (size_t i = 0; i < indices.size(); ++i) {
            if (indices[i] >= shape_[i]) {
                throw std::runtime_error("Index out of bounds");
            }
            flat_index += indices[i] * strides_[i];
        }
        return flat_index;
    }

public:
    // Constructors
    DynamicArray() {
        shape_ = {0};
    }
    
    // Create from initializer list
    DynamicArray(std::initializer_list<double> values) {
        shape_ = {values.size()};
        data_.reserve(values.size());
        for (const auto& val : values) {
            data_.emplace_back(val);
        }
        calculate_strides();
    }
    
    // Properties
    const std::vector<size_t>& shape() const { return shape_; }
    size_t length() const { return shape_.empty() ? 0 : shape_[0]; }
    size_t size() const { return data_.size(); }
    size_t ndim() const { return shape_.size(); }
    bool empty() const { return data_.empty(); }
    bool is_1d() const { return shape_.size() == 1; }
    
    // Element access
    DynamicValue& operator[](size_t index) {
        return data_[index];
    }
    
    const DynamicValue& operator[](size_t index) const {
        return data_[index];
    }
    
    // Multi-dimensional access
    DynamicValue& at(const std::vector<size_t>& indices) {
        return data_[get_flat_index(indices)];
    }
    
    const DynamicValue& at(const std::vector<size_t>& indices) const {
        return data_[get_flat_index(indices)];
    }
    
    // Operations
    template<typename T>
    void push(T value) {
        if (!is_1d()) {
            throw std::runtime_error("push() only works on 1D arrays");
        }
        
        data_.emplace_back(value);
        shape_[0] = data_.size();
    }
    
    DynamicValue pop() {
        if (!is_1d() || data_.empty()) {
            throw std::runtime_error("pop() only works on non-empty 1D arrays");
        }
        
        auto value = data_.back();
        data_.pop_back();
        shape_[0] = data_.size();
        return value;
    }
    
    // Statistical operations
    double sum() const {
        double result = 0.0;
        for (const auto& val : data_) {
            result += val.to_number();
        }
        return result;
    }
    
    double mean() const {
        if (data_.empty()) throw std::runtime_error("Cannot compute mean of empty array");
        return sum() / static_cast<double>(data_.size());
    }
    
    double max() const {
        if (data_.empty()) throw std::runtime_error("Cannot compute max of empty array");
        double max_val = data_[0].to_number();
        for (size_t i = 1; i < data_.size(); ++i) {
            max_val = std::max(max_val, data_[i].to_number());
        }
        return max_val;
    }
    
    double min() const {
        if (data_.empty()) throw std::runtime_error("Cannot compute min of empty array");
        double min_val = data_[0].to_number();
        for (size_t i = 1; i < data_.size(); ++i) {
            min_val = std::min(min_val, data_[i].to_number());
        }
        return min_val;
    }
    
    // String representation
    std::string toString() const {
        std::ostringstream oss;
        oss << "[";
        
        for (size_t i = 0; i < std::min(data_.size(), size_t(10)); ++i) {
            if (i > 0) oss << ", ";
            oss << data_[i].to_string();
        }
        
        if (data_.size() > 10) {
            oss << ", ...";
        }
        
        oss << "]";
        return oss.str();
    }
    
    // Static factory methods
    static DynamicArray zeros(size_t size) {
        DynamicArray result;
        result.shape_ = {size};
        result.data_.resize(size, DynamicValue(0.0));
        result.calculate_strides();
        return result;
    }
    
    static DynamicArray ones(size_t size) {
        DynamicArray result;
        result.shape_ = {size};
        result.data_.resize(size, DynamicValue(1.0));
        result.calculate_strides();
        return result;
    }
    
    static DynamicArray arange(double start, double stop, double step = 1.0) {
        size_t num_elements = static_cast<size_t>((stop - start) / step);
        DynamicArray result;
        result.shape_ = {num_elements};
        result.data_.reserve(num_elements);
        
        for (size_t i = 0; i < num_elements; ++i) {
            double value = start + static_cast<double>(i) * step;
            result.data_.emplace_back(value);
        }
        
        result.calculate_strides();
        return result;
    }
    
    static DynamicArray linspace(double start, double stop, size_t num = 50) {
        DynamicArray result;
        result.shape_ = {num};
        result.data_.reserve(num);
        
        if (num == 0) {
            result.calculate_strides();
            return result;
        }
        
        if (num == 1) {
            result.data_.emplace_back(start);
            result.calculate_strides();
            return result;
        }
        
        double step = (stop - start) / static_cast<double>(num - 1);
        for (size_t i = 0; i < num; ++i) {
            double value = start + static_cast<double>(i) * step;
            result.data_.emplace_back(value);
        }
        
        result.calculate_strides();
        return result;
    }
};

// ============================================================================
// TYPE ALIASES for easy use in compiler/parser
// ============================================================================

// Typed array aliases for all supported types
using Int8Array = TypedArray<int8_t>;
using Int16Array = TypedArray<int16_t>;
using Int32Array = TypedArray<int32_t>;
using Int64Array = TypedArray<int64_t>;
using Uint8Array = TypedArray<uint8_t>;
using Uint16Array = TypedArray<uint16_t>;
using Uint32Array = TypedArray<uint32_t>;
using Uint64Array = TypedArray<uint64_t>;
using Float32Array = TypedArray<float>;
using Float64Array = TypedArray<double>;

// Default Array alias - compiler will choose based on type inference
using Array = DynamicArray;  // For untyped arrays

} // namespace ultraScript
