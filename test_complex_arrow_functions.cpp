#include "compiler.h"
#include <iostream>
#include <fstream>

int main() {
    std::cout << "🏹 Testing Complex Arrow Functions" << std::endl;
    std::cout << "==================================" << std::endl;
    
    try {
        // Simple inline test without complex dependencies
        std::string js_code = R"(
function testComplexArrowFunctions() {
    // Multiple parameter arrow function with block body
    const complexArrow = (a, b) => {
        let result = a + b;
        return result * 2;
    };
    
    // Single parameter without parentheses  
    const simpleArrow = x => x + 1;
    
    // No parameter arrow function
    const noParamArrow = () => "hello";
}
)";
        
        std::cout << "JavaScript code:" << std::endl << js_code << std::endl;
        
        std::cout << "🔍 Parsing with REAL UltraScript GoTSCompiler..." << std::endl;
        
        // Use real GoTSCompiler
        GoTSCompiler compiler;
        auto result = compiler.parse_javascript(js_code);
        
        std::cout << "✅ JavaScript successfully parsed! AST nodes: " << result.size() << std::endl;
        
        // Find the function
        for (const auto& node : result) {
            if (auto func = dynamic_cast<FunctionDecl*>(node.get())) {
                std::cout << "✅ Found function: " << func->name << std::endl;
                
                // Count arrow functions in the AST
                int arrow_count = 0;
                std::function<void(ASTNode*)> count_arrows = [&](ASTNode* node) {
                    if (auto arrow = dynamic_cast<ArrowFunction*>(node)) {
                        arrow_count++;
                        std::cout << "✅ Found arrow function with " << arrow->parameters.size() << " parameter(s)" << std::endl;
                        if (arrow->is_single_expression) {
                            std::cout << "   - Type: Single expression" << std::endl;
                        } else {
                            std::cout << "   - Type: Block body with " << arrow->body.size() << " statement(s)" << std::endl;
                        }
                    }
                    
                    // Recursively check child nodes
                    if (auto assignment = dynamic_cast<Assignment*>(node)) {
                        if (assignment->value) count_arrows(assignment->value.get());
                    } else if (auto func_decl = dynamic_cast<FunctionDecl*>(node)) {
                        for (auto& stmt : func_decl->body) {
                            count_arrows(stmt.get());
                        }
                    }
                };
                
                count_arrows(func);
                
                std::cout << "✅ Total arrow functions found: " << arrow_count << std::endl;
                break;
            }
        }
        
        std::cout << std::endl << "🎉 Complex arrow function test completed successfully!" << std::endl;
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "❌ Error: " << e.what() << std::endl;
        return 1;
    }
}
                
                // Count arrow functions in the AST
                int arrow_count = 0;
                std::function<void(ASTNode*)> count_arrows = [&](ASTNode* node) {
                    if (auto arrow = dynamic_cast<ArrowFunction*>(node)) {
                        arrow_count++;
                        std::cout << "✅ Found arrow function with " << arrow->parameters.size() << " parameter(s)" << std::endl;
                        if (arrow->is_single_expression) {
                            std::cout << "   - Type: Single expression" << std::endl;
                        } else {
                            std::cout << "   - Type: Block body with " << arrow->body.size() << " statement(s)" << std::endl;
                        }
                    }
                    
                    // Recursively check child nodes
                    if (auto assignment = dynamic_cast<Assignment*>(node)) {
                        if (assignment->value) count_arrows(assignment->value.get());
                    } else if (auto func_decl = dynamic_cast<FunctionDecl*>(node)) {
                        for (auto& stmt : func_decl->body) {
                            count_arrows(stmt.get());
                        }
                    }
                };
                
                count_arrows(func);
                
                std::cout << "✅ Total arrow functions found: " << arrow_count << std::endl;
                break;
            }
        }
        
        std::cout << std::endl << "🎉 Complex arrow function test completed successfully!" << std::endl;
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "❌ Error: " << e.what() << std::endl;
        return 1;
    }
}
