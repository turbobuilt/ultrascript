#include "compiler.h"
#include "minimal_parser_gc.h" // Use minimal GC instead of parser_gc_integration.h
#include "simple_lexical_scope.h" // NEW simple lexical scope system
#include <algorithm>
#include <stdexcept>
#include <iostream>


void Parser::initialize_gc_integration() {
    gc_integration_ = std::make_unique<MinimalParserGCIntegration>();
    std::cout << "[Parser] Initialized minimal GC integration" << std::endl;
}

void Parser::finalize_gc_analysis() {
    if (gc_integration_) {
        gc_integration_->finalize_analysis();
    }
}

void Parser::initialize_simple_lexical_scope_system() {
    lexical_scope_analyzer_ = std::make_unique<SimpleLexicalScopeAnalyzer>();
    std::cout << "[Parser] Initialized simple lexical scope system" << std::endl;
}

void Parser::finalize_simple_lexical_scope_analysis() {
    if (lexical_scope_analyzer_) {
        lexical_scope_analyzer_->print_debug_info();
    }
    std::cout << "[Parser] Finalized simple lexical scope analysis" << std::endl;
}

void Parser::add_variable_to_current_scope(const std::string& name, const std::string& type) {
    current_scope_variables_[name] = type;
    std::cout << "[Parser] Added variable to current scope: " << name << " : " << type << std::endl;
    
    // Note: lexical_scope_analyzer_->declare_variable is called directly in parse_variable_declaration
    // with proper DataType information, so we don't call it here to avoid overwriting with DataType::ANY
}

void Parser::set_current_scope_variables(const std::unordered_map<std::string, std::string>& variables) {
    current_scope_variables_ = variables;
    std::cout << "[Parser] Set current scope with " << variables.size() << " variables" << std::endl;
}

// Scope management for function bodies
void Parser::enter_function_scope() {
    // Clear the current scope variables - function starts with empty local scope
    current_scope_variables_.clear();
    std::cout << "[Parser] Entered new function scope (cleared local variables)" << std::endl;
}

void Parser::exit_function_scope(const std::unordered_map<std::string, std::string>& parent_scope) {
    // Restore the parent scope variables
    current_scope_variables_ = parent_scope;
    std::cout << "[Parser] Exited function scope (restored " << parent_scope.size() << " parent variables)" << std::endl;
}

Parser::~Parser() {
    // Default destructor - the unique_ptr will handle cleanup properly now
    // since ParserGCIntegration is a complete type in this translation unit
}

Token& Parser::current_token() {
    if (pos >= tokens.size()) {
        static Token eof_token = {TokenType::EOF_TOKEN, "", 0, 0};
        return eof_token;
    }
    return tokens[pos];
}

Token& Parser::peek_token(int offset) {
    size_t peek_pos = pos + offset;
    if (peek_pos >= tokens.size()) {
        static Token eof_token = {TokenType::EOF_TOKEN, "", 0, 0};
        return eof_token;
    }
    return tokens[peek_pos];
}

void Parser::advance() {
    if (pos < tokens.size()) {
        pos++;
    }
}

bool Parser::match(TokenType type) {
    if (check(type)) {
        advance();
        return true;
    }
    return false;
}

bool Parser::check(TokenType type) {
    return current_token().type == type;
}

bool Parser::is_at_end() {
    return pos >= tokens.size() || current_token().type == TokenType::EOF_TOKEN;
}

std::unique_ptr<ExpressionNode> Parser::parse_expression() {
    return parse_assignment_expression();
}

std::unique_ptr<ExpressionNode> Parser::parse_assignment_expression() {
    auto expr = parse_ternary();
    
    // Check for arrow function: identifier => body or (params) => body
    if (check(TokenType::ARROW)) {
        // Check if expr is suitable for arrow function parameters
        if (auto identifier = dynamic_cast<Identifier*>(expr.get())) {
            // Single parameter arrow function: x => body
            std::string param_name = identifier->name;
            expr.release(); // Release the identifier since we'll use it as parameter
            return parse_arrow_function_from_identifier(param_name);
        } else {
            // For now, we don't handle (x, y) => body syntax
            // This would require detecting parenthesized parameter lists
            // TODO: Implement multi-parameter arrow functions
        }
    }
    
    if (match(TokenType::ASSIGN) || match(TokenType::PLUS_ASSIGN) ||
        match(TokenType::MINUS_ASSIGN) || match(TokenType::MULTIPLY_ASSIGN) ||
        match(TokenType::DIVIDE_ASSIGN)) {
        
        auto identifier = dynamic_cast<Identifier*>(expr.get());
        auto property_access = dynamic_cast<PropertyAccess*>(expr.get());
        auto expression_property_access = dynamic_cast<ExpressionPropertyAccess*>(expr.get());
        
        if (identifier) {
            std::string var_name = identifier->name;
            auto value = parse_assignment_expression();
            
            // GC Integration: Track assignment for escape analysis
            if (gc_integration_) {
                gc_integration_->assign_variable(var_name);
            }
            
            expr.release();
            auto assignment = std::make_unique<Assignment>(var_name, std::move(value));
            
            // Set lexical scope depth information and weak scope pointers
            if (lexical_scope_analyzer_) {
                assignment->definition_depth = lexical_scope_analyzer_->get_variable_definition_depth(var_name);
                assignment->assignment_depth = lexical_scope_analyzer_->get_current_depth();
                
                // NEW: Set raw pointer scope pointers for safe access
                assignment->definition_scope = lexical_scope_analyzer_->get_definition_scope_for_variable(var_name);
                assignment->assignment_scope = lexical_scope_analyzer_->get_current_scope_node();
                
                std::cout << "[Parser] Assignment '" << var_name 
                          << "' def_scope=" << assignment->definition_scope
                          << ", assign_scope=" << assignment->assignment_scope << std::endl;
            }
            
            return assignment;
        } else if (property_access) {
            std::string obj_name = property_access->object_name;
            std::string prop_name = property_access->property_name;
            auto value = parse_assignment_expression();
            
            // GC Integration: Track property assignment for escape analysis
            if (gc_integration_) {
                gc_integration_->mark_property_assignment(obj_name, prop_name);
            }
            
            expr.release();
            auto prop_assignment = std::make_unique<PropertyAssignment>(obj_name, prop_name, std::move(value));
            return prop_assignment;
        } else if (expression_property_access) {
            auto object_expr = std::move(expression_property_access->object);
            std::string prop_name = expression_property_access->property_name;
            auto value = parse_assignment_expression();
            
            expr.release();
            auto expr_prop_assignment = std::make_unique<ExpressionPropertyAssignment>(
                std::move(object_expr), prop_name, std::move(value));
            return expr_prop_assignment;
        } else {
            if (error_reporter) {
                error_reporter->report_parse_error("Invalid assignment target", current_token());
            }
            throw std::runtime_error("Invalid assignment target");
        }
    }
    
    return expr;
}

std::unique_ptr<ExpressionNode> Parser::parse_ternary() {
    auto expr = parse_logical_or();
    
    if (match(TokenType::QUESTION)) {
        auto true_expr = parse_expression();
        
        if (!match(TokenType::COLON)) {
            if (error_reporter) {
                error_reporter->report_parse_error("Expected ':' in ternary operator", current_token());
            }
            throw std::runtime_error("Expected ':' in ternary operator");
        }
        
        auto false_expr = parse_ternary(); // Right associative
        
        return std::make_unique<TernaryOperator>(std::move(expr), std::move(true_expr), std::move(false_expr));
    }
    
    return expr;
}

std::unique_ptr<ExpressionNode> Parser::parse_logical_or() {
    auto expr = parse_logical_and();
    
    while (match(TokenType::OR)) {
        TokenType op = tokens[pos - 1].type;
        auto right = parse_logical_and();
        expr = std::make_unique<BinaryOp>(std::move(expr), op, std::move(right));
    }
    
    return expr;
}

std::unique_ptr<ExpressionNode> Parser::parse_logical_and() {
    auto expr = parse_equality();
    
    while (match(TokenType::AND)) {
        TokenType op = tokens[pos - 1].type;
        auto right = parse_equality();
        expr = std::make_unique<BinaryOp>(std::move(expr), op, std::move(right));
    }
    
    return expr;
}

std::unique_ptr<ExpressionNode> Parser::parse_equality() {
    auto expr = parse_comparison();
    
    while (match(TokenType::EQUAL) || match(TokenType::NOT_EQUAL) || match(TokenType::STRICT_EQUAL)) {
        TokenType op = tokens[pos - 1].type;
        auto right = parse_comparison();
        expr = std::make_unique<BinaryOp>(std::move(expr), op, std::move(right));
    }
    
    return expr;
}

std::unique_ptr<ExpressionNode> Parser::parse_comparison() {
    auto expr = parse_addition();
    
    while (match(TokenType::GREATER) || match(TokenType::GREATER_EQUAL) ||
           match(TokenType::LESS) || match(TokenType::LESS_EQUAL)) {
        TokenType op = tokens[pos - 1].type;
        auto right = parse_addition();
        expr = std::make_unique<BinaryOp>(std::move(expr), op, std::move(right));
    }
    
    return expr;
}

std::unique_ptr<ExpressionNode> Parser::parse_addition() {
    auto expr = parse_multiplication();
    
    while (match(TokenType::PLUS) || match(TokenType::MINUS)) {
        TokenType op = tokens[pos - 1].type;
        auto right = parse_multiplication();
        expr = std::make_unique<BinaryOp>(std::move(expr), op, std::move(right));
    }
    
    return expr;
}

std::unique_ptr<ExpressionNode> Parser::parse_multiplication() {
    auto expr = parse_exponentiation();
    
    while (match(TokenType::MULTIPLY) || match(TokenType::DIVIDE) || match(TokenType::MODULO)) {
        TokenType op = tokens[pos - 1].type;
        auto right = parse_exponentiation();
        expr = std::make_unique<BinaryOp>(std::move(expr), op, std::move(right));
    }
    
    return expr;
}

std::unique_ptr<ExpressionNode> Parser::parse_exponentiation() {
    auto expr = parse_unary();
    
    // Exponentiation is right-associative, so use recursion instead of loop
    if (match(TokenType::POWER)) {
        TokenType op = tokens[pos - 1].type;
        auto right = parse_exponentiation(); // Right-associative
        expr = std::make_unique<BinaryOp>(std::move(expr), op, std::move(right));
    }
    
    return expr;
}

std::unique_ptr<ExpressionNode> Parser::parse_unary() {
    if (match(TokenType::NOT) || match(TokenType::MINUS)) {
        TokenType op = tokens[pos - 1].type;
        auto right = parse_unary();
        return std::make_unique<BinaryOp>(nullptr, op, std::move(right));
    }
    
    if (match(TokenType::GO)) {
        // Parse go functionCall() or go function(){}
        auto expr = parse_call();
        
        // The expression should be a function call or function expression - mark it as a goroutine
        if (auto func_call = dynamic_cast<FunctionCall*>(expr.get())) {
            func_call->is_goroutine = true;
            return expr;
        } else if (auto method_call = dynamic_cast<MethodCall*>(expr.get())) {
            method_call->is_goroutine = true;
            return expr;
        } else if (auto func_expr = dynamic_cast<FunctionExpression*>(expr.get())) {
            func_expr->is_goroutine = true;
            std::cout << "[Parser] Marked FunctionExpression as goroutine" << std::endl;
            
            // Escape analysis is already handled in function expression parsing
            // No need to duplicate it here
            
            return expr;
        } else {
            throw std::runtime_error("'go' can only be used with function calls or function expressions");
        }
    }
    
    return parse_call();
}

