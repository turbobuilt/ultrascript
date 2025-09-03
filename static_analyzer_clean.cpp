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

// Set the SimpleLexicalScopeAnalyzer from parser for integration
void StaticAnalyzer::set_parser_scope_analyzer(SimpleLexicalScopeAnalyzer* scope_analyzer) {
    parser_scope_analyzer_ = scope_analyzer;
    std::cout << "[StaticAnalyzer] Connected to parser's SimpleLexicalScopeAnalyzer" << std::endl;
}

// Helper method to find the lexical scope where a variable is defined
LexicalScopeNode* StaticAnalyzer::find_variable_definition_scope(const std::string& variable_name) {
    // Search from current scope upward through the scope hierarchy
    int search_depth = current_depth_;
    
    while (search_depth >= 1) {
        LexicalScopeNode* scope = get_scope_node_for_depth(search_depth);
        if (scope && scope->declared_variables.find(variable_name) != scope->declared_variables.end()) {
            return scope;
        }
        search_depth--;
    }
    
    return nullptr; // Variable not found
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
    
    // Traverse AST to find all variable references
    for (const auto& node : ast) {
        traverse_ast_node_for_variables(node.get());
    }
    
    std::cout << "[StaticAnalyzer] Variable reference resolution complete" << std::endl;
}

