#include <iostream>
#include <string>
#include <memory>
#include "compiler.h"

int main() {
    std::cout << "=== Testing While Loop Parser Implementation ===" << std::endl;
    
    // Test 1: Simple while loop
    std::string test1 = R"(
        function test() {
            let i = 0;
            while (i < 10) {
                i = i + 1;
            }
            return i;
        }
    )";
    
    try {
        std::cout << "\n--- Test 1: Simple while loop parsing ---" << std::endl;
        std::cout << "Source:\n" << test1 << std::endl;
        
        GoTSCompiler compiler(Backend::X86_64);
        auto ast = compiler.parse_javascript(test1);
        
        if (!ast.empty()) {
            std::cout << "✅ While loop parsing successful! AST contains " << ast.size() << " nodes." << std::endl;
        } else {
            std::cout << "❌ While loop parsing failed - empty AST" << std::endl;
        }
        
    } catch (const std::exception& e) {
        std::cout << "❌ While loop parsing failed with exception: " << e.what() << std::endl;
    }
    
    // Test 2: While loop without parentheses (if supported)
    std::string test2 = R"(
        function test2() {
            let x = 5;
            while x > 0 {
                x = x - 1;
            }
            return x;
        }
    )";
    
    try {
        std::cout << "\n--- Test 2: While loop without parentheses ---" << std::endl;
        std::cout << "Source:\n" << test2 << std::endl;
        
        GoTSCompiler compiler(Backend::X86_64);
        auto ast = compiler.parse_javascript(test2);
        
        if (!ast.empty()) {
            std::cout << "✅ Parentheses-optional while loop parsing successful!" << std::endl;
        } else {
            std::cout << "❌ Parentheses-optional while loop parsing failed" << std::endl;
        }
        
    } catch (const std::exception& e) {
        std::cout << "❌ Parentheses-optional while loop parsing failed: " << e.what() << std::endl;
    }
    
    // Test 3: Nested while loops
    std::string test3 = R"(
        function test3() {
            let i = 0;
            let j = 0;
            while (i < 3) {
                j = 0;
                while (j < 2) {
                    j = j + 1;
                }
                i = i + 1;
            }
            return i + j;
        }
    )";
    
    try {
        std::cout << "\n--- Test 3: Nested while loops ---" << std::endl;
        std::cout << "Source:\n" << test3 << std::endl;
        
        GoTSCompiler compiler(Backend::X86_64);
        auto ast = compiler.parse_javascript(test3);
        
        if (!ast.empty()) {
            std::cout << "✅ Nested while loop parsing successful!" << std::endl;
        } else {
            std::cout << "❌ Nested while loop parsing failed" << std::endl;
        }
        
    } catch (const std::exception& e) {
        std::cout << "❌ Nested while loop parsing failed: " << e.what() << std::endl;
    }
    
    std::cout << "\n=== While Loop Parser Test Complete ===" << std::endl;
    return 0;
}