std::unique_ptr<ExpressionNode> Parser::parse_call() {
    auto expr = parse_primary();
    
    while (true) {
        if (match(TokenType::LPAREN)) {
            auto identifier = dynamic_cast<Identifier*>(expr.get());
            if (!identifier) {
                throw std::runtime_error("Invalid function call");
            }
            
            std::string func_name = identifier->name;
            expr.release();
            
            auto call = std::make_unique<FunctionCall>(func_name);
            
            if (!check(TokenType::RPAREN)) {
                do {
                    // Check if this is a keyword argument (name=value)
                    if (check(TokenType::IDENTIFIER)) {
                        // Save current position to potentially backtrack
                        size_t saved_pos = pos;
                        Token id_token = current_token();
                        advance();
                        
                        if (check(TokenType::ASSIGN)) {
                            // This is a keyword argument
                            advance(); // consume '='
                            call->keyword_names.push_back(id_token.value);
                            call->arguments.push_back(parse_expression());
                        } else {
                            // Not a keyword argument, backtrack and parse as normal expression
                            pos = saved_pos;
                            call->keyword_names.push_back(""); // Empty string for positional arg
                            call->arguments.push_back(parse_expression());
                        }
                    } else {
                        // Regular positional argument
                        call->keyword_names.push_back(""); // Empty string for positional arg
                        call->arguments.push_back(parse_expression());
                    }
                } while (match(TokenType::COMMA));
            }
            
            if (!match(TokenType::RPAREN)) {
                if (error_reporter) {
                    error_reporter->report_parse_error("Expected ')' after function arguments", current_token());
                }
                throw std::runtime_error("Expected ')' after function arguments");
            }
            
            // GC Integration: Track function call for escape analysis
            if (gc_integration_) {
                std::vector<std::string> arg_names;
                // In a real implementation, we'd extract variable names from the expressions
                // For now, just track that a function call happened
                gc_integration_->mark_function_call(func_name, arg_names);
            }
            
            expr = std::move(call);
        } else if (match(TokenType::DOT)) {
            if (!match(TokenType::IDENTIFIER)) {
                if (error_reporter) {
                    error_reporter->report_parse_error("Expected property name after '.'", current_token());
                }
                throw std::runtime_error("Expected property name after '.'");
            }
            
            std::string property = tokens[pos - 1].value;
            
            // Check if this is a method call (has parentheses after the property)
            if (check(TokenType::LPAREN)) {
                auto identifier = dynamic_cast<Identifier*>(expr.get());
                auto this_expr = dynamic_cast<ThisExpression*>(expr.get());
                auto super_call = dynamic_cast<SuperCall*>(expr.get());
                
                if (super_call) {
                    // This is super.methodName() - create SuperMethodCall
                    expr.release();
                    
                    advance(); // consume LPAREN
                    auto super_method_call = std::make_unique<SuperMethodCall>(property);
                    
                    if (!check(TokenType::RPAREN)) {
                        do {
                            // Check if this is a keyword argument (name=value)
                            if (check(TokenType::IDENTIFIER)) {
                                // Save current position to potentially backtrack
                                size_t saved_pos = pos;
                                Token id_token = current_token();
                                advance();
                                
                                if (check(TokenType::ASSIGN)) {
                                    // This is a keyword argument
                                    advance(); // consume '='
                                    super_method_call->keyword_names.push_back(id_token.value);
                                    super_method_call->arguments.push_back(parse_expression());
                                } else {
                                    // Not a keyword argument, backtrack and parse as normal expression
                                    pos = saved_pos;
                                    super_method_call->keyword_names.push_back(""); // Empty string for positional arg
                                    super_method_call->arguments.push_back(parse_expression());
                                }
                            } else {
                                // Regular positional argument
                                super_method_call->keyword_names.push_back(""); // Empty string for positional arg
                                super_method_call->arguments.push_back(parse_expression());
                            }
                        } while (match(TokenType::COMMA));
                    }
                    
                    if (!match(TokenType::RPAREN)) {
                        throw std::runtime_error("Expected ')' after super method arguments");
                    }
                    
                    expr = std::move(super_method_call);
                    continue; // Continue to check for more chained calls
                }
                
                // Handle method calls on identifiers and 'this' using the original MethodCall
                if (identifier || this_expr) {
                    std::string object_name;
                    if (identifier) {
                        object_name = identifier->name;
                    } else {
                        object_name = "this";
                    }
                    
                    expr.release();
                    
                    // Parse the method call like a function call
                    advance(); // consume LPAREN
                    auto method_call = std::make_unique<MethodCall>(object_name, property);
                    
                    if (!check(TokenType::RPAREN)) {
                        do {
                            // Check if this is a keyword argument (name=value)
                            if (check(TokenType::IDENTIFIER)) {
                                // Save current position to potentially backtrack
                                size_t saved_pos = pos;
                                Token id_token = current_token();
                                advance();
                                
                                if (check(TokenType::ASSIGN)) {
                                    // This is a keyword argument
                                    advance(); // consume '='
                                    method_call->keyword_names.push_back(id_token.value);
                                    method_call->arguments.push_back(parse_expression());
                                } else {
                                    // Not a keyword argument, backtrack and parse as normal expression
                                    pos = saved_pos;
                                    method_call->keyword_names.push_back(""); // Empty string for positional arg
                                    method_call->arguments.push_back(parse_expression());
                                }
                            } else {
                                // Regular positional argument
                                method_call->keyword_names.push_back(""); // Empty string for positional arg
                                method_call->arguments.push_back(parse_expression());
                            }
                        } while (match(TokenType::COMMA));
                    }
                    
                    if (!match(TokenType::RPAREN)) {
                        throw std::runtime_error("Expected ')' after method arguments");
                    }
                    
                    expr = std::move(method_call);
                } else {
                    // Handle method calls on any expression using ExpressionMethodCall
                    auto object_expr = std::move(expr);
                    
                    // Parse the method call like a function call
                    advance(); // consume LPAREN
                    auto expr_method_call = std::make_unique<ExpressionMethodCall>(std::move(object_expr), property);
                    
                    if (!check(TokenType::RPAREN)) {
                        do {
                            // Check if this is a keyword argument (name=value)
                            if (check(TokenType::IDENTIFIER)) {
                                // Save current position to potentially backtrack
                                size_t saved_pos = pos;
                                Token id_token = current_token();
                                advance();
                                
                                if (check(TokenType::ASSIGN)) {
                                    // This is a keyword argument
                                    advance(); // consume '='
                                    expr_method_call->keyword_names.push_back(id_token.value);
                                    expr_method_call->arguments.push_back(parse_expression());
                                } else {
                                    // Not a keyword argument, backtrack and parse as normal expression
                                    pos = saved_pos;
                                    expr_method_call->keyword_names.push_back(""); // Empty string for positional arg
                                    expr_method_call->arguments.push_back(parse_expression());
                                }
                            } else {
                                // Regular positional argument
                                expr_method_call->keyword_names.push_back(""); // Empty string for positional arg
                                expr_method_call->arguments.push_back(parse_expression());
                            }
                        } while (match(TokenType::COMMA));
                    }
                    
                    if (!match(TokenType::RPAREN)) {
                        throw std::runtime_error("Expected ')' after method arguments");
                    }
                    
                    expr = std::move(expr_method_call);
                }
            } else {
                // This is property access, not a method call
                auto identifier = dynamic_cast<Identifier*>(expr.get());
                auto this_expr = dynamic_cast<ThisExpression*>(expr.get());
                auto super_call = dynamic_cast<SuperCall*>(expr.get());
                
                if (this_expr) {
                    expr.release();
                    expr = std::make_unique<PropertyAccess>("this", property);
                } else if (super_call) {
                    expr.release();
                    expr = std::make_unique<PropertyAccess>("super", property);
                } else {
                    // Handle property access on any expression using ExpressionPropertyAccess
                    // This includes regular identifiers like "result.length"
                    auto object_expr = std::move(expr);
                    expr = std::make_unique<ExpressionPropertyAccess>(std::move(object_expr), property);
                }
            }
        } else if (match(TokenType::INCREMENT)) {
            // Handle postfix increment
            auto identifier = dynamic_cast<Identifier*>(expr.get());
            if (!identifier) {
                throw std::runtime_error("Invalid increment operation");
            }
            
            std::string var_name = identifier->name;
            expr.release();
            expr = std::make_unique<PostfixIncrement>(var_name);
        } else if (match(TokenType::DECREMENT)) {
            // Handle postfix decrement
            auto identifier = dynamic_cast<Identifier*>(expr.get());
            if (!identifier) {
                throw std::runtime_error("Invalid decrement operation");
            }
            
            std::string var_name = identifier->name;
            expr.release();
            expr = std::make_unique<PostfixDecrement>(var_name);
        } else if (match(TokenType::LBRACKET)) {
            // Handle array access or operator[] overload
            auto object_expr = std::move(expr);
            
            // Check if this looks like a slice expression using lookahead
            bool is_slice = false;
            size_t lookahead_pos = pos;
            int bracket_depth = 1;
            
            // Look ahead to detect slice patterns (contains colons)
            while (lookahead_pos < tokens.size() && bracket_depth > 0) {
                const auto& token = tokens[lookahead_pos];
                
                if (token.type == TokenType::LBRACKET) {
                    bracket_depth++;
                } else if (token.type == TokenType::RBRACKET) {
                    bracket_depth--;
                } else if (token.type == TokenType::COLON && bracket_depth == 1) {
                    is_slice = true;
                    break;
                }
                lookahead_pos++;
            }
            
            if (is_slice) {
                // Collect tokens as slice string literal  
                std::string raw_content;
                
                while (pos < tokens.size() && !check(TokenType::RBRACKET)) {
                    if (!raw_content.empty()) {
                        raw_content += " ";
                    }
                    raw_content += current_token().value;
                    pos++;
                }
                
                if (!match(TokenType::RBRACKET)) {
                    throw std::runtime_error("Expected ']' after slice expression");
                }
                
                auto index_expr = std::make_unique<StringLiteral>(raw_content);
                auto array_access = std::make_unique<ArrayAccess>(std::move(object_expr), std::move(index_expr));
                array_access->is_slice_expression = true;
                array_access->slice_expression = raw_content;
                expr = std::move(array_access);
            } else {
                // Parse as normal expression
                auto index_expr = parse_expression();
                
                if (!match(TokenType::RBRACKET)) {
                    throw std::runtime_error("Expected ']' after array index");
                }
                
                auto array_access = std::make_unique<ArrayAccess>(std::move(object_expr), std::move(index_expr));
                expr = std::move(array_access);
            }
        } else if (match(TokenType::SLICE_BRACKET)) {
            // Handle [:]  slice syntax as a special case
            auto object_expr = std::move(expr);
            auto slice_literal = std::make_unique<StringLiteral>(":");
            auto array_access = std::make_unique<ArrayAccess>(std::move(object_expr), std::move(slice_literal));
            array_access->is_slice_expression = true;
            array_access->slice_expression = ":";
            expr = std::move(array_access);
        } else {
            break;
        }
    }
    
    return expr;
}

