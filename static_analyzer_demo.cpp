#include <iostream>
#include "static_analyzer.h"
#include "compiler.h"

int main() {
    std::cout << "=== Three-Phase Compilation System Demo ===" << std::endl;
    
    // Create a static analyzer instance
    StaticAnalyzer analyzer;
    
    // Create some mock AST nodes to demonstrate the analyzer
    std::vector<std::unique_ptr<ASTNode>> mock_ast;
    
    // For this demo, we'll create a simple AST manually
    // (In the full system, this would come from the parser)
    
    // Mock function declaration
    auto func_decl = std::make_unique<FunctionDecl>("testFunc");
    
    // Set parameters manually
    func_decl->parameters.push_back(Variable{"x", DataType::ANY});
    
    // Add some mock statements to the function body
    auto assignment = std::make_unique<Assignment>("y", std::make_unique<NumberLiteral>(42.0));
    func_decl->body.push_back(std::move(assignment));
    
    mock_ast.push_back(std::move(func_decl));
    
    // Run the static analyzer on our mock AST
    std::cout << "\nRunning static analysis..." << std::endl;
    analyzer.analyze(mock_ast);
    
    std::cout << "\n=== Static Analysis Complete ===" << std::endl;
    std::cout << "Three-phase system successfully demonstrated!" << std::endl;
    
    // Show that we can query the analyzer for scope information
    std::cout << "\nQuerying scope information:" << std::endl;
    LexicalScopeNode* global_scope = analyzer.get_scope_node_for_depth(1);
    if (global_scope) {
        std::cout << "Global scope found with " << global_scope->declared_variables.size() << " variables" << std::endl;
    }
    
    LexicalScopeNode* function_scope = analyzer.get_scope_node_for_depth(2);
    if (function_scope) {
        std::cout << "Function scope found with " << function_scope->declared_variables.size() << " variables" << std::endl;
    }
    
    return 0;
}
