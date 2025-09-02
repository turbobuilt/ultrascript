#include "parse_time_scope_tracker.h"
#include "compiler.h"
#include <iostream>

ParseTimeScopeTracker::ParseTimeScopeTracker() : current_depth_(0) {
    std::cout << "[ParseTimeScopeTracker] Initialized with depth " << current_depth_ << std::endl;
}

ParseTimeScopeTracker::~ParseTimeScopeTracker() {
    std::cout << "[ParseTimeScopeTracker] Destructor - completed " << completed_scopes_.size() << " scopes" << std::endl;
}

void ParseTimeScopeTracker::enter_scope(bool is_function_scope) {
    current_depth_++;
    std::cout << "[ParseTimeScopeTracker] Entering scope at depth " << current_depth_ 
              << " (function_scope=" << is_function_scope << ")" << std::endl;
    
    // Create new LexicalScopeNode
    auto new_scope = std::make_shared<LexicalScopeNode>(current_depth_, is_function_scope);
    
    // Add to scope stack and mapping
    scope_stack_.push_back(new_scope);
    depth_to_scope_node_[current_depth_] = new_scope.get();
    
    std::cout << "[ParseTimeScopeTracker] Created scope node at depth " << current_depth_ 
              << " address=" << (void*)new_scope.get() << std::endl;
}

std::unique_ptr<LexicalScopeNode> ParseTimeScopeTracker::exit_scope() {
    if (scope_stack_.empty()) {
        std::cerr << "[ParseTimeScopeTracker] ERROR: Attempting to exit scope when stack is empty" << std::endl;
        return nullptr;
    }
    
    std::cout << "[ParseTimeScopeTracker] Exiting scope at depth " << current_depth_ << std::endl;
    
    // Get the scope we're exiting
    auto exiting_scope_shared = scope_stack_.back();
    scope_stack_.pop_back();
    
    // Store in completed scopes to keep it alive
    completed_scopes_.push_back(exiting_scope_shared);
    
    std::cout << "[ParseTimeScopeTracker] Scope at depth " << current_depth_ 
              << " has " << exiting_scope_shared->declared_variables.size() << " declared variables"
              << " and " << exiting_scope_shared->declared_functions.size() << " declared functions" << std::endl;
    
    // Clean up depth mapping for this level
    cleanup_declarations_at_depth(current_depth_);
    depth_to_scope_node_.erase(current_depth_);
    
    current_depth_--;
    
    // Return a unique_ptr to the scope (transfers ownership to AST node)
    return std::unique_ptr<LexicalScopeNode>(
        new LexicalScopeNode(*exiting_scope_shared) // Copy constructor
    );
}

void ParseTimeScopeTracker::declare_variable(const std::string& name, const std::string& declaration_type, DataType data_type) {
    if (scope_stack_.empty()) {
        std::cerr << "[ParseTimeScopeTracker] ERROR: No current scope for variable declaration: " << name << std::endl;
        return;
    }
    
    auto current_scope = scope_stack_.back();
    current_scope->declare_variable(name);
    
    std::cout << "[ParseTimeScopeTracker] Declared variable '" << name << "' of type '" 
              << declaration_type << "' at depth " << current_depth_ << std::endl;
}

void ParseTimeScopeTracker::declare_variable(const std::string& name, const std::string& declaration_type) {
    declare_variable(name, declaration_type, DataType::ANY);
}

void ParseTimeScopeTracker::register_function_in_current_scope(FunctionDecl* func_decl) {
    if (scope_stack_.empty() || !func_decl) {
        std::cerr << "[ParseTimeScopeTracker] ERROR: Cannot register function in empty scope" << std::endl;
        return;
    }
    
    auto current_scope = scope_stack_.back();
    current_scope->register_function_declaration(func_decl);
    
    std::cout << "[ParseTimeScopeTracker] Registered function '" << func_decl->name 
              << "' at depth " << current_depth_ << std::endl;
}

void ParseTimeScopeTracker::register_function_expression_in_current_scope(FunctionExpression* func_expr) {
    if (scope_stack_.empty() || !func_expr) {
        std::cerr << "[ParseTimeScopeTracker] ERROR: Cannot register function expression in empty scope" << std::endl;
        return;
    }
    
    auto current_scope = scope_stack_.back();
    current_scope->register_function_expression(func_expr);
    
    std::cout << "[ParseTimeScopeTracker] Registered function expression at depth " << current_depth_ << std::endl;
}

LexicalScopeNode* ParseTimeScopeTracker::get_current_scope_node() const {
    if (scope_stack_.empty()) {
        return nullptr;
    }
    return scope_stack_.back().get();
}

LexicalScopeNode* ParseTimeScopeTracker::get_scope_node_for_depth(int depth) const {
    auto it = depth_to_scope_node_.find(depth);
    return (it != depth_to_scope_node_.end()) ? it->second : nullptr;
}

void ParseTimeScopeTracker::cleanup_declarations_at_depth(int depth) {
    // For parse-time tracker, we don't need to clean up variable declarations
    // since we're not tracking them in detail - just recording them in scopes
    std::cout << "[ParseTimeScopeTracker] Cleaned up declarations at depth " << depth << std::endl;
}