std::unique_ptr<ExpressionNode> Parser::parse_primary() {
    if (match(TokenType::NUMBER)) {
        double value = std::stod(tokens[pos - 1].value);
        return std::make_unique<NumberLiteral>(value);
    }
    
    if (match(TokenType::STRING)) {
        return std::make_unique<StringLiteral>(tokens[pos - 1].value);
    }
    
    if (match(TokenType::TEMPLATE_LITERAL)) {
        return std::make_unique<StringLiteral>(tokens[pos - 1].value);
    }
    
    if (match(TokenType::REGEX)) {
        std::string regex_value = tokens[pos - 1].value;
        
        // Parse pattern and flags (separated by |)
        std::string pattern;
        std::string flags;
        
        size_t separator_pos = regex_value.find('|');
        if (separator_pos != std::string::npos) {
            pattern = regex_value.substr(0, separator_pos);
            flags = regex_value.substr(separator_pos + 1);
        } else {
            pattern = regex_value;
        }
        
        return std::make_unique<RegexLiteral>(pattern, flags);
    }
    
    if (match(TokenType::BOOLEAN)) {
        double value = (tokens[pos - 1].value == "true") ? 1.0 : 0.0;
        return std::make_unique<NumberLiteral>(value);
    }
    
    if (match(TokenType::IDENTIFIER)) {
        std::string var_name = tokens[pos - 1].value;
        
        // NEW: Track variable access in lexical scope and get depth information
        int definition_depth = -1;
        int access_depth = -1;
        LexicalScopeNode* definition_scope = nullptr;
        LexicalScopeNode* access_scope = nullptr;
        
        if (lexical_scope_analyzer_) {
            lexical_scope_analyzer_->access_variable(var_name);
            definition_depth = lexical_scope_analyzer_->get_variable_definition_depth(var_name);
            access_depth = lexical_scope_analyzer_->get_current_depth();
            
            // NEW: Get raw pointers to the actual scope nodes
            definition_scope = lexical_scope_analyzer_->get_definition_scope_for_variable(var_name);
            access_scope = lexical_scope_analyzer_->get_current_scope_node();
            
            std::cout << "[Parser] Creating Identifier '" << var_name 
                      << "' with def_scope=" << definition_scope
                      << ", access_scope=" << access_scope << std::endl;
        }
        
        // Get direct pointer to variable declaration info for ultra-fast access
        VariableDeclarationInfo* var_info = nullptr;
        if (lexical_scope_analyzer_) {
            var_info = lexical_scope_analyzer_->get_variable_declaration_info(var_name);
        }
        
        // Use ultra-fast constructor with direct variable declaration pointer
        return std::make_unique<Identifier>(var_name, var_info, definition_scope, access_scope);
    }
    
    if (match(TokenType::LBRACKET)) {
        auto array_literal = std::make_unique<ArrayLiteral>();
        
        if (!check(TokenType::RBRACKET)) {
            do {
                // Check if this element looks like a slice expression using lookahead
                bool is_slice = false;
                size_t lookahead_pos = pos;
                
                // Look ahead to see if we encounter a colon before comma/bracket
                while (lookahead_pos < tokens.size()) {
                    const auto& token = tokens[lookahead_pos];
                    
                    if (token.type == TokenType::COMMA || token.type == TokenType::RBRACKET) {
                        break; // End of element
                    } else if (token.type == TokenType::COLON) {
                        is_slice = true;
                        break; // Found slice pattern
                    }
                    lookahead_pos++;
                }
                
                if (is_slice) {
                    // Collect tokens as slice string literal
                    std::string raw_content;
                    
                    while (pos < tokens.size() && 
                           !check(TokenType::COMMA) && 
                           !check(TokenType::RBRACKET)) {
                        if (!raw_content.empty()) {
                            raw_content += " ";
                        }
                        raw_content += current_token().value;
                        pos++;
                    }
                    
                    auto element_expr = std::make_unique<StringLiteral>(raw_content);
                    array_literal->elements.push_back(std::move(element_expr));
                } else {
                    // Parse as normal expression
                    auto element_expr = parse_expression();
                    array_literal->elements.push_back(std::move(element_expr));
                }
            } while (match(TokenType::COMMA));
        }
        
        if (!match(TokenType::RBRACKET)) {
            throw std::runtime_error("Expected ']' after array elements");
        }
        
        return array_literal;
    }
    
    if (match(TokenType::LBRACE)) {
        auto object_literal = std::make_unique<ObjectLiteral>();
        
        if (!check(TokenType::RBRACE)) {
            do {
                // Parse property: key : value
                if (!match(TokenType::IDENTIFIER) && !match(TokenType::STRING)) {
                    throw std::runtime_error("Expected property name");
                }
                std::string key = tokens[pos - 1].value;
                
                if (!match(TokenType::COLON)) {
                    throw std::runtime_error("Expected ':' after property name");
                }
                
                auto value = parse_expression();
                object_literal->properties.emplace_back(key, std::move(value));
                
            } while (match(TokenType::COMMA));
        }
        
        if (!match(TokenType::RBRACE)) {
            throw std::runtime_error("Expected '}' after object properties");
        }
        
        return object_literal;
    }
    
    if (match(TokenType::LPAREN)) {
        // Check if this might be arrow function parameters: (param1, param2) => body
        size_t saved_pos = pos;
        std::vector<Variable> potential_params;
        bool is_arrow_params = true;
        
        // Try to parse as parameter list
        if (!check(TokenType::RPAREN)) {
            do {
                if (check(TokenType::IDENTIFIER)) {
                    Variable param;
                    param.name = current_token().value;
                    param.type = DataType::ANY;
                    potential_params.push_back(param);
                    advance();
                    
                    // Optional type annotation (for future)
                    if (match(TokenType::COLON)) {
                        // Skip type for now
                        if (check(TokenType::IDENTIFIER)) {
                            advance();
                        }
                    }
                } else {
                    is_arrow_params = false;
                    break;
                }
            } while (match(TokenType::COMMA));
        }
        
        // Check if followed by ) and then =>
        if (is_arrow_params && match(TokenType::RPAREN) && check(TokenType::ARROW)) {
            // This is arrow function parameters: (param1, param2) => body
            return parse_arrow_function_from_params(potential_params);
        } else {
            // Reset position and parse as regular parenthesized expression
            pos = saved_pos;
            auto expr = parse_expression();
            if (!match(TokenType::RPAREN)) {
                throw std::runtime_error("Expected ')' after expression");
            }
            return expr;
        }
    }
    
    if (match(TokenType::GO)) {
        auto expr = parse_call();
        auto func_call = dynamic_cast<FunctionCall*>(expr.get());
        if (func_call) {
            func_call->is_goroutine = true;
        }
        return expr;
    }
    
    if (match(TokenType::AWAIT)) {
        auto expr = parse_call();
        if (auto func_call = dynamic_cast<FunctionCall*>(expr.get())) {
            func_call->is_awaited = true;
        } else if (auto method_call = dynamic_cast<MethodCall*>(expr.get())) {
            method_call->is_awaited = true;
        } else if (auto func_expr = dynamic_cast<FunctionExpression*>(expr.get())) {
            // Handle await go function() {...}
            func_expr->is_awaited = true;
        }
        return expr;
    }
    
    if (match(TokenType::THIS)) {
        return std::make_unique<ThisExpression>();
    }
    
    if (match(TokenType::SUPER)) {
        auto super_call = std::make_unique<SuperCall>();
        
        if (match(TokenType::LPAREN)) {
            // Parse super constructor arguments
            if (!check(TokenType::RPAREN)) {
                do {
                    super_call->arguments.push_back(parse_expression());
                } while (match(TokenType::COMMA));
            }
            
            if (!match(TokenType::RPAREN)) {
                throw std::runtime_error("Expected ')' after super arguments");
            }
        }
        
        return super_call;
    }
    
    if (match(TokenType::FUNCTION)) {
        // Parse function expression: function(params) { body }
        auto func_expr = std::make_unique<FunctionExpression>();
        
        // Check for optional function name (for recursion/debugging)
        if (check(TokenType::IDENTIFIER)) {
            func_expr->name = current_token().value;
            advance();
        }
        
        // Parse parameters
        if (!match(TokenType::LPAREN)) {
            if (error_reporter) {
                error_reporter->report_parse_error("Expected '(' after 'function'", current_token());
            }
            throw std::runtime_error("Expected '(' after 'function'");
        }
        
        if (!check(TokenType::RPAREN)) {
            do {
                if (!check(TokenType::IDENTIFIER)) {
                    if (error_reporter) {
                        error_reporter->report_parse_error("Expected parameter name", current_token());
                    }
                    throw std::runtime_error("Expected parameter name");
                }
                
                Variable param;
                param.name = current_token().value;
                param.type = DataType::ANY;  // Type inference will handle this
                advance();
                
                // Check for type annotation: param: type
                if (match(TokenType::COLON)) {
                    param.type = parse_type();
                }
                
                func_expr->parameters.push_back(param);
            } while (match(TokenType::COMMA));
        }
        
        if (!match(TokenType::RPAREN)) {
            if (error_reporter) {
                error_reporter->report_parse_error("Expected ')' after parameters", current_token());
            }
            throw std::runtime_error("Expected ')' after parameters");
        }
        
        // Parse return type annotation: function(): ReturnType
        if (match(TokenType::COLON)) {
            func_expr->return_type = parse_type();
        }
        
        // Parse function body
        if (!match(TokenType::LBRACE)) {
            if (error_reporter) {
                error_reporter->report_parse_error("Expected '{' to start function body", current_token());
            }
            throw std::runtime_error("Expected '{' to start function body");
        }
        
        // GC INTEGRATION: Enter function scope for proper variable scoping
        if (gc_integration_) {
            gc_integration_->enter_scope("function_expr", true);
        }
        
        // NEW: Enter lexical scope for function expression
        if (lexical_scope_analyzer_) {
            lexical_scope_analyzer_->enter_scope();
            
            // Declare parameters in the new scope
            for (const auto& param : func_expr->parameters) {
                lexical_scope_analyzer_->declare_variable(param.name, "let", param.type);
            }
        }
        
        // LEXICAL SCOPE MANAGEMENT: Save parent scope before entering function
        std::unordered_map<std::string, std::string> parent_scope = current_scope_variables_;
        enter_function_scope(); // Clear local scope for function body
        
        while (!check(TokenType::RBRACE) && !check(TokenType::EOF_TOKEN)) {
            func_expr->body.push_back(parse_statement());
        }
        
        if (!match(TokenType::RBRACE)) {
            if (error_reporter) {
                error_reporter->report_parse_error("Expected '}' to end function body", current_token());
            }
            throw std::runtime_error("Expected '}' to end function body");
        }
        
        // NEW: Exit lexical scope for function expression and capture scope info
        if (lexical_scope_analyzer_) {
            func_expr->lexical_scope = lexical_scope_analyzer_->exit_scope();
        }
        
        // GC INTEGRATION: Exit function scope
        if (gc_integration_) {
            gc_integration_->exit_scope();
        }
        
        // LEXICAL SCOPE MANAGEMENT: Restore parent scope and perform escape analysis
        exit_function_scope(parent_scope);
        
        return func_expr;
    }
    
    if (match(TokenType::NEW)) {
        if (!check(TokenType::IDENTIFIER)) {
            throw std::runtime_error("Expected class name after 'new'");
        }
        
        std::string class_name = current_token().value;
        advance();
        
        auto new_expr = std::make_unique<NewExpression>(class_name);
        
        if (match(TokenType::LBRACE)) {
            // Dart-style: new Person{name: "bob", age: 25}
            new_expr->is_dart_style = true;
            
            while (!check(TokenType::RBRACE) && !check(TokenType::EOF_TOKEN)) {
                if (!check(TokenType::IDENTIFIER)) {
                    throw std::runtime_error("Expected property name");
                }
                
                std::string prop_name = current_token().value;
                advance();
                
                if (!match(TokenType::COLON)) {
                    throw std::runtime_error("Expected ':' after property name");
                }
                
                auto value = parse_expression();
                new_expr->dart_args.push_back(std::make_pair(prop_name, std::move(value)));
                
                if (!match(TokenType::COMMA)) {
                    break;
                }
            }
            
            if (!match(TokenType::RBRACE)) {
                throw std::runtime_error("Expected '}' after object properties");
            }
        } else if (match(TokenType::LPAREN)) {
            // Regular style: new Person("bob", 25)
            while (!check(TokenType::RPAREN) && !check(TokenType::EOF_TOKEN)) {
                new_expr->arguments.push_back(parse_expression());
                if (!match(TokenType::COMMA)) {
                    break;
                }
            }
            
            if (!match(TokenType::RPAREN)) {
                throw std::runtime_error("Expected ')' after constructor arguments");
            }
        }
        
        return new_expr;
    }
    
    if (error_reporter) {
        error_reporter->report_parse_error("Unexpected token", current_token());
    }
    throw std::runtime_error("Unexpected token: " + current_token().value);
}

