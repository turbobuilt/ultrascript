#include "compiler.h"
#include <iostream>

int main() {
    std::cout << "🔍 Testing Arrow Function Lexer" << std::endl;
    
    std::string test_code = "x => x + 1";
    
    std::cout << "Input: " << test_code << std::endl;
    std::cout << "Tokens:" << std::endl;
    
    Lexer lexer(test_code);
    auto tokens = lexer.tokenize();
    
    for (size_t i = 0; i < tokens.size(); ++i) {
        const auto& token = tokens[i];
        std::cout << "  " << i << ": " << static_cast<int>(token.type) << " '" << token.value << "'";
        
        // Print token type name for clarity
        switch (token.type) {
            case TokenType::IDENTIFIER: std::cout << " (IDENTIFIER)"; break;
            case TokenType::ARROW: std::cout << " (ARROW)"; break;
            case TokenType::ASSIGN: std::cout << " (ASSIGN)"; break;
            case TokenType::GREATER: std::cout << " (GREATER)"; break;
            case TokenType::EQUAL: std::cout << " (EQUAL)"; break;
            case TokenType::PLUS: std::cout << " (PLUS)"; break;
            case TokenType::NUMBER: std::cout << " (NUMBER)"; break;
            case TokenType::EOF_TOKEN: std::cout << " (EOF)"; break;
            default: std::cout << " (OTHER " << static_cast<int>(token.type) << ")"; break;
        }
        
        std::cout << std::endl;
    }
    
    return 0;
}
