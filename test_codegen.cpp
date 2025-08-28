#include "x86_codegen_v2.h"
#include <iostream>

int main() {
    try {
        std::cout << "Creating X86CodeGenV2..." << std::endl;
        auto codegen = std::make_unique<X86CodeGenV2>();
        std::cout << "X86CodeGenV2 created successfully!" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return 1;
    }
}