std::unique_ptr<ASTNode> Parser::parse_statement() {
    if (check(TokenType::IMPORT)) {
        return parse_import_statement();
    }
    
    if (check(TokenType::EXPORT)) {
        return parse_export_statement();
    }
    
    if (check(TokenType::FUNCTION)) {
        return parse_function_declaration();
    }
    
    if (check(TokenType::CLASS)) {
        return parse_class_declaration();
    }
    
    if (check(TokenType::VAR) || check(TokenType::LET) || check(TokenType::CONST)) {
        return parse_variable_declaration();
    }
    
    if (check(TokenType::IF)) {
        return parse_if_statement();
    }
    
    if (check(TokenType::FOR)) {
        // Check if this is "for each" syntax
        if (peek_token(1).type == TokenType::EACH) {
            return parse_for_each_statement();
        } else {
            // Look ahead to see if this is a for-in loop
            // Pattern: for (let/var/const? identifier in expression) or for let/var/const? identifier in expression
            int lookahead = 1;
            
            // Check for for-in with parentheses: for (let key in obj)
            if (peek_token(lookahead).type == TokenType::LPAREN) {
                lookahead++;
                // Skip optional let/var/const
                if (peek_token(lookahead).type == TokenType::LET || 
                    peek_token(lookahead).type == TokenType::VAR ||
                    peek_token(lookahead).type == TokenType::CONST) {
                    lookahead++;
                }
                // Check for identifier followed by 'in'
                if (peek_token(lookahead).type == TokenType::IDENTIFIER &&
                    peek_token(lookahead + 1).type == TokenType::IN) {
                    return parse_for_in_statement();
                }
            }
            // Check for for-in without parentheses: for let key in obj
            else {
                // Skip optional let/var/const  
                if (peek_token(lookahead).type == TokenType::LET ||
                    peek_token(lookahead).type == TokenType::VAR ||
                    peek_token(lookahead).type == TokenType::CONST) {
                    lookahead++;
                }
                // Check for identifier followed by 'in'
                if (peek_token(lookahead).type == TokenType::IDENTIFIER &&
                    peek_token(lookahead + 1).type == TokenType::IN) {
                    return parse_for_in_statement();
                }
            }
            // Default: regular for-loop
            // Default: regular for-loop
            return parse_for_statement();
        }
    }
    
    if (check(TokenType::WHILE)) {
        return parse_while_statement();
    }
    
    if (check(TokenType::SWITCH)) {
        return parse_switch_statement();
    }
    
    if (check(TokenType::TRY)) {
        return parse_try_statement();
    }
    
    if (check(TokenType::THROW)) {
        return parse_throw_statement();
    }
    
    if (check(TokenType::LBRACE)) {
        return parse_block_statement();
    }
    
    if (check(TokenType::RETURN)) {
        return parse_return_statement();
    }
    
    if (check(TokenType::BREAK)) {
        return parse_break_statement();
    }
    
    if (check(TokenType::FREE)) {
        return parse_free_statement();
    }
    
    return parse_expression_statement();
}

std::unique_ptr<ASTNode> Parser::parse_function_declaration() {
    if (!match(TokenType::FUNCTION)) {
        throw std::runtime_error("Expected 'function'");
    }
    
    if (!match(TokenType::IDENTIFIER)) {
        throw std::runtime_error("Expected function name");
    }
    
    std::string func_name = tokens[pos - 1].value;
    auto func_decl = std::make_unique<FunctionDecl>(func_name);
    
    // GC Integration: Enter function scope
    if (gc_integration_) {
        gc_integration_->enter_scope(func_name, true);
    }
    
    // NEW: Enter lexical scope for function
    if (lexical_scope_analyzer_) {
        lexical_scope_analyzer_->enter_scope();
    }
    
    if (!match(TokenType::LPAREN)) {
        throw std::runtime_error("Expected '(' after function name");
    }
    
    if (!check(TokenType::RPAREN)) {
        do {
            if (!match(TokenType::IDENTIFIER)) {
                throw std::runtime_error("Expected parameter name");
            }
            
            std::string param_name = tokens[pos - 1].value;
            Variable param;
            param.name = param_name;
            param.type = DataType::ANY;
            
            if (match(TokenType::COLON)) {
                param.type = parse_type();
            }
            
            // GC Integration: Track parameter declaration
            if (gc_integration_) {
                gc_integration_->declare_variable(param_name, param.type);
            }
            
            // NEW: Track parameter in lexical scope
            std::string type_str = "param";
            if (lexical_scope_analyzer_) {
                lexical_scope_analyzer_->declare_variable(param_name, type_str);
            }
            
            func_decl->parameters.push_back(param);
        } while (match(TokenType::COMMA));
    }
    
    if (!match(TokenType::RPAREN)) {
        throw std::runtime_error("Expected ')' after parameters");
    }
    
    if (match(TokenType::COLON)) {
        func_decl->return_type = parse_type();
    }
    
    if (!match(TokenType::LBRACE)) {
        throw std::runtime_error("Expected '{' to start function body");
    }
    
    while (!check(TokenType::RBRACE) && !check(TokenType::EOF_TOKEN)) {
        func_decl->body.push_back(parse_statement());
    }
    
    if (!match(TokenType::RBRACE)) {
        throw std::runtime_error("Expected '}' to end function body");
    }
    
    // NEW: Exit lexical scope for function and capture scope info
    if (lexical_scope_analyzer_) {
        func_decl->lexical_scope = lexical_scope_analyzer_->exit_scope();
    }
    
    // GC Integration: Exit function scope
    if (gc_integration_) {
        gc_integration_->exit_scope();
    }
    
    return std::move(func_decl);
}

std::unique_ptr<ASTNode> Parser::parse_variable_declaration() {
    TokenType decl_type = current_token().type;
    advance();
    
    if (!match(TokenType::IDENTIFIER)) {
        throw std::runtime_error("Expected variable name");
    }
    
    std::string var_name = tokens[pos - 1].value;
    DataType type = DataType::ANY;
    
    if (match(TokenType::COLON)) {
        type = parse_type();
    }
    
    // GC Integration: Track variable declaration
    if (gc_integration_) {
        gc_integration_->declare_variable(var_name, type);
    }
    
    // NEW: Get declaration type string for lexical scope system
    std::string decl_type_str = "var";
    switch (decl_type) {
        case TokenType::LET: decl_type_str = "let"; break;
        case TokenType::CONST: decl_type_str = "const"; break;
        case TokenType::VAR: 
        default: decl_type_str = "var"; break;
    }
    
    // NEW: Declare variable in lexical scope analyzer
    if (lexical_scope_analyzer_) {
        lexical_scope_analyzer_->declare_variable(var_name, decl_type_str, type);
    }
    
    // Lexical Scope System: Add variable to current scope for escape analysis
    std::string type_str = "auto"; // Convert DataType to string
    if (type == DataType::INT32) type_str = "int";
    else if (type == DataType::FLOAT64) type_str = "float64";
    else if (type == DataType::STRING) type_str = "string";
    else if (type == DataType::BOOLEAN) type_str = "bool";
    add_variable_to_current_scope(var_name, decl_type_str); // Use declaration type instead of data type
    
    std::unique_ptr<ExpressionNode> value = nullptr;
    if (match(TokenType::ASSIGN)) {
        value = parse_expression();
        
        // GC Integration: Track assignment
        if (gc_integration_) {
            gc_integration_->assign_variable(var_name);
        }
    }
    
    auto assignment = std::make_unique<Assignment>(var_name, std::move(value));
    assignment->declared_type = type;
    assignment->declared_element_type = last_parsed_array_element_type;
    
    // Set lexical scope depth information and weak scope pointers
    if (lexical_scope_analyzer_) {
        assignment->definition_depth = lexical_scope_analyzer_->get_variable_definition_depth(var_name);
        assignment->assignment_depth = lexical_scope_analyzer_->get_current_depth();
        
        // NEW: Set raw pointer scope pointers for safe access
        assignment->definition_scope = lexical_scope_analyzer_->get_definition_scope_for_variable(var_name);
        assignment->assignment_scope = lexical_scope_analyzer_->get_current_scope_node();
        
        // NEW: Set direct pointer to variable declaration info for ultra-fast access
        assignment->variable_declaration_info = lexical_scope_analyzer_->get_variable_declaration_info(var_name);
        
        std::cout << "[Parser] Variable declaration '" << var_name 
                  << "' def_scope=" << assignment->definition_scope
                  << ", assign_scope=" << assignment->assignment_scope << std::endl;
    }
    
    // Set the declaration kind based on the parsed token type
    switch (decl_type) {
        case TokenType::VAR: 
            assignment->declaration_kind = Assignment::VAR; 
            break;
        case TokenType::LET: 
            assignment->declaration_kind = Assignment::LET; 
            break;
        case TokenType::CONST: 
            assignment->declaration_kind = Assignment::CONST; 
            break;
        default: 
            assignment->declaration_kind = Assignment::VAR; 
            break;
    }
    
    // Clear the element type after use
    last_parsed_array_element_type = DataType::ANY;
    
    if (match(TokenType::SEMICOLON)) {
        // Optional semicolon
    }
    
    return std::move(assignment);
}

