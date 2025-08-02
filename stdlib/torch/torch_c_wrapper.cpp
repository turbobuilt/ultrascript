#include <torch/torch.h>
#include <iostream>
#include <memory>
#include <stdexcept>

// C wrapper for LibTorch to make it accessible from UltraScript FFI
// This provides simple C functions that call the complex C++ LibTorch API

extern "C" {

// Error handling
static std::string last_error;

const char* torch_get_last_error() {
    return last_error.c_str();
}

void torch_clear_error() {
    last_error.clear();
}

// Helper function to set error and return null
void* set_error_and_return_null(const std::string& error) {
    last_error = error;
    return nullptr;
}

// Tensor creation functions
void* torch_ones_1d(int64_t size0) {
    try {
        auto tensor = torch::ones({size0});
        return new torch::Tensor(std::move(tensor));
    } catch (const std::exception& e) {
        return set_error_and_return_null(std::string("torch_ones_1d failed: ") + e.what());
    }
}

void* torch_ones_2d(int64_t size0, int64_t size1) {
    try {
        auto tensor = torch::ones({size0, size1});
        return new torch::Tensor(std::move(tensor));
    } catch (const std::exception& e) {
        return set_error_and_return_null(std::string("torch_ones_2d failed: ") + e.what());
    }
}

void* torch_zeros_2d(int64_t size0, int64_t size1) {
    try {
        auto tensor = torch::zeros({size0, size1});
        return new torch::Tensor(std::move(tensor));
    } catch (const std::exception& e) {
        return set_error_and_return_null(std::string("torch_zeros_2d failed: ") + e.what());
    }
}

void* torch_randn_2d(int64_t size0, int64_t size1) {
    try {
        auto tensor = torch::randn({size0, size1});
        return new torch::Tensor(std::move(tensor));
    } catch (const std::exception& e) {
        return set_error_and_return_null(std::string("torch_randn_2d failed: ") + e.what());
    }
}

// Tensor operations
void* torch_add(void* tensor_a, void* tensor_b) {
    try {
        if (!tensor_a || !tensor_b) {
            return set_error_and_return_null("torch_add: null tensor pointer");
        }
        
        torch::Tensor* a = static_cast<torch::Tensor*>(tensor_a);
        torch::Tensor* b = static_cast<torch::Tensor*>(tensor_b);
        
        auto result = a->add(*b);
        return new torch::Tensor(std::move(result));
    } catch (const std::exception& e) {
        return set_error_and_return_null(std::string("torch_add failed: ") + e.what());
    }
}

void* torch_sub(void* tensor_a, void* tensor_b) {
    try {
        if (!tensor_a || !tensor_b) {
            return set_error_and_return_null("torch_sub: null tensor pointer");
        }
        
        torch::Tensor* a = static_cast<torch::Tensor*>(tensor_a);
        torch::Tensor* b = static_cast<torch::Tensor*>(tensor_b);
        
        auto result = a->sub(*b);
        return new torch::Tensor(std::move(result));
    } catch (const std::exception& e) {
        return set_error_and_return_null(std::string("torch_sub failed: ") + e.what());
    }
}

void* torch_mul(void* tensor_a, void* tensor_b) {
    try {
        if (!tensor_a || !tensor_b) {
            return set_error_and_return_null("torch_mul: null tensor pointer");
        }
        
        torch::Tensor* a = static_cast<torch::Tensor*>(tensor_a);
        torch::Tensor* b = static_cast<torch::Tensor*>(tensor_b);
        
        auto result = a->mul(*b);
        return new torch::Tensor(std::move(result));
    } catch (const std::exception& e) {
        return set_error_and_return_null(std::string("torch_mul failed: ") + e.what());
    }
}

void* torch_matmul(void* tensor_a, void* tensor_b) {
    try {
        if (!tensor_a || !tensor_b) {
            return set_error_and_return_null("torch_matmul: null tensor pointer");
        }
        
        torch::Tensor* a = static_cast<torch::Tensor*>(tensor_a);
        torch::Tensor* b = static_cast<torch::Tensor*>(tensor_b);
        
        auto result = torch::matmul(*a, *b);
        return new torch::Tensor(std::move(result));
    } catch (const std::exception& e) {
        return set_error_and_return_null(std::string("torch_matmul failed: ") + e.what());
    }
}

// Tensor properties
int64_t torch_tensor_ndim(void* tensor) {
    try {
        if (!tensor) {
            last_error = "torch_tensor_ndim: null tensor pointer";
            return -1;
        }
        
        torch::Tensor* t = static_cast<torch::Tensor*>(tensor);
        return t->ndimension();
    } catch (const std::exception& e) {
        last_error = std::string("torch_tensor_ndim failed: ") + e.what();
        return -1;
    }
}

int64_t torch_tensor_size(void* tensor, int64_t dim) {
    try {
        if (!tensor) {
            last_error = "torch_tensor_size: null tensor pointer";
            return -1;
        }
        
        torch::Tensor* t = static_cast<torch::Tensor*>(tensor);
        return t->size(dim);
    } catch (const std::exception& e) {
        last_error = std::string("torch_tensor_size failed: ") + e.what();
        return -1;
    }
}

int64_t torch_tensor_numel(void* tensor) {
    try {
        if (!tensor) {
            last_error = "torch_tensor_numel: null tensor pointer";
            return -1;
        }
        
        torch::Tensor* t = static_cast<torch::Tensor*>(tensor);
        return t->numel();
    } catch (const std::exception& e) {
        last_error = std::string("torch_tensor_numel failed: ") + e.what();
        return -1;
    }
}

// Tensor utilities
void torch_tensor_print(void* tensor) {
    try {
        if (!tensor) {
            std::cout << "Tensor(null)" << std::endl;
            return;
        }
        
        torch::Tensor* t = static_cast<torch::Tensor*>(tensor);
        std::cout << *t << std::endl;
    } catch (const std::exception& e) {
        std::cout << "Error printing tensor: " << e.what() << std::endl;
    }
}

void* torch_tensor_clone(void* tensor) {
    try {
        if (!tensor) {
            return set_error_and_return_null("torch_tensor_clone: null tensor pointer");
        }
        
        torch::Tensor* t = static_cast<torch::Tensor*>(tensor);
        auto cloned = t->clone();
        return new torch::Tensor(std::move(cloned));
    } catch (const std::exception& e) {
        return set_error_and_return_null(std::string("torch_tensor_clone failed: ") + e.what());
    }
}

// Memory management
void torch_tensor_free(void* tensor) {
    try {
        if (tensor) {
            torch::Tensor* t = static_cast<torch::Tensor*>(tensor);
            delete t;
        }
    } catch (const std::exception& e) {
        last_error = std::string("torch_tensor_free failed: ") + e.what();
    }
}

// Utility functions
void torch_manual_seed(int64_t seed) {
    try {
        torch::manual_seed(seed);
    } catch (const std::exception& e) {
        last_error = std::string("torch_manual_seed failed: ") + e.what();
    }
}

bool torch_cuda_is_available() {
    try {
        return torch::cuda::is_available();
    } catch (const std::exception& e) {
        last_error = std::string("torch_cuda_is_available failed: ") + e.what();
        return false;
    }
}

int64_t torch_cuda_device_count() {
    try {
        if (torch::cuda::is_available()) {
            return torch::cuda::device_count();
        }
        return 0;
    } catch (const std::exception& e) {
        last_error = std::string("torch_cuda_device_count failed: ") + e.what();
        return 0;
    }
}

} // extern "C"
