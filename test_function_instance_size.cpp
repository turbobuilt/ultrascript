#include "compiler.h"
#include <iostream>
#include <fstream>

// Simple test for function instance size computation
int main() {
    std::cout << "🧮 Testing Function Instance Size Computation" << std::endl;
    std::cout << "=============================================" << std::endl;
    
    // Test 1: Function with no external scope access
    std::string simple_function_js = R"(
function simple() {
    var local = 10;
    return local + 5;
}
    )";
    
    std::cout << "\nTest 1: Simple function (no external scope access)" << std::endl;
    std::cout << "JavaScript code:" << std::endl;
    std::cout << simple_function_js << std::endl;
    
    try {
        std::cout << "\n🔍 Parsing with UltraScript..." << std::endl;
        
        GoTSCompiler compiler;
        auto parsed_result = compiler.parse_javascript(simple_function_js);
        
        if (parsed_result.empty()) {
            std::cout << "❌ Failed to parse JavaScript code" << std::endl;
            return 1;
        }
        
        std::cout << "✅ JavaScript successfully parsed! AST nodes: " << parsed_result.size() << std::endl;
        
        // Find the function
        FunctionDecl* simple_func = nullptr;
        for (auto& node : parsed_result) {
            if (auto* func = dynamic_cast<FunctionDecl*>(node.get())) {
                if (func->name == "simple") {
                    simple_func = func;
                    break;
                }
            }
        }
        
        if (!simple_func) {
            std::cout << "❌ Function 'simple' not found" << std::endl;
            return 1;
        }
        
        std::cout << "\n📊 Function Instance Size Analysis:" << std::endl;
        std::cout << "  - Function name: " << simple_func->name << std::endl;
        std::cout << "  - Function instance size: " << simple_func->function_instance_size << " bytes" << std::endl;
        
        if (simple_func->lexical_scope) {
            std::cout << "  - Scope depth: " << simple_func->lexical_scope->scope_depth << std::endl;
            std::cout << "  - Captured scopes: " << simple_func->lexical_scope->priority_sorted_parent_scopes.size() << std::endl;
            std::cout << "  - Expected size: 16 + (" << simple_func->lexical_scope->priority_sorted_parent_scopes.size() << " * 8) = " 
                      << (16 + simple_func->lexical_scope->priority_sorted_parent_scopes.size() * 8) << " bytes" << std::endl;
        }
        
    } catch (const std::exception& e) {
        std::cout << "❌ Error: " << e.what() << std::endl;
        return 1;
    }
    
    // Test 2: Function with closure (accesses external scope)
    std::string closure_function_js = R"(
var global_x = 10;
function outer() {
    var outer_y = 20;
    function inner() {
        console.log(global_x + outer_y);
    }
    return inner;
}
    )";
    
    std::cout << "\n\nTest 2: Closure function (accesses external scopes)" << std::endl;
    std::cout << "JavaScript code:" << std::endl;
    std::cout << closure_function_js << std::endl;
    
    try {
        std::cout << "\n🔍 Parsing with UltraScript..." << std::endl;
        
        GoTSCompiler compiler;
        auto parsed_result = compiler.parse_javascript(closure_function_js);
        
        if (parsed_result.empty()) {
            std::cout << "❌ Failed to parse JavaScript code" << std::endl;
            return 1;
        }
        
        std::cout << "✅ JavaScript successfully parsed! AST nodes: " << parsed_result.size() << std::endl;
        
        // Find the inner function
        FunctionDecl* inner_func = nullptr;
        
        // Search through all nodes recursively
        std::function<void(ASTNode*)> search_functions = [&](ASTNode* node) {
            if (auto* func = dynamic_cast<FunctionDecl*>(node)) {
                if (func->name == "inner") {
                    inner_func = func;
                    return;
                }
                // Search within function body
                for (auto& stmt : func->body) {
                    search_functions(stmt.get());
                }
            }
        };
        
        for (auto& node : parsed_result) {
            search_functions(node.get());
        }
        
        if (!inner_func) {
            std::cout << "❌ Function 'inner' not found" << std::endl;
            return 1;
        }
        
        std::cout << "\n📊 Function Instance Size Analysis for 'inner':" << std::endl;
        std::cout << "  - Function name: " << inner_func->name << std::endl;
        std::cout << "  - Function instance size: " << inner_func->function_instance_size << " bytes" << std::endl;
        
        if (inner_func->lexical_scope) {
            std::cout << "  - Scope depth: " << inner_func->lexical_scope->scope_depth << std::endl;
            std::cout << "  - Captured scopes: " << inner_func->lexical_scope->priority_sorted_parent_scopes.size() << std::endl;
            std::cout << "  - Priority sorted depths: ";
            for (int depth : inner_func->lexical_scope->priority_sorted_parent_scopes) {
                std::cout << depth << " ";
            }
            std::cout << std::endl;
            std::cout << "  - Expected size: 16 + (" << inner_func->lexical_scope->priority_sorted_parent_scopes.size() << " * 8) = " 
                      << (16 + inner_func->lexical_scope->priority_sorted_parent_scopes.size() * 8) << " bytes" << std::endl;
        }
        
    } catch (const std::exception& e) {
        std::cout << "❌ Error: " << e.what() << std::endl;
        return 1;
    }
    
    std::cout << "\n🎉 Function instance size test completed successfully!" << std::endl;
    return 0;
}