std::unique_ptr<ASTNode> Parser::parse_if_statement() {
    if (!match(TokenType::IF)) {
        throw std::runtime_error("Expected 'if'");
    }
    
    auto if_stmt = std::make_unique<IfStatement>();
    
    bool has_parens = false;
    if (check(TokenType::LPAREN)) {
        has_parens = true;
        advance();
    }
    
    if_stmt->condition = parse_expression();
    
    if (has_parens && !match(TokenType::RPAREN)) {
        throw std::runtime_error("Expected ')' after if condition");
    }
    
    // Enter lexical scope for then branch
    if (lexical_scope_analyzer_) {
        lexical_scope_analyzer_->enter_scope();
    }
    
    if (match(TokenType::LBRACE)) {
        while (!check(TokenType::RBRACE) && !check(TokenType::EOF_TOKEN)) {
            if_stmt->then_body.push_back(parse_statement());
        }
        
        if (!match(TokenType::RBRACE)) {
            throw std::runtime_error("Expected '}' after if body");
        }
    } else {
        if_stmt->then_body.push_back(parse_statement());
    }
    
    // Exit lexical scope for then branch and capture scope info
    if (lexical_scope_analyzer_) {
        if_stmt->then_lexical_scope = lexical_scope_analyzer_->exit_scope();
    }
    
    // Handle else clause
    if (current_token().type == TokenType::IDENTIFIER && current_token().value == "else") {
        advance(); // consume "else"
        
        // Enter lexical scope for else branch
        if (lexical_scope_analyzer_) {
            lexical_scope_analyzer_->enter_scope();
        }
        
        if (match(TokenType::LBRACE)) {
            while (!check(TokenType::RBRACE) && !check(TokenType::EOF_TOKEN)) {
                if_stmt->else_body.push_back(parse_statement());
            }
            
            if (!match(TokenType::RBRACE)) {
                throw std::runtime_error("Expected '}' after else body");
            }
        } else {
            if_stmt->else_body.push_back(parse_statement());
        }
        
        // Exit lexical scope for else branch and capture scope info
        if (lexical_scope_analyzer_) {
            if_stmt->else_lexical_scope = lexical_scope_analyzer_->exit_scope();
        }
    }
    
    return std::move(if_stmt);
}

std::unique_ptr<ASTNode> Parser::parse_for_statement() {
    if (!match(TokenType::FOR)) {
        throw std::runtime_error("Expected 'for'");
    }
    
    auto for_loop = std::make_unique<ForLoop>();
    
    // Enter lexical scope for for loop (ES6 for loops create block scope for let/const)
    if (lexical_scope_analyzer_) {
        lexical_scope_analyzer_->enter_scope();
    }
    
    bool has_parens = false;
    if (check(TokenType::LPAREN)) {
        has_parens = true;
        advance();
    }
    
    // Parse init if present
    if (!check(TokenType::SEMICOLON)) {
        // For variable declarations in for loops, we need to parse them specially
        // to avoid consuming the semicolon
        if (check(TokenType::VAR) || check(TokenType::LET) || check(TokenType::CONST)) {
            TokenType decl_type = current_token().type;
            advance();
            
            if (!match(TokenType::IDENTIFIER)) {
                throw std::runtime_error("Expected variable name");
            }
            
            std::string var_name = tokens[pos - 1].value;
            DataType type = DataType::ANY;
            
            if (match(TokenType::COLON)) {
                type = parse_type();
            }
            
            std::unique_ptr<ExpressionNode> value = nullptr;
            if (match(TokenType::ASSIGN)) {
                value = parse_expression();
            }
            
            // Convert TokenType to Assignment::DeclarationKind
            Assignment::DeclarationKind assignment_kind = Assignment::VAR;
            switch (decl_type) {
                case TokenType::VAR: assignment_kind = Assignment::VAR; break;
                case TokenType::LET: assignment_kind = Assignment::LET; break;
                case TokenType::CONST: assignment_kind = Assignment::CONST; break;
                default: assignment_kind = Assignment::VAR; break;
            }
            
            // Declare variable in lexical scope analyzer
            if (lexical_scope_analyzer_) {
                std::string decl_type_str = (assignment_kind == Assignment::LET) ? "let" : 
                                           (assignment_kind == Assignment::CONST) ? "const" : "var";
                lexical_scope_analyzer_->declare_variable(var_name, decl_type_str);
            }
            
            auto assignment = std::make_unique<Assignment>(var_name, std::move(value), assignment_kind);
            assignment->declared_type = type;
            
            // Set lexical scope depth information and weak scope pointers
            if (lexical_scope_analyzer_) {
                assignment->definition_depth = lexical_scope_analyzer_->get_variable_definition_depth(var_name);
                assignment->assignment_depth = lexical_scope_analyzer_->get_current_depth();
                
                // NEW: Set weak_ptr scope pointers for safe access
                assignment->definition_scope = lexical_scope_analyzer_->get_definition_scope_for_variable(var_name);
                assignment->assignment_scope = lexical_scope_analyzer_->get_current_scope_node();
                
                std::cout << "[Parser] For-loop declaration '" << var_name 
                          << "' def_scope=" << assignment->definition_scope
                          << ", assign_scope=" << assignment->assignment_scope << std::endl;
            }
            
            // Set the declaration kind on the ForLoop for scope analysis
            for_loop->init_declaration_kind = assignment_kind;
            for_loop->creates_block_scope = (assignment_kind == Assignment::LET || assignment_kind == Assignment::CONST);
            
            for_loop->init = std::move(assignment);
        } else {
            for_loop->init = parse_statement();
        }
    }
    
    if (match(TokenType::SEMICOLON)) {
        // Parse condition if present
        if (!check(TokenType::SEMICOLON)) {
            for_loop->condition = parse_expression();
        }
        
        if (!match(TokenType::SEMICOLON)) {
            throw std::runtime_error("Expected ';' after for condition");
        }
        
        // For parenthesized for loops, always try to parse the update part
        // unless we're at the closing paren (which means no update statement)
        if (has_parens && !check(TokenType::RPAREN)) {
            for_loop->update = parse_expression();
        } else if (!has_parens && !check(TokenType::RBRACE)) {
            for_loop->update = parse_statement();
        }
    }
    
    if (has_parens && !match(TokenType::RPAREN)) {
        throw std::runtime_error("Expected ')' after for header");
    }
    
    if (match(TokenType::LBRACE)) {
        while (!check(TokenType::RBRACE) && !check(TokenType::EOF_TOKEN)) {
            for_loop->body.push_back(parse_statement());
        }
        
        if (!match(TokenType::RBRACE)) {
            throw std::runtime_error("Expected '}' after for body");
        }
    } else {
        for_loop->body.push_back(parse_statement());
    }
    
    // Exit lexical scope for for loop and capture scope info
    if (lexical_scope_analyzer_) {
        for_loop->lexical_scope = lexical_scope_analyzer_->exit_scope();
    }
    
    return std::move(for_loop);
}

std::unique_ptr<ASTNode> Parser::parse_while_statement() {
    if (!match(TokenType::WHILE)) {
        throw std::runtime_error("Expected 'while'");
    }
    
    bool has_parens = false;
    if (check(TokenType::LPAREN)) {
        has_parens = true;
        advance();
    }
    
    // Parse condition
    auto condition = parse_expression();
    
    if (has_parens && !match(TokenType::RPAREN)) {
        throw std::runtime_error("Expected ')' after while condition");
    }
    
    auto while_loop = std::make_unique<WhileLoop>(std::move(condition));
    
    // Enter lexical scope for while loop body
    if (lexical_scope_analyzer_) {
        lexical_scope_analyzer_->enter_scope();
    }
    
    // Parse body - support both braced and single statement
    if (match(TokenType::LBRACE)) {
        while (!check(TokenType::RBRACE) && !check(TokenType::EOF_TOKEN)) {
            while_loop->body.push_back(parse_statement());
        }
        
        if (!match(TokenType::RBRACE)) {
            throw std::runtime_error("Expected '}' after while body");
        }
    } else {
        while_loop->body.push_back(parse_statement());
    }
    
    // Exit lexical scope for while loop and capture scope info
    if (lexical_scope_analyzer_) {
        while_loop->lexical_scope = lexical_scope_analyzer_->exit_scope();
    }
    
    return std::move(while_loop);
}

std::unique_ptr<ASTNode> Parser::parse_for_each_statement() {
    if (!match(TokenType::FOR)) {
        throw std::runtime_error("Expected 'for'");
    }
    
    if (!match(TokenType::EACH)) {
        throw std::runtime_error("Expected 'each' after 'for'");
    }
    
    // Enter lexical scope for for-each loop
    if (lexical_scope_analyzer_) {
        lexical_scope_analyzer_->enter_scope();
    }
    
    // Parse: index, value (where index represents index for arrays or key for objects)
    if (!match(TokenType::IDENTIFIER)) {
        throw std::runtime_error("Expected index/key variable name");
    }
    std::string index_var = tokens[pos - 1].value;
    
    // Declare index variable in lexical scope
    if (lexical_scope_analyzer_) {
        lexical_scope_analyzer_->declare_variable(index_var, "let");
    }
    
    if (!match(TokenType::COMMA)) {
        throw std::runtime_error("Expected ',' after index/key variable");
    }
    
    if (!match(TokenType::IDENTIFIER)) {
        throw std::runtime_error("Expected value variable name");
    }
    std::string value_var = tokens[pos - 1].value;
    
    // Declare value variable in lexical scope
    if (lexical_scope_analyzer_) {
        lexical_scope_analyzer_->declare_variable(value_var, "let");
    }
    
    if (!match(TokenType::IN)) {
        throw std::runtime_error("Expected 'in' after variable declarations");
    }
    
    // Parse the iterable expression
    auto iterable = parse_expression();
    
    auto for_each = std::make_unique<ForEachLoop>(index_var, value_var);
    for_each->iterable = std::move(iterable);
    
    // Parse the body
    if (match(TokenType::LBRACE)) {
        while (!check(TokenType::RBRACE) && !check(TokenType::EOF_TOKEN)) {
            for_each->body.push_back(parse_statement());
        }
        
        if (!match(TokenType::RBRACE)) {
            throw std::runtime_error("Expected '}' after for-each body");
        }
    } else {
        for_each->body.push_back(parse_statement());
    }
    
    // Exit lexical scope for for-each loop and capture scope info
    if (lexical_scope_analyzer_) {
        for_each->lexical_scope = lexical_scope_analyzer_->exit_scope();
    }
    
    return std::move(for_each);
}

