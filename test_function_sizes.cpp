#include "compiler.h"
#include "lexer.h" 
#include "parser.h"
#include "simple_lexical_scope.h"
#include <iostream>

int main() {
    try {
        std::cout << "Testing function instance size computation...\n";
        
        std::string test_code = R"(
function test() {
    let x = 42;
    return x;
}

function withClosure() {
    let a = 10;
    
    function inner() {
        let b = 20;
        
        function deepInner() {
            return a + b + 30;
        }
        
        return deepInner();
    }
    
    return inner();
}

let arrow = () => {
    let y = 100;
    return y;
};
        )";
        
        Lexer lexer(test_code);
        Parser parser(lexer);
        
        auto ast = parser.parse();
        
        std::cout << "AST parsed successfully\n";
        
        // Find function declarations in the AST and print their sizes
        for (const auto& node : ast) {
            if (auto funcDecl = dynamic_cast<FunctionDecl*>(node.get())) {
                std::cout << "Function '" << funcDecl->name 
                         << "' instance size: " << funcDecl->function_instance_size 
                         << " bytes\n";
            }
            else if (auto assignment = dynamic_cast<Assignment*>(node.get())) {
                if (auto arrowFunc = dynamic_cast<ArrowFunction*>(assignment->value.get())) {
                    std::cout << "Arrow function '" << assignment->name 
                             << "' instance size: " << arrowFunc->function_instance_size 
                             << " bytes\n";
                }
            }
        }
        
        std::cout << "Test completed successfully!\n";
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}
