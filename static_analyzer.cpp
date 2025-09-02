#include "static_analyzer.h"
#include "compiler.h"
#include <iostream>
#include <algorithm>

// Constructor and destructor
StaticAnalyzer::StaticAnalyzer() 
    : current_scope_(nullptr), current_depth_(0) {
    std::cout << "[StaticAnalyzer] Initialized static analyzer for pure AST analysis" << std::endl;
}

StaticAnalyzer::~StaticAnalyzer() {
    std::cout << "[StaticAnalyzer] Static analyzer destroyed" << std::endl;
}

// Main entry point: perform complete static analysis on the pure AST
void StaticAnalyzer::analyze(std::vector<std::unique_ptr<ASTNode>>& ast) {
    std::cout << "[StaticAnalyzer] Starting pure AST analysis on " << ast.size() << " AST nodes" << std::endl;
    
    // Phase 1: Build complete scope hierarchy from AST traversal
    build_scope_hierarchy_from_ast(ast);
    
    // Phase 2: Resolve all variable references using scope information
    resolve_all_variable_references_from_ast(ast);
    
    // Phase 3: Perform variable packing for all scopes
    perform_complete_variable_packing_from_scopes();
    
    // Phase 4: Compute function static analysis
    compute_complete_function_analysis_from_scopes();
    
    std::cout << "[StaticAnalyzer] Pure AST analysis complete" << std::endl;
    print_analysis_results();
}

void StaticAnalyzer::build_scope_hierarchy_from_ast(const std::vector<std::unique_ptr<ASTNode>>& ast) {
    std::cout << "[StaticAnalyzer] Building scope hierarchy from pure AST analysis..." << std::endl;
    
    // Create global scope (depth 1)
    auto global_scope = std::make_unique<LexicalScopeNode>(1, true);
    current_depth_ = 1;
    current_scope_ = global_scope.get();
    depth_to_scope_node_[1] = std::move(global_scope);
    
    // Traverse AST to find all scopes
    for (const auto& node : ast) {
        traverse_ast_node_for_scopes(node.get());
    }
    
    std::cout << "[StaticAnalyzer] Built " << depth_to_scope_node_.size() << " scope nodes from AST" << std::endl;
}

void StaticAnalyzer::resolve_all_variable_references_from_ast(const std::vector<std::unique_ptr<ASTNode>>& ast) {
    std::cout << "[StaticAnalyzer] Resolving variable references from AST..." << std::endl;
    
    // Reset for variable resolution
    current_depth_ = 1;
    current_scope_ = depth_to_scope_node_[1].get();
    
    if (!current_scope_) {
        std::cout << "[StaticAnalyzer] ERROR: Global scope not found!" << std::endl;
        return;
    }
    
    std::cout << "[StaticAnalyzer] Global scope found at depth 1, checking variable_declarations map..." << std::endl;
    std::cout << "[StaticAnalyzer] variable_declarations map size: " << current_scope_->variable_declarations.size() << std::endl;
    std::cout << "[StaticAnalyzer] variable_declarations map bucket_count: " << current_scope_->variable_declarations.bucket_count() << std::endl;
    
    // Workaround: Force initialization of the hash map if bucket count is 0
    if (current_scope_->variable_declarations.bucket_count() == 0) {
        std::cout << "[StaticAnalyzer] Forcing hash map initialization..." << std::endl;
        current_scope_->variable_declarations.rehash(1);
        std::cout << "[StaticAnalyzer] After rehash, bucket_count: " << current_scope_->variable_declarations.bucket_count() << std::endl;
    }
    
    std::cout << "[StaticAnalyzer] Starting traversal of " << ast.size() << " AST nodes" << std::endl;
    
    // Traverse AST to find all variable references
    for (size_t i = 0; i < ast.size(); i++) {
        std::cout << "[StaticAnalyzer] Traversing AST node " << (i + 1) << " of " << ast.size() << std::endl;
        traverse_ast_node_for_variables(ast[i].get());
    }
    
    std::cout << "[StaticAnalyzer] Variable reference resolution complete" << std::endl;
}