std::unique_ptr<ASTNode> Parser::parse_for_in_statement() {
    if (!match(TokenType::FOR)) {
        throw std::runtime_error("Expected 'for'");
    }
    
    // Enter lexical scope for for-in loop
    if (lexical_scope_analyzer_) {
        lexical_scope_analyzer_->enter_scope();
    }
    
    // Check if parentheses are used
    bool has_parentheses = check(TokenType::LPAREN);
    if (has_parentheses) {
        advance(); // consume '('
    }
    
    // Optional variable declaration (let/var/const)
    bool has_declaration = false;
    std::string decl_type = "let"; // default
    if (check(TokenType::LET) || check(TokenType::VAR) || check(TokenType::CONST)) {
        has_declaration = true;
        if (check(TokenType::LET)) decl_type = "let";
        else if (check(TokenType::VAR)) decl_type = "var";
        else if (check(TokenType::CONST)) decl_type = "const";
        advance(); // consume the declaration keyword
    }
    
    // Parse the key variable name
    if (!match(TokenType::IDENTIFIER)) {
        throw std::runtime_error("Expected variable name in for-in loop");
    }
    std::string key_var = tokens[pos - 1].value;
    
    // Declare key variable in lexical scope
    if (lexical_scope_analyzer_) {
        lexical_scope_analyzer_->declare_variable(key_var, decl_type);
    }
    
    if (!match(TokenType::IN)) {
        throw std::runtime_error("Expected 'in' after variable name in for-in loop");
    }
    
    // Parse the object expression
    auto object = parse_expression();
    
    if (has_parentheses) {
        if (!match(TokenType::RPAREN)) {
            throw std::runtime_error("Expected ')' after for-in header");
        }
    }
    
    auto for_in = std::make_unique<ForInStatement>(key_var);
    for_in->object = std::move(object);
    
    // Parse the body
    if (match(TokenType::LBRACE)) {
        while (!check(TokenType::RBRACE) && !check(TokenType::EOF_TOKEN)) {
            for_in->body.push_back(parse_statement());
        }
        
        if (!match(TokenType::RBRACE)) {
            throw std::runtime_error("Expected '}' after for-in body");
        }
    } else {
        for_in->body.push_back(parse_statement());
    }
    
    // Exit lexical scope for for-in loop and capture scope info
    if (lexical_scope_analyzer_) {
        for_in->lexical_scope = lexical_scope_analyzer_->exit_scope();
    }
    
    return std::move(for_in);
}

std::unique_ptr<ASTNode> Parser::parse_return_statement() {
    if (!match(TokenType::RETURN)) {
        throw std::runtime_error("Expected 'return'");
    }
    
    std::unique_ptr<ExpressionNode> value = nullptr;
    if (!check(TokenType::SEMICOLON) && !check(TokenType::RBRACE) && !check(TokenType::EOF_TOKEN)) {
        value = parse_expression();
        
        // GC Integration: Track escaped value in return statement
        if (gc_integration_ && value) {
            // Extract variable name if it's an identifier
            // For now, just mark that a return happened
            gc_integration_->mark_return_value("return_value");
        }
    }
    
    if (match(TokenType::SEMICOLON)) {
        // Optional semicolon
    }
    
    return std::make_unique<ReturnStatement>(std::move(value));
}

std::unique_ptr<ASTNode> Parser::parse_break_statement() {
    if (!match(TokenType::BREAK)) {
        throw std::runtime_error("Expected 'break'");
    }
    
    if (match(TokenType::SEMICOLON)) {
        // Optional semicolon
    }
    
    return std::make_unique<BreakStatement>();
}

std::unique_ptr<ASTNode> Parser::parse_free_statement() {
    if (!match(TokenType::FREE)) {
        throw std::runtime_error("Expected 'free'");
    }
    
    bool is_shallow = false;
    
    // Check for optional 'shallow' keyword
    if (check(TokenType::SHALLOW)) {
        is_shallow = true;
        advance();
    }
    
    // If no 'shallow' keyword, this is a deep free - throw error
    if (!is_shallow) {
        throw std::runtime_error("Deep free not yet implemented. Use 'free shallow' for shallow freeing.");
    }
    
    // Parse the target expression to free
    auto target = parse_expression();
    
    if (match(TokenType::SEMICOLON)) {
        // Optional semicolon
    }
    
    return std::make_unique<FreeStatement>(std::move(target), is_shallow);
}

std::unique_ptr<ASTNode> Parser::parse_switch_statement() {
    if (!match(TokenType::SWITCH)) {
        throw std::runtime_error("Expected 'switch'");
    }
    
    // Parse discriminant (the expression to switch on)
    bool has_parens = false;
    if (check(TokenType::LPAREN)) {
        has_parens = true;
        advance();
    }
    
    auto discriminant = parse_expression();
    
    if (has_parens && !match(TokenType::RPAREN)) {
        throw std::runtime_error("Expected ')' after switch expression");
    }
    
    auto switch_stmt = std::make_unique<SwitchStatement>(std::move(discriminant));
    
    // Parse switch body
    if (!match(TokenType::LBRACE)) {
        throw std::runtime_error("Expected '{' after switch expression");
    }
    
    // Parse case clauses
    while (!check(TokenType::RBRACE) && !check(TokenType::EOF_TOKEN)) {
        switch_stmt->cases.push_back(parse_case_clause());
    }
    
    if (!match(TokenType::RBRACE)) {
        throw std::runtime_error("Expected '}' after switch body");
    }
    
    return switch_stmt;
}

std::unique_ptr<CaseClause> Parser::parse_case_clause() {
    std::unique_ptr<CaseClause> case_clause;
    
    if (match(TokenType::CASE)) {
        // Parse case value
        auto value = parse_expression();
        case_clause = std::make_unique<CaseClause>(std::move(value));
        
        if (!match(TokenType::COLON)) {
            throw std::runtime_error("Expected ':' after case value");
        }
    } else if (match(TokenType::DEFAULT)) {
        // Parse default case
        case_clause = std::make_unique<CaseClause>();  // Default constructor
        
        if (!match(TokenType::COLON)) {
            throw std::runtime_error("Expected ':' after 'default'");
        }
    } else {
        throw std::runtime_error("Expected 'case' or 'default' in switch statement");
    }
    
    // Check for optional block syntax: case 0: { ... }
    if (check(TokenType::LBRACE)) {
        auto block = std::make_unique<BlockStatement>();
        advance(); // consume '{'
        
        while (!check(TokenType::RBRACE) && !check(TokenType::EOF_TOKEN)) {
            block->body.push_back(parse_statement());
        }
        
        if (!match(TokenType::RBRACE)) {
            throw std::runtime_error("Expected '}' after case block");
        }
        
        case_clause->block_body = std::move(block);
    } else {
        // Parse case body (statements until next case/default/end of switch)
        while (!check(TokenType::CASE) && !check(TokenType::DEFAULT) && 
               !check(TokenType::RBRACE) && !check(TokenType::EOF_TOKEN)) {
            case_clause->body.push_back(parse_statement());
        }
    }
    
    return case_clause;
}

std::unique_ptr<ASTNode> Parser::parse_expression_statement() {
    auto expr = parse_expression();
    
    if (match(TokenType::SEMICOLON)) {
        // Optional semicolon
    }
    
    return std::move(expr);
}

DataType Parser::parse_type() {
    // Handle typed array syntax like [int32], [float32], etc.
    if (match(TokenType::LBRACKET)) {
        if (!match(TokenType::IDENTIFIER)) {
            throw std::runtime_error("Expected type name in array brackets");
        }
        
        std::string element_type = tokens[pos - 1].value;
        
        if (!match(TokenType::RBRACKET)) {
            throw std::runtime_error("Expected ']' after array element type");
        }
        
        // Store the element type for the Assignment to access
        if (element_type == "int8") last_parsed_array_element_type = DataType::INT8;
        else if (element_type == "int16") last_parsed_array_element_type = DataType::INT16;
        else if (element_type == "int32") last_parsed_array_element_type = DataType::INT32;
        else if (element_type == "int64") last_parsed_array_element_type = DataType::INT64;
        else if (element_type == "uint8") last_parsed_array_element_type = DataType::UINT8;
        else if (element_type == "uint16") last_parsed_array_element_type = DataType::UINT16;
        else if (element_type == "uint32") last_parsed_array_element_type = DataType::UINT32;
        else if (element_type == "uint64") last_parsed_array_element_type = DataType::UINT64;
        else if (element_type == "float32") last_parsed_array_element_type = DataType::FLOAT32;
        else if (element_type == "float64" || element_type == "number") last_parsed_array_element_type = DataType::FLOAT64;
        else if (element_type == "boolean") last_parsed_array_element_type = DataType::BOOLEAN;
        else if (element_type == "string") last_parsed_array_element_type = DataType::STRING;
        else last_parsed_array_element_type = DataType::ANY;
        
        // Always return DataType::ARRAY for array types
        return DataType::ARRAY;
    }
    
    // Clear element type for non-array types
    last_parsed_array_element_type = DataType::ANY;
    
    if (!match(TokenType::IDENTIFIER)) {
        throw std::runtime_error("Expected type name");
    }
    
    std::string type_name = tokens[pos - 1].value;
    
    if (type_name == "int8") return DataType::INT8;
    if (type_name == "int16") return DataType::INT16;
    if (type_name == "int32") return DataType::INT32;
    if (type_name == "int64") return DataType::INT64;
    if (type_name == "uint8") return DataType::UINT8;
    if (type_name == "uint16") return DataType::UINT16;
    if (type_name == "uint32") return DataType::UINT32;
    if (type_name == "uint64") return DataType::UINT64;
    if (type_name == "float32") return DataType::FLOAT32;
    if (type_name == "float64") return DataType::FLOAT64;
    if (type_name == "number") return DataType::FLOAT64;
    if (type_name == "boolean") return DataType::BOOLEAN;
    if (type_name == "string") return DataType::STRING;
    if (type_name == "tensor") return DataType::TENSOR;
    if (type_name == "array") return DataType::ARRAY;
    if (type_name == "void") return DataType::VOID;
    if (type_name == "any") return DataType::ANY;
    
    return DataType::ANY;
}

std::unique_ptr<ASTNode> Parser::parse_class_declaration() {
    if (!match(TokenType::CLASS)) {
        throw std::runtime_error("Expected 'class'");
    }
    
    if (!check(TokenType::IDENTIFIER)) {
        throw std::runtime_error("Expected class name");
    }
    
    std::string class_name = current_token().value;
    advance();
    
    auto class_decl = std::make_unique<ClassDecl>(class_name);
    
    // Handle inheritance
    if (match(TokenType::EXTENDS)) {
        // Parse comma-separated list of parent classes
        do {
            if (!check(TokenType::IDENTIFIER)) {
                throw std::runtime_error("Expected parent class name");
            }
            class_decl->parent_classes.push_back(current_token().value);
            advance();
        } while (match(TokenType::COMMA));
    }
    
    if (!match(TokenType::LBRACE)) {
        throw std::runtime_error("Expected '{' after class name");
    }
    
    // Parse class body
    while (!check(TokenType::RBRACE) && !check(TokenType::EOF_TOKEN)) {
        // Check for access modifiers
        bool is_private = false;
        bool is_protected = false;
        bool is_static = false;
        
        if (match(TokenType::PRIVATE)) {
            is_private = true;
        } else if (match(TokenType::PROTECTED)) {
            is_protected = true;
        } else if (match(TokenType::PUBLIC)) {
            // public is default, just consume token
        }
        
        if (match(TokenType::STATIC)) {
            is_static = true;
        }
        
        if (check(TokenType::CONSTRUCTOR)) {
            if (class_decl->constructor) {
                throw std::runtime_error("Class can only have one constructor");
            }
            class_decl->constructor = parse_constructor_declaration(class_decl->name);
        } else if (check(TokenType::OPERATOR)) {
            // Operator overloading declaration
            auto operator_overload = parse_operator_overload_declaration(class_decl->name);
            class_decl->operator_overloads.push_back(std::move(operator_overload));
        } else if (check(TokenType::IDENTIFIER)) {
            // Could be field or method
            std::string member_name = current_token().value;
            advance();
            
            if (check(TokenType::COLON)) {
                // Field declaration: name: type [= defaultValue];
                advance(); // consume ':'
                DataType field_type = parse_type();
                
                Variable field;
                field.name = member_name;
                field.type = field_type;
                field.is_mutable = true;
                field.is_static = is_static;
                
                // Check for default value
                if (match(TokenType::ASSIGN)) {
                    // Parse the default value expression
                    field.default_value = parse_expression();
                }
                
                class_decl->fields.push_back(field);
                
                if (match(TokenType::SEMICOLON)) {
                    // Optional semicolon
                }
            } else if (check(TokenType::LPAREN)) {
                // Method declaration
                pos--; // Go back to method name
                auto method = parse_method_declaration(class_decl->name);
                method->is_static = is_static;
                method->is_private = is_private;
                method->is_protected = is_protected;
                class_decl->methods.push_back(std::move(method));
            } else {
                throw std::runtime_error("Expected ':' for field or '(' for method");
            }
        } else {
            throw std::runtime_error("Expected constructor, operator, field, or method declaration");
        }
    }
    
    if (!match(TokenType::RBRACE)) {
        throw std::runtime_error("Expected '}' after class body");
    }
    
    return std::move(class_decl);
}