void StaticAnalyzer::perform_complete_variable_packing_from_scopes() {
    std::cout << "[StaticAnalyzer] Performing variable packing for all scopes..." << std::endl;
    
    // Pack variables in all discovered scopes
    for (auto& scope_entry : depth_to_scope_node_) {
        LexicalScopeNode* scope = scope_entry.second.get();
        if (scope && !scope->declared_variables.empty()) {
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
    if (!node) return;
    
    // Variable reference analysis - identify all variable accesses and their depths
    if (auto* identifier = dynamic_cast<Identifier*>(node)) {
        std::string var_name = identifier->name;
        std::cout << "[StaticAnalyzer] Resolving variable reference: " << var_name
                  << " at access depth " << current_depth_ << std::endl;
        
        // Find the definition scope for this variable
        LexicalScopeNode* def_scope = find_variable_definition_scope(var_name);
        if (def_scope) {
            int definition_depth = def_scope->scope_depth;
            std::cout << "[StaticAnalyzer] Variable '" << var_name 
                      << "' defined at depth " << definition_depth 
                      << ", accessed at depth " << current_depth_ << std::endl;
            
            // Record access pattern in the accessing scope
            if (current_scope_) {
                current_scope_->record_variable_access(var_name, definition_depth);
                
                // If accessing from outer scope, record as dependency
                if (definition_depth < current_depth_) {
                    // This is a closure access - record in self_dependencies
                    bool found = false;
                    for (auto& dep : current_scope_->self_dependencies) {
                        if (dep.variable_name == var_name && dep.definition_depth == definition_depth) {
                            dep.access_count++;
                            found = true;
                            break;
                        }
                    }
                    if (!found) {
                        ScopeDependency new_dep(var_name, definition_depth);
                        new_dep.access_count = 1;
                        current_scope_->self_dependencies.push_back(new_dep);
                        
                        std::cout << "[StaticAnalyzer] Added closure dependency: " << var_name 
                                  << " (def_depth=" << definition_depth << ") to scope " << current_depth_ << std::endl;
                    }
                }
            }
        } else {
            std::cout << "[StaticAnalyzer] WARNING: Variable '" << var_name 
                      << "' not found in any scope" << std::endl;
            unresolved_variables_.insert(var_name);
        }
    }
    
    // Variable declarations - record in current scope
    else if (auto* assignment = dynamic_cast<Assignment*>(node)) {
        if (assignment->declaration_kind != Assignment::VAR) { // let, const, var declarations
            std::string var_name = assignment->variable_name;
            if (current_scope_ && current_scope_->declared_variables.find(var_name) == current_scope_->declared_variables.end()) {
                // New variable declaration
                current_scope_->declared_variables.insert(var_name);
                std::cout << "[StaticAnalyzer] Declared variable '" << var_name 
                          << "' in scope depth " << current_depth_ << std::endl;
            }
        }
        
        // Process the assignment expression for variable references
        if (assignment->value) {
            traverse_ast_node_for_variables(assignment->value.get());
        }
    }
    
    // Function declarations - enter function scope for body analysis
    else if (auto* func_decl = dynamic_cast<FunctionDecl*>(node)) {
        int old_depth = current_depth_;
        LexicalScopeNode* old_scope = current_scope_;
        
        // Enter function scope
        current_depth_++;
        current_scope_ = get_scope_node_for_depth(current_depth_);
        
        // Traverse function body
        for (const auto& stmt : func_decl->body) {
            traverse_ast_node_for_variables(stmt.get());
        }
        
        // Exit function scope
        current_depth_ = old_depth;
        current_scope_ = old_scope;
    }
    
    // Function expressions - similar to function declarations
    else if (auto* func_expr = dynamic_cast<FunctionExpression*>(node)) {
        int old_depth = current_depth_;
        LexicalScopeNode* old_scope = current_scope_;
        
        // Enter function scope
        current_depth_++;
        current_scope_ = get_scope_node_for_depth(current_depth_);
        
        // Traverse function body
        for (const auto& stmt : func_expr->body) {
            traverse_ast_node_for_variables(stmt.get());
        }
        
        // Exit function scope
        current_depth_ = old_depth;
        current_scope_ = old_scope;
    }
    
    // Binary operations - traverse both operands
    else if (auto* binop = dynamic_cast<BinaryOp*>(node)) {
        traverse_ast_node_for_variables(binop->left.get());
        traverse_ast_node_for_variables(binop->right.get());
    }
    
    // Function calls - traverse function name and arguments
    else if (auto* func_call = dynamic_cast<FunctionCall*>(node)) {
        // Function name might be a variable reference
        if (!func_call->name.empty()) {
            // Create a temporary identifier to process function name as variable reference
            Identifier temp_id(func_call->name);
            traverse_ast_node_for_variables(&temp_id);
        }
        
        // Process arguments
        for (const auto& arg : func_call->arguments) {
            traverse_ast_node_for_variables(arg.get());
        }
    }
    
    // Control flow statements
    else if (auto* if_stmt = dynamic_cast<IfStatement*>(node)) {
        traverse_ast_node_for_variables(if_stmt->condition.get());
        for (const auto& stmt : if_stmt->then_body) {
            traverse_ast_node_for_variables(stmt.get());
        }
        for (const auto& stmt : if_stmt->else_body) {
            traverse_ast_node_for_variables(stmt.get());
        }
    }
    
    // Return statements
    else if (auto* ret_stmt = dynamic_cast<ReturnStatement*>(node)) {
        if (ret_stmt->value) {
            traverse_ast_node_for_variables(ret_stmt->value.get());
        }
    }
    
    // Property access
    else if (auto* prop_access = dynamic_cast<PropertyAccess*>(node)) {
        // Object name might be a variable reference
        Identifier temp_id(prop_access->object_name);
        traverse_ast_node_for_variables(&temp_id);
    }
    
    // TODO: Add more node types as needed (loops, switch statements, etc.)
}

void StaticAnalyzer::perform_optimal_packing_for_scope(LexicalScopeNode* scope) {
    if (!scope) return;
    
    std::cout << "[StaticAnalyzer] Performing optimal packing for scope at depth " 
              << scope->scope_depth << " with " << scope->declared_variables.size() 
              << " variables" << std::endl;
    
    // Clear existing packing
    scope->variable_offsets.clear();
    scope->packed_variable_order.clear();
    
    // Simple linear packing - declared_variables is a set<string>
    size_t current_offset = 0;
    for (const std::string& var_name : scope->declared_variables) {
        scope->variable_offsets[var_name] = current_offset;
        scope->packed_variable_order.push_back(var_name);
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
    
    // Step 1: Collect all scope dependencies for this function AND all its descendants
    std::unordered_map<int, size_t> scope_access_counts; // depth -> total access count
    
    // Process self dependencies (variables this function accesses from outer scopes)
    for (const auto& dep : scope->self_dependencies) {
        scope_access_counts[dep.definition_depth] += dep.access_count;
        std::cout << "[StaticAnalyzer] Self dependency: depth " << dep.definition_depth 
                  << " (" << dep.variable_name << ") accessed " << dep.access_count << " times" << std::endl;
    }
    
    // Step 2: Recursively collect dependencies from all descendant scopes
    collect_descendant_dependencies(scope, scope_access_counts);
    
    // Step 3: Sort scopes by access frequency (most frequently accessed first)
    std::vector<std::pair<int, size_t>> sorted_deps; // (depth, access_count)
    for (const auto& entry : scope_access_counts) {
        if (entry.first != scope->scope_depth) { // Don't include self
            sorted_deps.push_back(entry);
        }
    }
    
    // Sort by access count in descending order (most frequent first)
    std::sort(sorted_deps.begin(), sorted_deps.end(), 
              [](const auto& a, const auto& b) { return a.second > b.second; });
    
    // Step 4: Extract the priority-sorted parent scopes
    scope->priority_sorted_parent_scopes.clear();
    for (const auto& dep : sorted_deps) {
        scope->priority_sorted_parent_scopes.push_back(dep.first);
        std::cout << "[StaticAnalyzer] Priority scope depth " << dep.first 
                  << " (total accesses: " << dep.second << ")" << std::endl;
    }
    
    // Step 5: Update any corresponding FunctionDecl with static analysis data
    for (FunctionDecl* func_decl : scope->declared_functions) {
        if (func_decl && func_decl->lexical_scope.get() == scope) {
            func_decl->static_analysis.needed_parent_scopes = scope->priority_sorted_parent_scopes;
            func_decl->static_analysis.local_scope_size = scope->total_scope_frame_size;
            
            std::cout << "[StaticAnalyzer] Updated FunctionDecl '" << func_decl->name 
                      << "' with " << scope->priority_sorted_parent_scopes.size() 
                      << " parent scope dependencies" << std::endl;
        }
    }
    
    std::cout << "[StaticAnalyzer] Function dependency analysis complete for scope depth " 
              << scope->scope_depth << " - needs " << scope->priority_sorted_parent_scopes.size() 
              << " parent scopes" << std::endl;
}

void StaticAnalyzer::collect_descendant_dependencies(LexicalScopeNode* scope, std::unordered_map<int, size_t>& scope_access_counts) {
    if (!scope) return;
    
    // Find all descendant scopes (scopes with depth > current scope's depth)
    for (const auto& scope_pair : depth_to_scope_node_) {
        LexicalScopeNode* descendant = scope_pair.second.get();
        if (descendant && descendant->scope_depth > scope->scope_depth) {
            // Check if this descendant is actually within our scope's hierarchy
            // For now, we assume all deeper scopes are descendants (can be refined later)
            
            // Add descendant's dependencies to our count
            for (const auto& dep : descendant->self_dependencies) {
                scope_access_counts[dep.definition_depth] += dep.access_count;
                std::cout << "[StaticAnalyzer] Descendant dependency from depth " 
                          << descendant->scope_depth << ": depth " << dep.definition_depth 
                          << " (" << dep.variable_name << ") accessed " << dep.access_count << " times" << std::endl;
            }
        }
    }
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
        if (scope && scope->declared_variables.find(name) != scope->declared_variables.end()) {
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
                  << scope->declared_variables.size() << " variables, "
                  << scope->total_scope_frame_size << " bytes" << std::endl;
                  
        for (const std::string& var_name : scope->declared_variables) {
            auto offset_it = scope->variable_offsets.find(var_name);
            size_t offset = (offset_it != scope->variable_offsets.end()) ? offset_it->second : 0;
            std::cout << "  '" << var_name << "' @ offset " << offset << std::endl;
        }
    }
    
    std::cout << "[StaticAnalyzer] =================================" << std::endl;
}

VariableDeclarationInfo* StaticAnalyzer::get_variable_declaration_info(const std::string& name, int access_depth) {
    // Find the variable in the appropriate scope
    LexicalScopeNode* def_scope = find_variable_definition_scope(name);
    if (def_scope) {
        // Try the full variable_declarations map first
        auto var_it = def_scope->variable_declarations.find(name);
        if (var_it != def_scope->variable_declarations.end()) {
            return &var_it->second;
        }
        
        // If not found in variable_declarations but exists in declared_variables,
        // create a default VariableDeclarationInfo and add it
        if (def_scope->declared_variables.find(name) != def_scope->declared_variables.end()) {
            VariableDeclarationInfo default_info;
            default_info.data_type = DataType::ANY;
            default_info.declaration_type = "var";
            def_scope->variable_declarations[name] = default_info;
            return &def_scope->variable_declarations[name];
        }
    }
    return nullptr;
}