void StaticAnalyzer::perform_complete_variable_packing_from_scopes() {
    std::cout << "[StaticAnalyzer] Performing variable packing for all scopes..." << std::endl;
    
    // Pack variables in all discovered scopes
    for (auto& scope_entry : depth_to_scope_node_) {
        LexicalScopeNode* scope = scope_entry.second.get();
        if (scope && !scope->variable_declarations.empty()) {
            perform_optimal_packing_for_scope(scope);
        }
    }
    
    std::cout << "[StaticAnalyzer] Variable packing complete" << std::endl;
}

void StaticAnalyzer::compute_complete_function_analysis_from_scopes() {
    std::cout << "[StaticAnalyzer] Computing function analysis for all scopes..." << std::endl;
    
    // Analyze all function scopes  
    for (auto& scope_entry : depth_to_scope_node_) {
        LexicalScopeNode* scope = scope_entry.second.get();
        if (scope && scope->is_function_scope) {
            analyze_function_dependencies(scope);
        }
    }
    
    std::cout << "[StaticAnalyzer] Function analysis complete" << std::endl;
}

void StaticAnalyzer::traverse_ast_node_for_scopes(ASTNode* node) {
    if (!node) return;
    
    // Handle function declarations and expressions that create scopes
    if (auto* func_decl = dynamic_cast<FunctionDecl*>(node)) {
        // Enter function scope
        current_depth_++;
        auto function_scope = std::make_unique<LexicalScopeNode>(current_depth_, true);
        current_scope_ = function_scope.get();
        depth_to_scope_node_[current_depth_] = std::move(function_scope);
        
        // Record function declaration in parent scope
        if (depth_to_scope_node_[current_depth_ - 1]) {
            depth_to_scope_node_[current_depth_ - 1]->register_function_declaration(func_decl);
        }
        
        // Traverse function body
        for (const auto& stmt : func_decl->body) {
            traverse_ast_node_for_scopes(stmt.get());
        }
        
        // Exit function scope
        current_depth_--;
        current_scope_ = depth_to_scope_node_[current_depth_].get();
    }
    else if (auto* func_expr = dynamic_cast<FunctionExpression*>(node)) {
        // Similar to function declaration
        current_depth_++;
        auto function_scope = std::make_unique<LexicalScopeNode>(current_depth_, true);
        current_scope_ = function_scope.get();
        depth_to_scope_node_[current_depth_] = std::move(function_scope);
        
        // Record function expression in parent scope
        if (depth_to_scope_node_[current_depth_ - 1]) {
            depth_to_scope_node_[current_depth_ - 1]->register_function_expression(func_expr);
        }
        
        // Traverse function body
        for (const auto& stmt : func_expr->body) {
            traverse_ast_node_for_scopes(stmt.get());
        }
        
        // Exit function scope
        current_depth_--;
        current_scope_ = depth_to_scope_node_[current_depth_].get();
    }
    // TODO: Add more node types that create scopes (if/while blocks, etc.)
}