std::unique_ptr<MethodDecl> Parser::parse_method_declaration(const std::string& class_name) {
    if (!check(TokenType::IDENTIFIER)) {
        throw std::runtime_error("Expected method name");
    }
    
    std::string method_name = current_token().value;
    advance();
    
    auto method = std::make_unique<MethodDecl>(method_name, class_name);
    
    if (!match(TokenType::LPAREN)) {
        throw std::runtime_error("Expected '(' after method name");
    }
    
    // Parse parameters
    while (!check(TokenType::RPAREN) && !check(TokenType::EOF_TOKEN)) {
        if (!check(TokenType::IDENTIFIER)) {
            throw std::runtime_error("Expected parameter name");
        }
        
        Variable param;
        param.name = current_token().value;
        advance();
        
        if (match(TokenType::COLON)) {
            param.type = parse_type();
        } else {
            param.type = DataType::ANY;
        }
        
        method->parameters.push_back(param);
        
        if (!match(TokenType::COMMA)) {
            break;
        }
    }
    
    if (!match(TokenType::RPAREN)) {
        throw std::runtime_error("Expected ')' after parameters");
    }
    
    // Parse return type
    if (match(TokenType::COLON)) {
        method->return_type = parse_type();
    }
    
    if (!match(TokenType::LBRACE)) {
        throw std::runtime_error("Expected '{' before method body");
    }
    
    // Parse method body
    while (!check(TokenType::RBRACE) && !check(TokenType::EOF_TOKEN)) {
        method->body.push_back(parse_statement());
    }
    
    if (!match(TokenType::RBRACE)) {
        throw std::runtime_error("Expected '}' after method body");
    }
    
    return method;
}

std::unique_ptr<ConstructorDecl> Parser::parse_constructor_declaration(const std::string& class_name) {
    if (!match(TokenType::CONSTRUCTOR)) {
        throw std::runtime_error("Expected 'constructor'");
    }
    
    auto constructor = std::make_unique<ConstructorDecl>(class_name);
    
    if (!match(TokenType::LPAREN)) {
        throw std::runtime_error("Expected '(' after constructor");
    }
    
    // Parse parameters
    while (!check(TokenType::RPAREN) && !check(TokenType::EOF_TOKEN)) {
        if (!check(TokenType::IDENTIFIER)) {
            throw std::runtime_error("Expected parameter name");
        }
        
        Variable param;
        param.name = current_token().value;
        advance();
        
        if (match(TokenType::COLON)) {
            param.type = parse_type();
        } else {
            param.type = DataType::ANY;
        }
        
        constructor->parameters.push_back(param);
        
        if (!match(TokenType::COMMA)) {
            break;
        }
    }
    
    if (!match(TokenType::RPAREN)) {
        throw std::runtime_error("Expected ')' after constructor parameters");
    }
    
    if (!match(TokenType::LBRACE)) {
        throw std::runtime_error("Expected '{' before constructor body");
    }
    
    // Parse constructor body
    while (!check(TokenType::RBRACE) && !check(TokenType::EOF_TOKEN)) {
        constructor->body.push_back(parse_statement());
    }
    
    if (!match(TokenType::RBRACE)) {
        throw std::runtime_error("Expected '}' after constructor body");
    }
    
    return constructor;
}

std::unique_ptr<ASTNode> Parser::parse_import_statement() {
    if (!match(TokenType::IMPORT)) {
        throw std::runtime_error("Expected 'import'");
    }
    
    auto import_stmt = std::make_unique<ImportStatement>("");
    
    // Handle different import patterns:
    // import defaultExport from "module"
    // import { named1, named2 } from "module" 
    // import { named as alias } from "module"
    // import * as namespace from "module"
    // import "module" (side-effect only)
    
    if (check(TokenType::MULTIPLY)) {
        // import * as namespace from "module"
        advance(); // consume *
        
        if (!match(TokenType::AS)) {
            throw std::runtime_error("Expected 'as' after '* in import");
        }
        
        if (!check(TokenType::IDENTIFIER)) {
            throw std::runtime_error("Expected identifier after 'as'");
        }
        
        import_stmt->is_namespace_import = true;
        import_stmt->namespace_name = current_token().value;
        advance();
        
    } else if (check(TokenType::LBRACE)) {
        // import { named1, named2, ... } from "module"
        advance(); // consume {
        
        while (!check(TokenType::RBRACE) && !check(TokenType::EOF_TOKEN)) {
            if (!check(TokenType::IDENTIFIER)) {
                throw std::runtime_error("Expected identifier in import specifier");
            }
            
            std::string imported_name = current_token().value;
            advance();
            
            std::string local_name = imported_name;
            if (match(TokenType::AS)) {
                if (!check(TokenType::IDENTIFIER)) {
                    throw std::runtime_error("Expected identifier after 'as'");
                }
                local_name = current_token().value;
                advance();
            }
            
            import_stmt->specifiers.emplace_back(imported_name, local_name);
            
            if (!check(TokenType::RBRACE)) {
                if (!match(TokenType::COMMA)) {
                    throw std::runtime_error("Expected ',' or '}' in import specifiers");
                }
            }
        }
        
        if (!match(TokenType::RBRACE)) {
            throw std::runtime_error("Expected '}' after import specifiers");
        }
        
    } else if (check(TokenType::IDENTIFIER)) {
        // import defaultExport from "module" or import identifier from "module"
        std::string name = current_token().value;
        advance();
        
        ImportSpecifier spec(name);
        spec.is_default = true;
        import_stmt->specifiers.push_back(spec);
    }
    
    // Parse "from" clause (optional for side-effect imports)
    if (match(TokenType::FROM)) {
        if (!check(TokenType::STRING)) {
            throw std::runtime_error("Expected string literal after 'from'");
        }
        
        import_stmt->module_path = current_token().value;
        // Remove quotes from string literal (handle both single and double quotes)
        if (import_stmt->module_path.length() >= 2) {
            char first_char = import_stmt->module_path[0];
            char last_char = import_stmt->module_path[import_stmt->module_path.length() - 1];
            if ((first_char == '"' && last_char == '"') || (first_char == '\'' && last_char == '\'')) {
                import_stmt->module_path = import_stmt->module_path.substr(1, import_stmt->module_path.length() - 2);
            }
        }
        advance();
    } else if (check(TokenType::STRING)) {
        // Side-effect import: import "module"
        import_stmt->module_path = current_token().value;
        // Remove quotes from string literal (handle both single and double quotes)
        if (import_stmt->module_path.length() >= 2) {
            char first_char = import_stmt->module_path[0];
            char last_char = import_stmt->module_path[import_stmt->module_path.length() - 1];
            if ((first_char == '"' && last_char == '"') || (first_char == '\'' && last_char == '\'')) {
                import_stmt->module_path = import_stmt->module_path.substr(1, import_stmt->module_path.length() - 2);
            }
        }
        advance();
    } else {
        throw std::runtime_error("Expected 'from' clause or string literal in import");
    }
    
    match(TokenType::SEMICOLON); // Optional semicolon
    
    return std::move(import_stmt);
}

std::unique_ptr<ASTNode> Parser::parse_export_statement() {
    if (!match(TokenType::EXPORT)) {
        throw std::runtime_error("Expected 'export'");
    }
    
    auto export_stmt = std::make_unique<ExportStatement>();
    
    // Handle different export patterns:
    // export default expression
    // export { name1, name2 }
    // export { name as alias }
    // export function name() {}
    // export var/let/const name = value
    
    if (current_token().type == TokenType::IDENTIFIER && current_token().value == "default") {
        // export default ...
        advance(); // consume "default"
        export_stmt->is_default = true;
        
        // The rest is an expression or declaration
        export_stmt->declaration = parse_statement();
        
    } else if (check(TokenType::LBRACE)) {
        // export { name1, name2, ... }
        advance(); // consume {
        
        while (!check(TokenType::RBRACE) && !check(TokenType::EOF_TOKEN)) {
            if (!check(TokenType::IDENTIFIER)) {
                throw std::runtime_error("Expected identifier in export specifier");
            }
            
            std::string local_name = current_token().value;
            advance();
            
            std::string exported_name = local_name;
            if (match(TokenType::AS)) {
                if (!check(TokenType::IDENTIFIER)) {
                    throw std::runtime_error("Expected identifier after 'as'");
                }
                exported_name = current_token().value;
                advance();
            }
            
            export_stmt->specifiers.emplace_back(local_name, exported_name);
            
            if (!check(TokenType::RBRACE)) {
                if (!match(TokenType::COMMA)) {
                    throw std::runtime_error("Expected ',' or '}' in export specifiers");
                }
            }
        }
        
        if (!match(TokenType::RBRACE)) {
            throw std::runtime_error("Expected '}' after export specifiers");
        }
        
    } else {
        // export declaration (function, var, class, etc.)
        export_stmt->declaration = parse_statement();
    }
    
    match(TokenType::SEMICOLON); // Optional semicolon
    
    return std::move(export_stmt);
}

std::vector<std::unique_ptr<ASTNode>> Parser::parse() {
    std::vector<std::unique_ptr<ASTNode>> statements;
    
    // GC Integration: Initialize GC tracking for top-level scope
    if (gc_integration_) {
        gc_integration_->enter_scope("global", false);
    }
    
    // Lexical Scope System: Initialize global scope
    if (lexical_scope_analyzer_) {
        lexical_scope_analyzer_->enter_scope();
    }
    
    while (!check(TokenType::EOF_TOKEN)) {
        statements.push_back(parse_statement());
    }
    
    // GC Integration: Finalize escape analysis
    if (gc_integration_) {
        gc_integration_->exit_scope();
        gc_integration_->finalize_analysis();
    }
    
    // Lexical Scope System: Close global scope and perform variable packing
    if (lexical_scope_analyzer_) {
        lexical_scope_analyzer_->exit_scope();
    }
    
    return statements;
}

