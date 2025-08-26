// Arrow function parsing methods
std::unique_ptr<ArrowFunction> Parser::parse_arrow_function_from_identifier(const std::string& param_name) {
    // We're parsing: identifier => body
    if (!match(TokenType::ARROW)) {
        throw std::runtime_error("Expected '=>' in arrow function");
    }
    
    auto arrow_func = std::make_unique<ArrowFunction>();
    
    // Add the single parameter
    Variable param;
    param.name = param_name;
    param.type = DataType::ANY;  // Infer later
    arrow_func->parameters.push_back(param);
    
    // Parse the arrow function body
    if (check(TokenType::LBRACE)) {
        // Block body: x => { return x + 1; }
        arrow_func->is_single_expression = false;
        match(TokenType::LBRACE);
        
        // Parse statements in the block body
        while (!check(TokenType::RBRACE) && !is_at_end()) {
            arrow_func->body.push_back(parse_statement());
        }
        
        if (!match(TokenType::RBRACE)) {
            throw std::runtime_error("Expected '}' after arrow function body");
        }
    } else {
        // Expression body: x => x + 1
        arrow_func->is_single_expression = true;
        arrow_func->expression = parse_assignment_expression();
    }
    
    return arrow_func;
}

std::unique_ptr<ArrowFunction> Parser::parse_arrow_function_from_params(const std::vector<Variable>& params) {
    // We're parsing: (x, y) => body
    if (!match(TokenType::ARROW)) {
        throw std::runtime_error("Expected '=>' in arrow function");
    }
    
    auto arrow_func = std::make_unique<ArrowFunction>();
    arrow_func->parameters = params;
    
    // Parse the arrow function body (same logic as single parameter version)
    if (check(TokenType::LBRACE)) {
        // Block body: (x, y) => { return x + y; }
        arrow_func->is_single_expression = false;
        match(TokenType::LBRACE);
        
        while (!check(TokenType::RBRACE) && !is_at_end()) {
            arrow_func->body.push_back(parse_statement());
        }
        
        if (!match(TokenType::RBRACE)) {
            throw std::runtime_error("Expected '}' after arrow function body");
        }
    } else {
        // Expression body: (x, y) => x + y
        arrow_func->is_single_expression = true;
        arrow_func->expression = parse_assignment_expression();
    }
    
    return arrow_func;
}