void StaticAnalyzer::traverse_ast_node_for_variables(ASTNode* node) {
    if (!node) {
        std::cout << "[StaticAnalyzer] WARNING: null node passed to traverse_ast_node_for_variables" << std::endl;
        return;
    }
    
    std::cout << "[StaticAnalyzer] Traversing node of type: " << typeid(*node).name() << std::endl;
    
    // Handle variable declarations and references
    if (auto* assignment = dynamic_cast<Assignment*>(node)) {
        std::cout << "[StaticAnalyzer] Processing assignment to variable: " << assignment->variable_name << std::endl;
        
        // This is a variable declaration/assignment
        if (current_scope_) {
            std::cout << "[StaticAnalyzer] Current scope is valid, depth=" << current_depth_ << std::endl;
            
            // Create variable declaration info
            VariableDeclarationInfo var_info(
                current_depth_, 
                (assignment->declaration_kind == Assignment::LET) ? "let" :
                (assignment->declaration_kind == Assignment::CONST) ? "const" : "var",
                assignment->declared_type
            );
            
            std::cout << "[StaticAnalyzer] Created declaration info for " << assignment->variable_name << std::endl;
            
            // Store in scope
            current_scope_->variable_declarations[assignment->variable_name] = var_info;
            
            // Populate AST node fields
            assignment->definition_scope = current_scope_;
            assignment->assignment_scope = current_scope_;
            assignment->variable_declaration_info = &current_scope_->variable_declarations[assignment->variable_name];
            // NOTE: definition_depth is now available via variable_declaration_info->depth
            
            std::cout << "[StaticAnalyzer] Found variable declaration: " << assignment->variable_name 
                      << " at depth " << current_depth_ << std::endl;
        } else {
            std::cout << "[StaticAnalyzer] ERROR: current_scope_ is null!" << std::endl;
            return;
        }
        
        // Traverse the assignment value
        if (assignment->value) {
            std::cout << "[StaticAnalyzer] Traversing assignment value" << std::endl;
            traverse_ast_node_for_variables(assignment->value.get());
        }
    }
    else if (auto* identifier = dynamic_cast<Identifier*>(node)) {
        // This is a variable reference - find its declaration
        LexicalScopeNode* definition_scope = find_variable_definition_scope(identifier->name);
        
        if (definition_scope) {
            // Populate AST node fields
            identifier->definition_scope = definition_scope;
            identifier->access_scope = current_scope_;
            identifier->definition_depth = definition_scope->scope_depth;
            identifier->access_depth = current_depth_;
            
            // Get the variable declaration info
            auto it = definition_scope->variable_declarations.find(identifier->name);
            if (it != definition_scope->variable_declarations.end()) {
                identifier->variable_declaration_info = &it->second;
            }
            
            std::cout << "[StaticAnalyzer] Found variable reference: " << identifier->name 
                      << " at depth " << current_depth_ << " (defined at depth " << definition_scope->scope_depth << ")" << std::endl;
        } else {
            std::cout << "[StaticAnalyzer] WARNING: Variable reference not found: " << identifier->name 
                      << " at depth " << current_depth_ << std::endl;
        }
    }
    
    // Recursively traverse child nodes for more node types
    if (auto* func_decl = dynamic_cast<FunctionDecl*>(node)) {
        // Enter function scope  
        current_depth_++;
        current_scope_ = depth_to_scope_node_[current_depth_].get();
        
        // Traverse function parameters (declare them in function scope)
        for (const auto& param : func_decl->parameters) {
            if (current_scope_) {
                // Create parameter declaration info
                VariableDeclarationInfo param_info(
                    current_depth_, "param", param.type
                );
                current_scope_->declare_variable(param.name, param_info);
                
                std::cout << "[StaticAnalyzer] Found function parameter: " << param.name 
                          << " at depth " << current_depth_ << std::endl;
            }
        }
        
        // Traverse function body
        for (const auto& stmt : func_decl->body) {
            traverse_ast_node_for_variables(stmt.get());
        }
        
        // Exit function scope
        current_depth_--;
        current_scope_ = depth_to_scope_node_[current_depth_].get();
    }
    else if (auto* func_expr = dynamic_cast<FunctionExpression*>(node)) {
        // Similar to function declaration
        current_depth_++;
        current_scope_ = depth_to_scope_node_[current_depth_].get();
        
        // Traverse function parameters
        for (const auto& param : func_expr->parameters) {
            if (current_scope_) {
                VariableDeclarationInfo param_info(
                    current_depth_, "param", param.type
                );
                current_scope_->declare_variable(param.name, param_info);
                
                std::cout << "[StaticAnalyzer] Found function expression parameter: " << param.name 
                          << " at depth " << current_depth_ << std::endl;
            }
        }
        
        // Traverse function body
        for (const auto& stmt : func_expr->body) {
            traverse_ast_node_for_variables(stmt.get());
        }
        
        // Exit function scope
        current_depth_--;
        current_scope_ = depth_to_scope_node_[current_depth_].get();
    }
    else if (auto* binary_op = dynamic_cast<BinaryOp*>(node)) {
        // Traverse operands
        if (binary_op->left) traverse_ast_node_for_variables(binary_op->left.get());
        if (binary_op->right) traverse_ast_node_for_variables(binary_op->right.get());
    }
    else if (auto* func_call = dynamic_cast<FunctionCall*>(node)) {
        // Traverse arguments only (function name is just a string)
        for (const auto& arg : func_call->arguments) {
            if (arg) traverse_ast_node_for_variables(arg.get());
        }
    }
    // TODO: Add more AST node types as needed
}