std::unique_ptr<OperatorOverloadDecl> Parser::parse_operator_overload_declaration(const std::string& class_name) {
    if (!match(TokenType::OPERATOR)) {
        throw std::runtime_error("Expected 'operator'");
    }
    
    // Parse the operator symbol
    TokenType operator_type = current_token().type;
    if (operator_type != TokenType::PLUS && operator_type != TokenType::MINUS && 
        operator_type != TokenType::MULTIPLY && operator_type != TokenType::DIVIDE &&
        operator_type != TokenType::LBRACKET && operator_type != TokenType::EQUAL &&
        operator_type != TokenType::NOT_EQUAL && operator_type != TokenType::LESS &&
        operator_type != TokenType::GREATER && operator_type != TokenType::LESS_EQUAL &&
        operator_type != TokenType::GREATER_EQUAL) {
        throw std::runtime_error("Invalid operator for overloading");
    }
    
    advance(); // consume operator token
    
    // Special handling for [] operator
    if (operator_type == TokenType::LBRACKET) {
        if (!match(TokenType::RBRACKET)) {
            throw std::runtime_error("Expected ']' after '['");
        }
    }
    
    auto operator_decl = std::make_unique<OperatorOverloadDecl>(operator_type, class_name);
    
    if (!match(TokenType::LPAREN)) {
        throw std::runtime_error("Expected '(' after operator");
    }
    
    // Parse parameters
    while (!check(TokenType::RPAREN) && !check(TokenType::EOF_TOKEN)) {
        if (!check(TokenType::IDENTIFIER)) {
            throw std::runtime_error("Expected parameter name");
        }
        
        Variable param;
        param.name = current_token().value;
        advance();
        
        if (match(TokenType::COLON)) {
            param.type = parse_type();
        } else {
            param.type = DataType::ANY; // Untyped parameter
        }
        
        param.is_mutable = true;
        operator_decl->parameters.push_back(param);
        
        if (!check(TokenType::RPAREN)) {
            if (!match(TokenType::COMMA)) {
                throw std::runtime_error("Expected ',' or ')' in operator parameters");
            }
        }
    }
    
    if (!match(TokenType::RPAREN)) {
        throw std::runtime_error("Expected ')' after operator parameters");
    }
    
    // Optional return type
    if (match(TokenType::COLON)) {
        operator_decl->return_type = parse_type();
    }
    
    // Parse function body
    if (!match(TokenType::LBRACE)) {
        throw std::runtime_error("Expected '{' for operator body");
    }
    
    while (!check(TokenType::RBRACE) && !check(TokenType::EOF_TOKEN)) {
        operator_decl->body.push_back(parse_statement());
    }
    
    if (!match(TokenType::RBRACE)) {
        throw std::runtime_error("Expected '}' after operator body");
    }
    
    return operator_decl;
}

// LEXICAL SCOPE ANALYSIS METHODS

std::vector<std::string> Parser::analyze_function_variable_captures(FunctionExpression* func_expr) {
    std::cout << "[DEBUG] Parser::analyze_function_variable_captures - Analyzing function body" << std::endl;
    std::vector<std::string> captured_vars;
    
    if (!func_expr) {
        return captured_vars;
    }
    
    // Walk through all statements in the function body to find variable references
    for (const auto& stmt : func_expr->body) {
        find_variable_references_in_node(stmt.get(), captured_vars);
    }
    
    // Remove duplicates
    std::sort(captured_vars.begin(), captured_vars.end());
    captured_vars.erase(std::unique(captured_vars.begin(), captured_vars.end()), captured_vars.end());
    
    std::cout << "[DEBUG] Parser::analyze_function_variable_captures - Found " << captured_vars.size() << " unique variable references" << std::endl;
    return captured_vars;
}

void Parser::find_variable_references_in_node(ASTNode* node, std::vector<std::string>& variables) {
    if (!node) return;
    
    // Check different node types for variable references
    if (auto* identifier = dynamic_cast<Identifier*>(node)) {
        std::cout << "[DEBUG] Parser::find_variable_references_in_node - Found variable reference: '" << identifier->name << "'" << std::endl;
        variables.push_back(identifier->name);
    }
    else if (auto* assignment = dynamic_cast<Assignment*>(node)) {
        // Assignment target and value
        variables.push_back(assignment->variable_name);
        if (assignment->value) {
            find_variable_references_in_node(assignment->value.get(), variables);
        }
    }
    else if (auto* func_call = dynamic_cast<FunctionCall*>(node)) {
        // Function call arguments
        for (const auto& arg : func_call->arguments) {
            find_variable_references_in_node(arg.get(), variables);
        }
    }
    else if (auto* method_call = dynamic_cast<MethodCall*>(node)) {
        // Method call - add object_name and arguments
        variables.push_back(method_call->object_name);
        for (const auto& arg : method_call->arguments) {
            find_variable_references_in_node(arg.get(), variables);
        }
    }
    else if (auto* binary_op = dynamic_cast<BinaryOp*>(node)) {
        // Binary operation operands
        if (binary_op->left) {
            find_variable_references_in_node(binary_op->left.get(), variables);
        }
        if (binary_op->right) {
            find_variable_references_in_node(binary_op->right.get(), variables);
        }
    }
    // Add more node types as needed...
    
    std::cout << "[DEBUG] Parser::find_variable_references_in_node - Processed node of type: " << typeid(*node).name() << std::endl;
}

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
    
    // Enter lexical scope for arrow function (even single expressions create scope)
    if (lexical_scope_analyzer_) {
        lexical_scope_analyzer_->enter_scope();
        
        // Declare parameter in the new scope
        lexical_scope_analyzer_->declare_variable(param_name, "let");
    }
    
    // Parse the arrow function body
    if (check(TokenType::LBRACE)) {
        // Block body: x => { return x + 1; }
        arrow_func->is_single_expression = false;
        match(TokenType::LBRACE);
        
        // Parse statements in the block body
        while (!check(TokenType::RBRACE) && !check(TokenType::EOF_TOKEN)) {
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
    
    // Exit lexical scope for arrow function and capture scope info
    if (lexical_scope_analyzer_) {
        arrow_func->lexical_scope = lexical_scope_analyzer_->exit_scope();
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
    
    // Enter lexical scope for arrow function
    if (lexical_scope_analyzer_) {
        lexical_scope_analyzer_->enter_scope();
        
        // Declare all parameters in the new scope
        for (const auto& param : params) {
            lexical_scope_analyzer_->declare_variable(param.name, "let");
        }
    }
    
    // Parse the arrow function body (same logic as single parameter version)
    if (check(TokenType::LBRACE)) {
        // Block body: (x, y) => { return x + y; }
        arrow_func->is_single_expression = false;
        match(TokenType::LBRACE);
        
        while (!check(TokenType::RBRACE) && !check(TokenType::EOF_TOKEN)) {
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
    
    // Exit lexical scope for arrow function and capture scope info
    if (lexical_scope_analyzer_) {
        arrow_func->lexical_scope = lexical_scope_analyzer_->exit_scope();
    }
    
    return arrow_func;
}

// Try-Catch-Throw-Finally Statement Parsing

std::unique_ptr<ASTNode> Parser::parse_try_statement() {
    if (!match(TokenType::TRY)) {
        throw std::runtime_error("Expected 'try'");
    }
    
    auto try_stmt = std::make_unique<TryStatement>();
    
    // Parse try block
    if (!match(TokenType::LBRACE)) {
        throw std::runtime_error("Expected '{' after 'try'");
    }
    
    // Parse try body
    while (!check(TokenType::RBRACE) && !check(TokenType::EOF_TOKEN)) {
        try_stmt->try_body.push_back(parse_statement());
    }
    
    if (!match(TokenType::RBRACE)) {
        throw std::runtime_error("Expected '}' after try body");
    }
    
    // Parse optional catch clause
    if (check(TokenType::CATCH)) {
        try_stmt->catch_clause = parse_catch_clause();
    }
    
    // Parse optional finally clause
    if (check(TokenType::FINALLY)) {
        advance(); // consume 'finally'
        
        if (!match(TokenType::LBRACE)) {
            throw std::runtime_error("Expected '{' after 'finally'");
        }
        
        while (!check(TokenType::RBRACE) && !check(TokenType::EOF_TOKEN)) {
            try_stmt->finally_body.push_back(parse_statement());
        }
        
        if (!match(TokenType::RBRACE)) {
            throw std::runtime_error("Expected '}' after finally body");
        }
    }
    
    // Ensure we have at least a catch or finally clause
    if (!try_stmt->catch_clause && try_stmt->finally_body.empty()) {
        throw std::runtime_error("Try statement must have either catch or finally clause");
    }
    
    return try_stmt;
}

std::unique_ptr<CatchClause> Parser::parse_catch_clause() {
    if (!match(TokenType::CATCH)) {
        throw std::runtime_error("Expected 'catch'");
    }
    
    // Parse catch parameter: catch(error)
    if (!match(TokenType::LPAREN)) {
        throw std::runtime_error("Expected '(' after 'catch'");
    }
    
    if (!match(TokenType::IDENTIFIER)) {
        throw std::runtime_error("Expected parameter name in catch clause");
    }
    
    std::string param_name = tokens[pos - 1].value;
    
    if (!match(TokenType::RPAREN)) {
        throw std::runtime_error("Expected ')' after catch parameter");
    }
    
    auto catch_clause = std::make_unique<CatchClause>(param_name);
    
    // Parse catch body
    if (!match(TokenType::LBRACE)) {
        throw std::runtime_error("Expected '{' after catch clause");
    }
    
    while (!check(TokenType::RBRACE) && !check(TokenType::EOF_TOKEN)) {
        catch_clause->body.push_back(parse_statement());
    }
    
    if (!match(TokenType::RBRACE)) {
        throw std::runtime_error("Expected '}' after catch body");
    }
    
    return catch_clause;
}

std::unique_ptr<ASTNode> Parser::parse_throw_statement() {
    if (!match(TokenType::THROW)) {
        throw std::runtime_error("Expected 'throw'");
    }
    
    // Parse the expression to throw
    auto value = parse_expression();
    
    // Optional semicolon
    if (match(TokenType::SEMICOLON)) {
        // Semicolon consumed
    }
    
    return std::make_unique<ThrowStatement>(std::move(value));
}

std::unique_ptr<ASTNode> Parser::parse_block_statement() {
    if (!match(TokenType::LBRACE)) {
        throw std::runtime_error("Expected '{' for block statement");
    }
    
    auto block = std::make_unique<BlockStatement>();
    
    // GC Integration: Enter block scope for let/const variables
    if (gc_integration_) {
        gc_integration_->enter_scope("block", false);
    }
    
    // NEW: Enter lexical scope for block
    if (lexical_scope_analyzer_) {
        lexical_scope_analyzer_->enter_scope();
    }
    
    while (!check(TokenType::RBRACE) && !check(TokenType::EOF_TOKEN)) {
        block->body.push_back(parse_statement());
    }
    
    if (!match(TokenType::RBRACE)) {
        throw std::runtime_error("Expected '}' after block body");
    }
    
    // NEW: Exit lexical scope for block and capture scope info
    if (lexical_scope_analyzer_) {
        block->lexical_scope = lexical_scope_analyzer_->exit_scope();
    }
    
    // GC Integration: Exit block scope
    if (gc_integration_) {
        gc_integration_->exit_scope();
    }
    
    return block;
}