void StaticAnalyzer::perform_optimal_packing_for_scope(LexicalScopeNode* scope) {
    if (!scope) return;
    
    std::cout << "[StaticAnalyzer] Performing optimal packing for scope at depth " 
              << scope->scope_depth << " with " << scope->variable_declarations.size() 
              << " variables" << std::endl;
    
    // Clear existing packing
    scope->variable_offsets.clear();
    scope->packed_variable_order.clear();
    
    // Pack variables and update their declaration info with offsets
    size_t current_offset = 0;
    for (const auto& var_entry : scope->variable_declarations) {
        const std::string& var_name = var_entry.first;
        const VariableDeclarationInfo& var_info = var_entry.second;
        
        scope->variable_offsets[var_name] = current_offset;
        scope->packed_variable_order.push_back(var_name);
        
        // Update the VariableDeclarationInfo with offset and size info
        auto& mutable_var_info = const_cast<VariableDeclarationInfo&>(var_info);
        mutable_var_info.offset = current_offset;
        mutable_var_info.size_bytes = 8; // Default 64-bit size
        mutable_var_info.alignment = 8;
        
        current_offset += 8; // Default 64-bit size
        
        std::cout << "[StaticAnalyzer] Variable '" << var_name 
                  << "' assigned offset " << scope->variable_offsets[var_name] << std::endl;
    }
    
    scope->total_scope_frame_size = current_offset;
    std::cout << "[StaticAnalyzer] Optimal packing complete for scope depth " 
              << scope->scope_depth << ": " << current_offset << " bytes total" << std::endl;
}

void StaticAnalyzer::analyze_function_dependencies(LexicalScopeNode* scope) {
    if (!scope || !scope->is_function_scope) return;
    
    std::cout << "[StaticAnalyzer] Analyzing function dependencies for scope at depth " << scope->scope_depth << std::endl;
    
    // For now, basic analysis - can be expanded
    // Functions beyond global can have closures
    bool has_closures = (scope->scope_depth > 1);
    
    std::cout << "[StaticAnalyzer] Function scope analysis complete - closures: " 
              << (has_closures ? "yes" : "no") << std::endl;
}

// Public interface methods (for compatibility with code generator)
LexicalScopeNode* StaticAnalyzer::get_scope_node_for_depth(int depth) const {
    auto it = depth_to_scope_node_.find(depth);
    return (it != depth_to_scope_node_.end()) ? it->second.get() : nullptr;
}

LexicalScopeNode* StaticAnalyzer::get_definition_scope_for_variable(const std::string& name) const {
    // Search through all scopes for the variable definition
    for (const auto& scope_pair : depth_to_scope_node_) {
        LexicalScopeNode* scope = scope_pair.second.get();
        if (scope && scope->has_variable(name)) {
            return scope;
        }
    }
    return nullptr;
}

void StaticAnalyzer::perform_deferred_packing_for_scope(LexicalScopeNode* scope_node) {
    if (!scope_node) return;
    
    std::cout << "[StaticAnalyzer] Performing deferred packing for scope at depth " 
              << scope_node->scope_depth << std::endl;
    
    // Use the existing optimal packing method
    perform_optimal_packing_for_scope(scope_node);
    
    std::cout << "[StaticAnalyzer] Deferred packing completed: " 
              << scope_node->total_scope_frame_size << " bytes total" << std::endl;
}

// Stub implementations for helper methods
void StaticAnalyzer::enter_scope(LexicalScopeNode* scope) {
    current_scope_ = scope;
    if (scope) current_depth_ = scope->scope_depth;
}

void StaticAnalyzer::exit_scope() {
    if (current_depth_ > 1) {
        current_depth_--;
        current_scope_ = get_scope_node_for_depth(current_depth_);
    }
}

LexicalScopeNode* StaticAnalyzer::find_variable_definition_scope(const std::string& variable_name) {
    // Search from current scope up to global scope
    for (int depth = current_depth_; depth >= 1; depth--) {
        auto scope_it = depth_to_scope_node_.find(depth);
        if (scope_it != depth_to_scope_node_.end()) {
            LexicalScopeNode* scope = scope_it->second.get();
            if (scope->has_variable(variable_name)) {
                return scope;
            }
        }
    }
    return nullptr; // Variable not found
}

VariableDeclarationInfo* StaticAnalyzer::find_variable_declaration(const std::string& name, int access_depth) {
    // Look through the scope hierarchy starting from the access depth and going up
    for (int depth = access_depth; depth >= 1; depth--) {
        auto scope_it = depth_to_scope_node_.find(depth);
        if (scope_it == depth_to_scope_node_.end()) {
            continue;
        }
        
        LexicalScopeNode* scope = scope_it->second.get();
        if (!scope) {
            continue;
        }
        
        // Check if the variable is declared in this scope's variable_declarations
        auto var_it = scope->variable_declarations.find(name);
        if (var_it != scope->variable_declarations.end()) {
            return const_cast<VariableDeclarationInfo*>(&var_it->second);
        }
    }
    
    // Also check the all_variable_declarations_ map for backward compatibility
    auto it = all_variable_declarations_.find(name);
    if (it != all_variable_declarations_.end() && !it->second.empty()) {
        // Return the most recent declaration (last in vector)
        return it->second.back();
    }
    
    return nullptr; // Variable not found
}

int StaticAnalyzer::compute_access_depth_between_scopes(LexicalScopeNode* definition_scope, LexicalScopeNode* access_scope) {
    // Stub for now  
    return 0;
}

size_t StaticAnalyzer::get_datatype_size(DataType type) const {
    switch (type) {
        case DataType::INT32: return 4;
        case DataType::INT64: return 8;
        case DataType::FLOAT32: return 4;
        case DataType::FLOAT64: return 8;
        case DataType::BOOLEAN: return 1;
        case DataType::STRING: return 8; // Pointer to string
        case DataType::LOCAL_FUNCTION_INSTANCE: return 16; // Default function instance
        case DataType::DYNAMIC_VALUE: return 16; // DynamicValue struct
        default: return 8; // Default to pointer size
    }
}

size_t StaticAnalyzer::get_datatype_alignment(DataType type) const {
    switch (type) {
        case DataType::INT32: return 4;
        case DataType::FLOAT32: return 4;
        case DataType::BOOLEAN: return 1;
        default: return 8; // 8-byte alignment for most types
    }
}

void StaticAnalyzer::print_analysis_results() const {
    std::cout << "[StaticAnalyzer] =================================" << std::endl;
    std::cout << "[StaticAnalyzer] STATIC ANALYSIS RESULTS" << std::endl;
    std::cout << "[StaticAnalyzer] Total scopes discovered: " << depth_to_scope_node_.size() << std::endl;
    
    for (const auto& scope_pair : depth_to_scope_node_) {
        int depth = scope_pair.first;
        LexicalScopeNode* scope = scope_pair.second.get();
        
        std::cout << "[StaticAnalyzer] Scope " << depth 
                  << " (function=" << (scope->is_function_scope ? "yes" : "no") << "): "
                  << scope->variable_declarations.size() << " variables, "
                  << scope->total_scope_frame_size << " bytes" << std::endl;
                  
        for (const auto& var_entry : scope->variable_declarations) {
            const std::string& var_name = var_entry.first;
            auto offset_it = scope->variable_offsets.find(var_name);
            size_t offset = (offset_it != scope->variable_offsets.end()) ? offset_it->second : 0;
            std::cout << "  '" << var_name << "' @ offset " << offset << std::endl;
        }
    }
    
    std::cout << "[StaticAnalyzer] =================================" << std::endl;
}

VariableDeclarationInfo* StaticAnalyzer::get_variable_declaration_info(const std::string& name, int access_depth) {
    return find_variable_declaration(name, access_depth);
}
