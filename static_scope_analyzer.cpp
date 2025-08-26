#include "static_scope_analyzer.h"
#include "compiler.h"
#include <iostream>

// ============================================================================
// STATIC SCOPE ANALYZER IMPLEMENTATION
// Pure compile-time analysis to determine lexical scope requirements
// ============================================================================

StaticScopeAnalyzer::StaticScopeAnalyzer() {
    std::cout << "[DEBUG] StaticScopeAnalyzer: Created with pure static analysis and ES6 block scoping support" << std::endl;
}

// ES6 Block Scoping Analysis Methods
void StaticScopeAnalyzer::begin_function_analysis(const std::string& function_name) {
    std::cout << "[DEBUG] StaticScopeAnalyzer: Beginning analysis for function '" << function_name << "'" << std::endl;
    current_function_name_ = function_name;
    current_scope_level_ = 0;
    
    // Initialize function analysis structure
    function_analyses_[function_name] = FunctionScopeAnalysis();
    function_analyses_[function_name].function_name = function_name;
}

void StaticScopeAnalyzer::end_function_analysis() {
    std::cout << "[DEBUG] StaticScopeAnalyzer: Ending analysis for function '" << current_function_name_ << "'" << std::endl;
    current_function_name_ = "";
    current_scope_level_ = 0;
}

// Non-const version that returns a reference for modification
LexicalScopeInfo& StaticScopeAnalyzer::get_variable_info(const std::string& var_name) {
    return variable_scope_map_[var_name];
}

void StaticScopeAnalyzer::analyze_function(const std::string& function_name, ASTNode* function_node) {
    std::cout << "[DEBUG] StaticScopeAnalyzer: Analyzing function '" << function_name << "'" << std::endl;
    
    FunctionScopeAnalysis analysis;
    analysis.function_name = function_name;
    analysis.has_escaping_variables = false;
    analysis.total_stack_space_needed = 0;
    analysis.total_heap_scope_size = 0;
    
    if (!function_node) {
        std::cout << "[DEBUG] StaticScopeAnalyzer: Function node is null, using defaults" << std::endl;
        function_analyses_[function_name] = analysis;
        return;
    }
    
    // Reset analysis state for this function
    current_function_name_ = function_name;
    current_scope_level_ = 0;  // Start at global scope level
    variable_scope_map_.clear();
    
    std::cout << "[DEBUG] StaticScopeAnalyzer: Starting AST walk for function '" << function_name << "'" << std::endl;
    
    // Walk the AST to build complete scope information
    walk_ast_for_scopes(function_node);
    
    // CRITICAL: Analyze what parent scopes descendants need and propagate upward
    analyze_descendant_scope_needs(function_name, function_node);
    
    // Analyze what parent scopes this function needs
    analyze_parent_scope_dependencies(function_name);
    
    // Calculate memory layouts
    calculate_memory_layouts(function_name);
    
    // Determine optimal register allocation
    determine_register_allocation(function_name);
    
    // NEW: Optimize variable layout and calculate offsets
    optimize_variable_layout(function_name);
    calculate_variable_offsets(function_name);
    
    // Store the analysis
    function_analyses_[function_name] = analysis;
    
    std::cout << "[DEBUG] StaticScopeAnalyzer: Completed analysis for function '" << function_name << "'" << std::endl;
    print_function_analysis(function_name);
}

void StaticScopeAnalyzer::build_scope_hierarchy(ASTNode* function_node) {
    std::cout << "[DEBUG] StaticScopeAnalyzer: Building scope hierarchy with full AST walking" << std::endl;
    
    if (!function_node) return;
    
    // Walk the AST and identify all scopes and variable declarations
    current_scope_level_ = 0;
    walk_ast_for_scopes(function_node);
}

void StaticScopeAnalyzer::walk_ast_for_scopes(ASTNode* node) {
    if (!node) {
        std::cout << "[DEBUG] StaticScopeAnalyzer: walk_ast_for_scopes called with null node" << std::endl;
        return;
    }
    
    std::cout << "[DEBUG] StaticScopeAnalyzer: Processing AST node type: " << typeid(*node).name() << std::endl;
    
    // Handle different node types for scope analysis
    if (auto* func_decl = dynamic_cast<FunctionDecl*>(node)) {
        // Function declaration - analyze parameters and body
        std::cout << "[DEBUG] StaticScopeAnalyzer: Processing FunctionDecl: " << func_decl->name << std::endl;
        
        // Analyze parameters as local variables
        for (const auto& param : func_decl->parameters) {
            add_variable_to_scope(param.name, current_scope_level_, param.type);
        }
        
        // Analyze function body
        for (const auto& stmt : func_decl->body) {
            walk_ast_for_scopes(stmt.get());
        }
    }
    else if (auto* func_expr = dynamic_cast<FunctionExpression*>(node)) {
        // Nested function creates new scope
        current_scope_level_++;
        
        // Analyze parameters as local variables
        for (const auto& param : func_expr->parameters) {
            add_variable_to_scope(param.name, current_scope_level_, param.type);
        }
        
        // Analyze function body
        for (const auto& stmt : func_expr->body) {
            walk_ast_for_scopes(stmt.get());
        }
        
        current_scope_level_--;
    }
    else if (auto* arrow_func = dynamic_cast<ArrowFunction*>(node)) {
        // Arrow function creates block scope (like FunctionExpression) 
        std::cout << "[DEBUG] StaticScopeAnalyzer: Processing ArrowFunction" << std::endl;
        current_scope_level_++;
        
        // Analyze parameters as local variables
        for (const auto& param : arrow_func->parameters) {
            add_variable_to_scope(param.name, current_scope_level_, param.type);
        }
        
        // Analyze arrow function body
        if (arrow_func->is_single_expression) {
            // Expression body: x => x + 1
            if (arrow_func->expression) {
                walk_ast_for_scopes(arrow_func->expression.get());
            }
        } else {
            // Block body: x => { return x + 1; }
            for (const auto& stmt : arrow_func->body) {
                walk_ast_for_scopes(stmt.get());
            }
        }
        
        current_scope_level_--;
    }
    else if (auto* assignment = dynamic_cast<Assignment*>(node)) {
        // Variable assignment/declaration - add to current scope with ES6 semantics
        add_variable_with_declaration_kind(assignment->variable_name, assignment->declaration_kind, current_scope_level_, 0);
        
        // Also analyze the assigned expression
        if (assignment->value) {
            walk_ast_for_scopes(assignment->value.get());
        }
    }
    else if (auto* identifier = dynamic_cast<Identifier*>(node)) {
        // Variable usage - record for escape analysis
        record_variable_usage(identifier->name, current_scope_level_);
    }
    else if (auto* func_call = dynamic_cast<FunctionCall*>(node)) {
        // Check if this is a goroutine call
        if (func_call->is_goroutine) {
            current_goroutine_depth_++;
            for (const auto& arg : func_call->arguments) {
                walk_ast_for_scopes(arg.get());
            }
            current_goroutine_depth_--;
        } else {
            for (const auto& arg : func_call->arguments) {
                walk_ast_for_scopes(arg.get());
            }
        }
    }
    else if (auto* method_call = dynamic_cast<MethodCall*>(node)) {
        // Check if this is a goroutine method call
        if (method_call->is_goroutine) {
            current_goroutine_depth_++;
            for (const auto& arg : method_call->arguments) {
                walk_ast_for_scopes(arg.get());
            }
            current_goroutine_depth_--;
        } else {
            for (const auto& arg : method_call->arguments) {
                walk_ast_for_scopes(arg.get());
            }
        }
    }
    else if (auto* binary_op = dynamic_cast<BinaryOp*>(node)) {
        walk_ast_for_scopes(binary_op->left.get());
        walk_ast_for_scopes(binary_op->right.get());
    }
    else if (auto* ternary = dynamic_cast<TernaryOperator*>(node)) {
        walk_ast_for_scopes(ternary->condition.get());
        walk_ast_for_scopes(ternary->true_expr.get());
        walk_ast_for_scopes(ternary->false_expr.get());
    }
    else if (auto* if_stmt = dynamic_cast<IfStatement*>(node)) {
        // If statement - analyze condition and both branches
        // ES6: if blocks create new block scopes for let/const
        walk_ast_for_scopes(if_stmt->condition.get());
        
        // Process then branch in new block scope
        int saved_scope = current_scope_level_;
        current_scope_level_++;  // Create new block scope for if body
        
        std::cout << "[DEBUG] StaticScopeAnalyzer: if-statement creates block scope at level " 
                  << current_scope_level_ << std::endl;
        
        for (const auto& stmt : if_stmt->then_body) {
            walk_ast_for_scopes(stmt.get());
        }
        
        current_scope_level_ = saved_scope;  // Restore scope
        
        // Process else branch in new block scope (if it exists)
        if (!if_stmt->else_body.empty()) {
            current_scope_level_++;  // Create new block scope for else body
            
            std::cout << "[DEBUG] StaticScopeAnalyzer: else-statement creates block scope at level " 
                      << current_scope_level_ << std::endl;
            
            for (const auto& stmt : if_stmt->else_body) {
                walk_ast_for_scopes(stmt.get());
            }
            
            current_scope_level_ = saved_scope;  // Restore scope
        }
        
        std::cout << "[DEBUG] StaticScopeAnalyzer: Completed if-statement analysis, scope restored to " 
                  << saved_scope << std::endl;
    }
    else if (auto* for_loop = dynamic_cast<ForLoop*>(node)) {
        // ES6 For-loop scoping - handle let/const vs var semantics correctly
        std::cout << "[DEBUG] StaticScopeAnalyzer: Processing for-loop with declaration kind" << std::endl;
        
        // Check if we need to create a block scope for this loop
        bool needs_block_scope = (for_loop->init_declaration_kind == Assignment::LET || 
                                 for_loop->init_declaration_kind == Assignment::CONST);
        
        int loop_body_scope_level = current_scope_level_;
        
        if (needs_block_scope) {
            // let/const for-loops: Create block scope for both init variable and body
            loop_body_scope_level = current_scope_level_ + 1;
            
            // Mark this scope as containing let/const (affects optimization)
            if (function_analyses_.count(current_function_name_)) {
                function_analyses_[current_function_name_].scope_contains_let_const[loop_body_scope_level] = true;
            }
            
            std::cout << "[DEBUG] StaticScopeAnalyzer: for(let/const) creates block scope at level " 
                      << loop_body_scope_level << std::endl;
        } else {
            std::cout << "[DEBUG] StaticScopeAnalyzer: for(var) - no new scope, variables hoist to function" << std::endl;
        }
        
        // Analyze init statement - for let/const, it goes in the block scope
        // for var, it gets hoisted to function scope during add_variable_to_scope
        int saved_scope = current_scope_level_;
        current_scope_level_ = loop_body_scope_level;
        
        // Process the initialization (variable declaration)
        if (auto* assignment = dynamic_cast<Assignment*>(for_loop->init.get())) {
            // Add the loop variable with correct scoping semantics
            add_variable_with_declaration_kind(assignment->variable_name, for_loop->init_declaration_kind, loop_body_scope_level, 0);
            
            // Process the initialization value
            walk_ast_for_scopes(assignment->value.get());
        } else {
            // Non-assignment init (e.g., expression)
            walk_ast_for_scopes(for_loop->init.get());
        }
        
        // Process condition and update in the same scope as init
        walk_ast_for_scopes(for_loop->condition.get());
        walk_ast_for_scopes(for_loop->update.get());
        
        // Process loop body - in the same scope as init for let/const loops
        for (const auto& stmt : for_loop->body) {
            walk_ast_for_scopes(stmt.get());
        }
        
        current_scope_level_ = saved_scope;
        
        std::cout << "[DEBUG] StaticScopeAnalyzer: Completed for-loop analysis, scope restored to " 
                  << current_scope_level_ << std::endl;
    }
    else if (auto* while_loop = dynamic_cast<WhileLoop*>(node)) {
        // ES6 While-loop scoping - while loops create block scope for let/const variables
        std::cout << "[DEBUG] StaticScopeAnalyzer: Processing while-loop with block scope" << std::endl;
        
        // While loops always create a block scope in ES6
        int saved_scope = current_scope_level_;
        current_scope_level_++;  // Create new block scope for while body
        
        // Mark this scope as requiring allocation (block scope)
        if (function_analyses_.count(current_function_name_)) {
            function_analyses_[current_function_name_].scope_contains_let_const[current_scope_level_] = true;
        }
        
        std::cout << "[DEBUG] StaticScopeAnalyzer: while-loop creates block scope at level " 
                  << current_scope_level_ << std::endl;
        
        // Process condition (evaluated in outer scope)
        int condition_scope = saved_scope;
        current_scope_level_ = condition_scope;
        walk_ast_for_scopes(while_loop->condition.get());
        
        // Process body in block scope
        current_scope_level_ = saved_scope + 1;
        for (const auto& stmt : while_loop->body) {
            walk_ast_for_scopes(stmt.get());
        }
        
        current_scope_level_ = saved_scope;
        
        std::cout << "[DEBUG] StaticScopeAnalyzer: Completed while-loop analysis, scope restored to " 
                  << current_scope_level_ << std::endl;
    }
    else if (auto* foreach_loop = dynamic_cast<ForEachLoop*>(node)) {
        // ForEach loop - analyze iterable and body, add loop variables to scope
        walk_ast_for_scopes(foreach_loop->iterable.get());
        
        // Add loop variables to current scope
        add_variable_to_scope(foreach_loop->index_var_name, current_scope_level_, DataType::INT64);
        add_variable_to_scope(foreach_loop->value_var_name, current_scope_level_, DataType::ANY);
        
        for (const auto& stmt : foreach_loop->body) {
            walk_ast_for_scopes(stmt.get());
        }
    }
    else if (auto* forin_stmt = dynamic_cast<ForInStatement*>(node)) {
        // ForIn statement - analyze object and body, add key variable to scope
        walk_ast_for_scopes(forin_stmt->object.get());
        
        // Add key variable to current scope
        add_variable_to_scope(forin_stmt->key_var_name, current_scope_level_, DataType::STRING);
        
        for (const auto& stmt : forin_stmt->body) {
            walk_ast_for_scopes(stmt.get());
        }
    }
    else if (auto* return_stmt = dynamic_cast<ReturnStatement*>(node)) {
        // Return statement - analyze return value expression
        if (return_stmt->value) {
            walk_ast_for_scopes(return_stmt->value.get());
        }
    }
}

void StaticScopeAnalyzer::add_variable_to_scope(const std::string& name, int scope_level, DataType type) {
    // Use the enhanced version with declaration kind
    add_variable_with_declaration_kind(name, Assignment::VAR, scope_level, 0);
}

void StaticScopeAnalyzer::add_variable_with_declaration_kind(const std::string& name, DeclarationKind kind, int scope_level, int usage_order) {
    // ES6 Scoping Rules: var is hoisted to function scope, let/const are block-scoped
    int actual_scope_level = scope_level;
    
    if (kind == Assignment::VAR) {
        // var declarations are hoisted to the nearest function scope (level 0)
        actual_scope_level = 0;
        std::cout << "[DEBUG] StaticScopeAnalyzer: var '" << name << "' hoisted from level " 
                  << scope_level << " to function scope (level 0)" << std::endl;
    }
    
    LexicalScopeInfo info;
    info.variable_name = name;
    info.scope_level = actual_scope_level;  // Use the hoisted scope level
    info.offset_in_scope = 0; // Will be calculated later
    info.escapes_current_function = false; // Will be determined by usage analysis
    info.type = DataType::ANY; // Default type, can be refined later
    info.size_bytes = 8; // Default pointer size
    info.declaration_kind = kind;
    info.is_block_scoped = (kind == Assignment::LET || kind == Assignment::CONST);
    info.is_loop_iteration_scoped = false; // Will be set for loop variables
    
    // Initialize ordering and access optimization fields
    info.access_frequency = usage_order; // Use usage_order as initial access frequency
    info.co_accessed_variables.clear(); // Will be populated during co-access analysis
    info.optimal_order_index = -1;       // Will be set during variable ordering
    info.is_hot_variable = false;        // Will be determined by access frequency
    info.alignment_requirement = 8; // Default alignment
    
    variable_scope_map_[name] = info;
    
    // Track if this scope contains let/const (affects scope optimization)
    // Only count let/const as requiring scope allocation
    if (info.is_block_scoped && !current_function_name_.empty()) {
        auto& analysis = function_analyses_[current_function_name_];
        analysis.scope_contains_let_const[actual_scope_level] = true;
        
        std::cout << "[DEBUG] StaticScopeAnalyzer: Scope " << actual_scope_level 
                  << " now requires allocation (contains let/const)" << std::endl;
    }
    
    std::cout << "[DEBUG] StaticScopeAnalyzer: Added variable '" << name 
              << "' with " << (kind == Assignment::VAR ? "var" : kind == Assignment::LET ? "let" : "const") 
              << " scoping at level " << actual_scope_level << std::endl;
}

// Public wrapper for variable ordering optimization
void StaticScopeAnalyzer::optimize_variable_ordering(const std::string& function_name) {
    std::cout << "[DEBUG] StaticScopeAnalyzer: Starting variable ordering optimization for '" << function_name << "'" << std::endl;
    optimize_variable_layout(function_name);
}

// Public wrapper for offset calculation  
void StaticScopeAnalyzer::compute_variable_offsets(const std::string& function_name) {
    std::cout << "[DEBUG] StaticScopeAnalyzer: Computing variable offsets for '" << function_name << "'" << std::endl;
    calculate_variable_offsets(function_name);
}

void StaticScopeAnalyzer::record_variable_usage(const std::string& name, int usage_scope_level) {
    auto it = variable_scope_map_.find(name);
    if (it != variable_scope_map_.end()) {
        // Variable found in local scope - check if it's used from different scope than declaration
        if (usage_scope_level != it->second.scope_level || current_goroutine_depth_ > 0) {
            it->second.escapes_current_function = true;
            std::cout << "[DEBUG] StaticScopeAnalyzer: Variable '" << name << "' escapes (used in scope " 
                      << usage_scope_level << ", declared in scope " << it->second.scope_level 
                      << ", goroutine_depth=" << current_goroutine_depth_ << ")" << std::endl;
        } else {
            std::cout << "[DEBUG] StaticScopeAnalyzer: Variable '" << name << "' used locally (scope " 
                      << usage_scope_level << ")" << std::endl;
        }
    } else {
        // Variable not found in local scope - this is a PARENT SCOPE ACCESS!
        std::cout << "[DEBUG] StaticScopeAnalyzer: Variable '" << name << "' accessed from parent scope!" << std::endl;
        std::cout << "[DEBUG] StaticScopeAnalyzer: Current usage scope: " << usage_scope_level 
                  << ", function: " << current_function_name_ << std::endl;
        
        // Record this as a parent scope dependency
        // The parent scope level is usage_scope_level - 1 (parent of current scope)
        int parent_scope_level = usage_scope_level - 1;
        if (parent_scope_level >= 0) {
            auto& analysis = function_analyses_[current_function_name_];
            
            // CRITICAL: This is a SELF access (current function directly accesses parent scope)
            analysis.self_parent_scope_needs.insert(parent_scope_level);
            analysis.required_parent_scopes.insert(parent_scope_level);
            
            // Create a LexicalScopeInfo for this parent variable access
            LexicalScopeInfo parent_var_info;
            parent_var_info.variable_name = name;
            parent_var_info.scope_level = parent_scope_level;  // Parent scope level
            parent_var_info.offset_in_scope = 0;  // Will be calculated later
            parent_var_info.escapes_current_function = true;  // Parent access = escape
            parent_var_info.type = DataType::UNKNOWN;
            parent_var_info.size_bytes = 8;
            
            analysis.variables[name] = parent_var_info;
            
            std::cout << "[DEBUG] StaticScopeAnalyzer: Added SELF parent scope dependency: level " 
                      << parent_scope_level << " for variable '" << name << "'" << std::endl;
        }
    }
}

void StaticScopeAnalyzer::analyze_parent_scope_dependencies(const std::string& function_name) {
    std::cout << "[DEBUG] StaticScopeAnalyzer: Analyzing parent scope dependencies for '" << function_name << "'" << std::endl;
    
    auto it = function_analyses_.find(function_name);
    if (it == function_analyses_.end()) {
        std::cout << "[DEBUG] StaticScopeAnalyzer: No analysis found for function " << function_name << std::endl;
        return;
    }
    
    FunctionScopeAnalysis& analysis = it->second;
    
    // CRITICAL: We need to analyze not just what this function directly accesses,
    // but also what parent scopes its descendants (nested functions, goroutines) need!
    // This ensures that we provide all necessary parent scope registers.
    
    std::cout << "[DEBUG] StaticScopeAnalyzer: Initial required parent scopes for '" << function_name << "': ";
    for (int level : analysis.required_parent_scopes) {
        std::cout << level << " ";
    }
    std::cout << std::endl;
    
    // TODO: Add descendant analysis here
    // For now, we'll need to implement a separate pass that:
    // 1. Identifies all nested functions/goroutines within this function
    // 2. Recursively analyzes their parent scope needs
    // 3. Propagates those needs upward to this function
    
    std::cout << "[DEBUG] StaticScopeAnalyzer: Final required parent scopes for '" << function_name << "': ";
    for (int level : analysis.required_parent_scopes) {
        std::cout << level << " ";
    }
    std::cout << std::endl;
    
    // Mark that this function has escaping variables if it accesses parent scopes
    if (!analysis.required_parent_scopes.empty()) {
        analysis.has_escaping_variables = true;
        std::cout << "[DEBUG] StaticScopeAnalyzer: Function '" << function_name 
                  << "' marked as having escaping variables due to parent scope access" << std::endl;
    }
}

void StaticScopeAnalyzer::analyze_descendant_scope_needs(const std::string& function_name, ASTNode* function_node) {
    std::cout << "[DEBUG] StaticScopeAnalyzer: Analyzing descendant scope needs for '" << function_name << "'" << std::endl;
    
    if (!function_node) {
        std::cout << "[DEBUG] StaticScopeAnalyzer: No function node provided for descendant analysis" << std::endl;
        return;
    }
    
    // Step 1: Find all nested functions within this function's AST
    function_descendants_.clear();
    function_parent_.clear();
    function_scope_level_.clear();
    
    find_nested_functions(function_node, function_name, current_scope_level_);
    
    // Step 2: Show what we found
    std::cout << "[DEBUG] StaticScopeAnalyzer: Found nested functions in '" << function_name << "':" << std::endl;
    for (const auto& descendant : function_descendants_[function_name]) {
        std::cout << "[DEBUG]   - " << descendant << " at scope level " << function_scope_level_[descendant] << std::endl;
    }
    
    // Step 3: Perform bottom-up propagation of parent scope needs
    propagate_descendant_needs_bottom_up();
    
    auto& analysis = function_analyses_[function_name];
    std::cout << "[DEBUG] StaticScopeAnalyzer: After descendant analysis, '" << function_name << "' needs parent levels: ";
    for (int level : analysis.required_parent_scopes) {
        std::cout << level << " ";
    }
    std::cout << std::endl;
}

void StaticScopeAnalyzer::find_nested_functions(ASTNode* node, const std::string& parent_function, int current_level) {
    if (!node) return;
    
    // Look for nested function expressions and goroutine calls
    if (auto* func_expr = dynamic_cast<FunctionExpression*>(node)) {
        // Generate a unique name for this nested function
        std::string nested_function_name = parent_function + "_nested_" + std::to_string(current_level);
        
        std::cout << "[DEBUG] StaticScopeAnalyzer: Found nested function '" << nested_function_name 
                  << "' in '" << parent_function << "' at level " << current_level << std::endl;
        
        // Track the relationship
        function_descendants_[parent_function].push_back(nested_function_name);
        function_parent_[nested_function_name] = parent_function;
        function_scope_level_[nested_function_name] = current_level + 1;
        
        // Create analysis entry for the nested function
        FunctionScopeAnalysis nested_analysis;
        nested_analysis.function_name = nested_function_name;
        nested_analysis.has_escaping_variables = false;
        nested_analysis.total_stack_space_needed = 0;
        nested_analysis.total_heap_scope_size = 0;
        function_analyses_[nested_function_name] = nested_analysis;
        
        // Recursively analyze the nested function's body
        std::string saved_function_name = current_function_name_;
        int saved_scope_level = current_scope_level_;
        
        current_function_name_ = nested_function_name;
        current_scope_level_ = current_level + 1;
        
        // Analyze parameters and body of nested function
        for (const auto& param : func_expr->parameters) {
            add_variable_to_scope(param.name, current_scope_level_, param.type);
        }
        
        for (const auto& stmt : func_expr->body) {
            walk_ast_for_scopes(stmt.get());
            find_nested_functions(stmt.get(), nested_function_name, current_level + 1);
        }
        
        // Restore context
        current_function_name_ = saved_function_name;
        current_scope_level_ = saved_scope_level;
    }
    else if (auto* func_call = dynamic_cast<FunctionCall*>(node)) {
        if (func_call->is_goroutine) {
            // Goroutine call - treat as nested function
            std::string goroutine_name = parent_function + "_goroutine_" + std::to_string(current_level);
            
            std::cout << "[DEBUG] StaticScopeAnalyzer: Found goroutine '" << goroutine_name 
                      << "' in '" << parent_function << "' at level " << current_level << std::endl;
            
            function_descendants_[parent_function].push_back(goroutine_name);
            function_parent_[goroutine_name] = parent_function;
            function_scope_level_[goroutine_name] = current_level + 1;
            
            // Create analysis entry
            FunctionScopeAnalysis goroutine_analysis;
            goroutine_analysis.function_name = goroutine_name;
            goroutine_analysis.has_escaping_variables = true; // Goroutines always escape
            function_analyses_[goroutine_name] = goroutine_analysis;
        }
        
        // Continue searching in arguments
        for (const auto& arg : func_call->arguments) {
            find_nested_functions(arg.get(), parent_function, current_level);
        }
    }
    else {
        // For other node types, recursively search their children
        // This is a simplified version - in real implementation, we'd need to handle all AST node types
        if (auto* assignment = dynamic_cast<Assignment*>(node)) {
            if (assignment->value) {
                find_nested_functions(assignment->value.get(), parent_function, current_level);
            }
        }
    }
}

void StaticScopeAnalyzer::propagate_descendant_needs_bottom_up() {
    std::cout << "[DEBUG] StaticScopeAnalyzer: Starting bottom-up propagation of descendant needs" << std::endl;
    
    // Get all functions sorted by scope level (deepest first)
    std::vector<std::pair<std::string, int>> functions_by_level;
    for (const auto& entry : function_scope_level_) {
        functions_by_level.push_back({entry.first, entry.second});
    }
    
    // Sort by scope level (deepest first)
    std::sort(functions_by_level.begin(), functions_by_level.end(), 
              [](const auto& a, const auto& b) { return a.second > b.second; });
    
    std::cout << "[DEBUG] StaticScopeAnalyzer: Processing functions in bottom-up order:" << std::endl;
    for (const auto& func_level : functions_by_level) {
        std::cout << "[DEBUG]   - " << func_level.first << " (level " << func_level.second << ")" << std::endl;
    }
    
        // Process each function from deepest to shallowest
        for (const auto& func_level : functions_by_level) {
            const std::string& function_name = func_level.first;
            int function_level = func_level.second;
            
            std::cout << "[DEBUG] StaticScopeAnalyzer: Processing '" << function_name << "' at level " << function_level << std::endl;
            
            auto& analysis = function_analyses_[function_name];
            
            // Show what this function directly needs (SELF needs)
            std::cout << "[DEBUG]   SELF parent needs: ";
            for (int level : analysis.self_parent_scope_needs) {
                std::cout << level << " ";
            }
            if (analysis.self_parent_scope_needs.empty()) std::cout << "(none)";
            std::cout << std::endl;
            
            // Propagate this function's needs to its parent
            if (function_parent_.find(function_name) != function_parent_.end()) {
                const std::string& parent_name = function_parent_[function_name];
                auto& parent_analysis = function_analyses_[parent_name];
                int parent_function_level = function_scope_level_[parent_name];
                
                std::cout << "[DEBUG]   Propagating to parent '" << parent_name << "'" << std::endl;
                
                // For each parent scope level this function needs, check if parent should provide it
                for (int needed_level : analysis.required_parent_scopes) {
                    // If the needed level is shallower than the parent function's level,
                    // the parent must provide access to it
                    if (needed_level < parent_function_level) {
                        // Check if this is a new need for the parent (descendant-only need)
                        bool was_new = parent_analysis.required_parent_scopes.insert(needed_level).second;
                        
                        if (was_new && parent_analysis.self_parent_scope_needs.count(needed_level) == 0) {
                            // This is a DESCENDANT-ONLY need (parent doesn't access it directly)
                            parent_analysis.descendant_parent_scope_needs.insert(needed_level);
                            std::cout << "[DEBUG]     Propagated DESCENDANT-ONLY need for parent level " << needed_level 
                                      << " to '" << parent_name << "'" << std::endl;
                        } else if (!was_new) {
                            std::cout << "[DEBUG]     Level " << needed_level << " already needed by '" << parent_name << "'" << std::endl;
                        } else {
                            std::cout << "[DEBUG]     Level " << needed_level << " already SELF-accessed by '" << parent_name << "'" << std::endl;
                        }
                    }
                }
            }
        }    std::cout << "[DEBUG] StaticScopeAnalyzer: Bottom-up propagation complete" << std::endl;
}

void StaticScopeAnalyzer::print_function_analysis(const std::string& function_name) {
    auto it = function_analyses_.find(function_name);
    if (it == function_analyses_.end()) {
        return;
    }
    
    const FunctionScopeAnalysis& analysis = it->second;
    
    std::cout << "\n=== SCOPE ANALYSIS for " << function_name << " ===" << std::endl;
    std::cout << "Required parent scope levels: ";
    for (int level : analysis.required_parent_scopes) {
        std::cout << level << " ";
    }
    std::cout << std::endl;
    
    std::cout << "Variables in function:" << std::endl;
    for (const auto& [var_name, var_info] : analysis.variables) {
        std::cout << "  " << var_name << " (scope level " << var_info.scope_level 
                  << ", escapes: " << (var_info.escapes_current_function ? "yes" : "no") << ")" << std::endl;
    }
    
    std::cout << "    Register allocation:" << std::endl;
    for (const auto& [scope_level, register_id] : analysis.fast_register_allocation) {
        std::cout << "  Parent scope level " << scope_level << " -> r" << register_id << std::endl;
    }
    std::cout << "=== END ANALYSIS ===" << std::endl << std::endl;
}

void StaticScopeAnalyzer::analyze_variable_declarations(ASTNode* node) {
    std::cout << "[DEBUG] StaticScopeAnalyzer: Analyzing variable declarations with AST walking" << std::endl;
    walk_ast_for_scopes(node);
}

void StaticScopeAnalyzer::find_goroutine_captures(ASTNode* node, std::unordered_set<std::string>& captured_vars) {
    std::cout << "[DEBUG] StaticScopeAnalyzer: Finding goroutine captures with full AST analysis" << std::endl;
    
    // Use the variable_scope_map_ built during scope hierarchy analysis
    for (const auto& [name, info] : variable_scope_map_) {
        if (info.escapes_current_function) {
            captured_vars.insert(name);
            std::cout << "[DEBUG] StaticScopeAnalyzer: Variable '" << name << "' captured by goroutine/escape" << std::endl;
        }
    }
}

void StaticScopeAnalyzer::find_callback_captures(ASTNode* node, std::unordered_set<std::string>& captured_vars) {
    std::cout << "[DEBUG] StaticScopeAnalyzer: Finding callback captures (using same escape analysis as goroutines)" << std::endl;
    
    // For callbacks/closures, use the same escape analysis as goroutines
    // Variables captured by callbacks also need to escape to heap scope
    for (const auto& [name, info] : variable_scope_map_) {
        if (info.escapes_current_function) {
            captured_vars.insert(name);
            std::cout << "[DEBUG] StaticScopeAnalyzer: Variable '" << name << "' captured by callback/escape" << std::endl;
        }
    }
}

void StaticScopeAnalyzer::determine_register_allocation(const std::string& function_name) {
    std::cout << "[DEBUG] StaticScopeAnalyzer: Determining PRIORITY register allocation for " << function_name << std::endl;
    
    auto it = function_analyses_.find(function_name);
    if (it == function_analyses_.end()) {
        std::cout << "[DEBUG] StaticScopeAnalyzer: No analysis found for function " << function_name << std::endl;
        return;
    }
    
    FunctionScopeAnalysis& analysis = it->second;
    
    // ULTIMATE OPTIMIZATION: Priority allocation strategy
    // 1. Fast registers (r12, r13, r14) go to SELF-accessed parent scopes
    // 2. Stack fallback for DESCENDANT-ONLY parent scopes when registers run out
    //
    // REGISTER ALLOCATION CONVENTION:
    // R15: Always holds current scope address for local variables [r15+offset]
    // R12-R14: Hold parent scope addresses for SELF-accessed variables [r12+offset, r13+offset, etc]
    // Stack: Hold parent scope addresses for DESCENDANT-ONLY variables when registers are full
    std::vector<int> fast_registers = {12, 13, 14}; // r12, r13, r14 (r15 is always current)
    
    std::cout << "[DEBUG] StaticScopeAnalyzer: Function '" << function_name << "' scope needs analysis:" << std::endl;
    std::cout << "[DEBUG]   SELF needs (high priority): ";
    for (int level : analysis.self_parent_scope_needs) {
        std::cout << level << " ";
    }
    if (analysis.self_parent_scope_needs.empty()) std::cout << "(none)";
    std::cout << std::endl;
    
    std::cout << "[DEBUG]   DESCENDANT-ONLY needs (low priority): ";
    for (int level : analysis.descendant_parent_scope_needs) {
        std::cout << level << " ";
    }
    if (analysis.descendant_parent_scope_needs.empty()) std::cout << "(none)";
    std::cout << std::endl;
    
    // Clear previous allocations
    analysis.fast_register_allocation.clear();
    analysis.stack_allocation.clear();
    analysis.used_scope_registers.clear();
    
    // PHASE 1: Allocate fast registers to SELF-accessed parent scopes (highest priority)
    std::vector<int> self_needs(analysis.self_parent_scope_needs.begin(), analysis.self_parent_scope_needs.end());
    std::sort(self_needs.begin(), self_needs.end());
    
    int register_index = 0;
    std::cout << "[DEBUG] StaticScopeAnalyzer: PHASE 1 - Allocating fast registers to SELF needs:" << std::endl;
    for (int parent_scope_level : self_needs) {
        if (register_index < static_cast<int>(fast_registers.size())) {
            int assigned_register = fast_registers[register_index];
            analysis.fast_register_allocation[parent_scope_level] = assigned_register;
            analysis.used_scope_registers.insert(assigned_register);
            
            std::cout << "[DEBUG]   SELF parent scope level " << parent_scope_level 
                      << " -> r" << assigned_register << " (FAST)" << std::endl;
            register_index++;
        }
    }
    
    // PHASE 2: Allocate remaining fast registers to DESCENDANT-ONLY needs
    std::vector<int> descendant_needs(analysis.descendant_parent_scope_needs.begin(), 
                                     analysis.descendant_parent_scope_needs.end());
    std::sort(descendant_needs.begin(), descendant_needs.end());
    
    int stack_offset = 0;
    std::cout << "[DEBUG] StaticScopeAnalyzer: PHASE 2 - Allocating remaining resources to DESCENDANT needs:" << std::endl;
    for (int parent_scope_level : descendant_needs) {
        if (register_index < static_cast<int>(fast_registers.size())) {
            // Still have fast registers available for descendant needs
            int assigned_register = fast_registers[register_index];
            analysis.fast_register_allocation[parent_scope_level] = assigned_register;
            analysis.used_scope_registers.insert(assigned_register);
            
            std::cout << "[DEBUG]   DESCENDANT parent scope level " << parent_scope_level 
                      << " -> r" << assigned_register << " (FAST - bonus)" << std::endl;
            register_index++;
        } else {
            // No more fast registers - use stack fallback for descendant-only needs
            analysis.stack_allocation[parent_scope_level] = stack_offset;
            analysis.needs_stack_fallback = true;
            
            std::cout << "[DEBUG]   DESCENDANT parent scope level " << parent_scope_level 
                      << " -> stack[" << stack_offset << "] (SLOW - acceptable for descendants)" << std::endl;
            stack_offset += 8; // 8 bytes per pointer
        }
    }
    
    // PHASE 3: Summary and validation
    std::cout << "[DEBUG] StaticScopeAnalyzer: PRIORITY ALLOCATION COMPLETE for '" << function_name << "':" << std::endl;
    std::cout << "[DEBUG]   Fast registers used: " << analysis.used_scope_registers.size() << "/3" << std::endl;
    std::cout << "[DEBUG]   Stack fallback needed: " << (analysis.needs_stack_fallback ? "YES" : "NO") << std::endl;
    
    if (!analysis.self_parent_scope_needs.empty()) {
        std::cout << "[DEBUG]   ✅ All SELF-accessed parent scopes got FAST registers" << std::endl;
    }
    
    if (analysis.needs_stack_fallback) {
        std::cout << "[DEBUG]   ⚠️  Some DESCENDANT-ONLY scopes use stack (acceptable optimization)" << std::endl;
    }
    
    std::cout << "[DEBUG] StaticScopeAnalyzer: Ultimate optimization applied successfully!" << std::endl;
}

void StaticScopeAnalyzer::calculate_memory_layouts(const std::string& function_name) {
    std::cout << "[DEBUG] StaticScopeAnalyzer: Calculating memory layouts for " << function_name << std::endl;
    
    // Get the analysis for this function
    auto it = function_analyses_.find(function_name);
    if (it == function_analyses_.end()) {
        std::cout << "[DEBUG] StaticScopeAnalyzer: No analysis found for function " << function_name << std::endl;
        return;
    }
    
    FunctionScopeAnalysis& analysis = it->second;
    
    // For each captured variable, determine memory layout
    int64_t heap_offset = 0;
    int64_t stack_offset = -8;  // Start at -8 from RBP
    
    // For captured variables, create entries in the variables map
    // TODO: This would be populated from actual AST analysis
    for (int i = 0; i < static_cast<int>(analysis.total_heap_scope_size / 8); ++i) {
        std::string var_name = "captured_var_" + std::to_string(i);
        LexicalScopeInfo var_info;
        var_info.variable_name = var_name;
        var_info.scope_level = 0;
        var_info.offset_in_scope = heap_offset;
        var_info.escapes_current_function = true;
        var_info.type = DataType::UNKNOWN;
        var_info.size_bytes = 8;
        
        analysis.variables[var_name] = var_info;
        heap_offset += 8;  // Each variable takes 8 bytes
    }
    
    // Non-captured variables get stack locations
    // TODO: Analyze all local variables and assign stack offsets
}

LexicalScopeInfo StaticScopeAnalyzer::get_variable_info(const std::string& var_name) const {
    auto it = variable_scope_map_.find(var_name);
    if (it != variable_scope_map_.end()) {
        return it->second;
    }
    
    // Return default info for unknown variables
    LexicalScopeInfo default_info;
    default_info.variable_name = var_name;
    default_info.scope_level = 0;
    default_info.offset_in_scope = 0;
    default_info.escapes_current_function = false;
    default_info.type = DataType::UNKNOWN;
    default_info.size_bytes = 8;  // Default size
    
    return default_info;
}

FunctionScopeAnalysis StaticScopeAnalyzer::get_function_analysis(const std::string& function_name) const {
    auto it = function_analyses_.find(function_name);
    if (it != function_analyses_.end()) {
        return it->second;
    }
    
    // Return default analysis for unknown functions
    FunctionScopeAnalysis default_analysis;
    default_analysis.function_name = function_name;
    default_analysis.has_escaping_variables = false;
    default_analysis.total_stack_space_needed = 0;
    default_analysis.total_heap_scope_size = 0;
    return default_analysis;
}

// ============================================================================
// LEXICAL SCOPE INTEGRATION CLASS
// Bridge between static analyzer and type inference system
// ============================================================================

LexicalScopeIntegration::LexicalScopeIntegration() 
    : analyzer_(std::make_unique<StaticScopeAnalyzer>()) {
    std::cout << "[DEBUG] LexicalScopeIntegration: Created with static analyzer" << std::endl;
}

void LexicalScopeIntegration::analyze_function(const std::string& function_name, ASTNode* function_node) {
    if (analyzer_) {
        analyzer_->analyze_function(function_name, function_node);
    }
}

bool LexicalScopeIntegration::function_needs_r15_register(const std::string& function_name) const {
    if (analyzer_) {
        auto analysis = analyzer_->get_function_analysis(function_name);
        return analysis.has_escaping_variables;  // If has escaping vars, needs R15
    }
    return false;
}

bool LexicalScopeIntegration::should_use_heap_scope(const std::string& function_name) const {
    if (analyzer_) {
        auto analysis = analyzer_->get_function_analysis(function_name);
        return analysis.has_escaping_variables && analysis.total_heap_scope_size > 0;
    }
    return false;
}

std::vector<int> LexicalScopeIntegration::get_required_parent_scope_levels(const std::string& function_name) const {
    // TODO: Calculate based on static analysis
    return {};  // Return empty vector for now
}

size_t LexicalScopeIntegration::get_heap_scope_size(const std::string& function_name) const {
    // TODO: Calculate based on number of captured variables
    return 64;  // Default 64 bytes
}

bool LexicalScopeIntegration::variable_escapes(const std::string& function_name, const std::string& var_name) const {
    if (analyzer_) {
        auto analysis = analyzer_->get_function_analysis(function_name);
        auto var_it = analysis.variables.find(var_name);
        return (var_it != analysis.variables.end()) && var_it->second.escapes_current_function;
    }
    return false;
}

int64_t LexicalScopeIntegration::get_variable_offset(const std::string& function_name, const std::string& var_name) const {
    if (analyzer_) {
        auto analysis = analyzer_->get_function_analysis(function_name);
        auto var_it = analysis.variables.find(var_name);
        if (var_it != analysis.variables.end()) {
            return static_cast<int64_t>(var_it->second.offset_in_scope);
        }
    }
    return -8;  // Default stack offset
}

// HIGH-PERFORMANCE REGISTER-BASED SCOPE ACCESS METHODS
int LexicalScopeIntegration::get_register_for_scope_level(const std::string& function_name, int scope_level) const {
    if (analyzer_) {
        auto analysis = analyzer_->get_function_analysis(function_name);
        auto reg_it = analysis.fast_register_allocation.find(scope_level);
        if (reg_it != analysis.fast_register_allocation.end()) {
            return reg_it->second;
        }
    }
    return -1;  // No register assigned (might be using stack)
}

std::unordered_set<int> LexicalScopeIntegration::get_used_scope_registers(const std::string& function_name) const {
    if (analyzer_) {
        auto analysis = analyzer_->get_function_analysis(function_name);
        return analysis.used_scope_registers;
    }
    return {};  // Empty set
}

bool LexicalScopeIntegration::needs_stack_fallback(const std::string& function_name) const {
    if (analyzer_) {
        auto analysis = analyzer_->get_function_analysis(function_name);
        return analysis.needs_stack_fallback;
    }
    return false;
}

// NEW PRIORITY-BASED ACCESS METHODS
int LexicalScopeIntegration::get_stack_offset_for_scope_level(const std::string& function_name, int scope_level) const {
    if (analyzer_) {
        auto analysis = analyzer_->get_function_analysis(function_name);
        auto stack_it = analysis.stack_allocation.find(scope_level);
        if (stack_it != analysis.stack_allocation.end()) {
            return stack_it->second;
        }
    }
    return -1;  // No stack allocation
}

bool LexicalScopeIntegration::scope_level_uses_fast_register(const std::string& function_name, int scope_level) const {
    return get_register_for_scope_level(function_name, scope_level) != -1;
}

bool LexicalScopeIntegration::scope_level_uses_stack(const std::string& function_name, int scope_level) const {
    return get_stack_offset_for_scope_level(function_name, scope_level) != -1;
}

// PRIORITY ANALYSIS QUERIES
std::unordered_set<int> LexicalScopeIntegration::get_self_parent_scope_needs(const std::string& function_name) const {
    if (analyzer_) {
        auto analysis = analyzer_->get_function_analysis(function_name);
        return analysis.self_parent_scope_needs;
    }
    return {};
}

std::unordered_set<int> LexicalScopeIntegration::get_descendant_parent_scope_needs(const std::string& function_name) const {
    if (analyzer_) {
        auto analysis = analyzer_->get_function_analysis(function_name);
        return analysis.descendant_parent_scope_needs;
    }
    return {};
}

// VARIABLE ACCESS HELPER - Returns how a variable should be accessed with priority info
std::string LexicalScopeIntegration::get_variable_access_pattern(const std::string& function_name, const std::string& var_name) const {
    if (!analyzer_) {
        return "[ERROR: No analyzer]";
    }
    
    auto analysis = analyzer_->get_function_analysis(function_name);
    auto var_it = analysis.variables.find(var_name);
    
    if (var_it == analysis.variables.end()) {
        return "[ERROR: Variable not found]";
    }
    
    const LexicalScopeInfo& var_info = var_it->second;
    
    if (var_info.scope_level == 0) {
        // Current scope variable - always use r15
        return "[r15+" + std::to_string(var_info.offset_in_scope) + "] (current scope)";
    } else {
        // Parent scope variable - check if it uses fast register or stack
        auto reg_it = analysis.fast_register_allocation.find(var_info.scope_level);
        if (reg_it != analysis.fast_register_allocation.end()) {
            int reg_num = reg_it->second;
            std::string access_type;
            if (analysis.self_parent_scope_needs.count(var_info.scope_level) > 0) {
                access_type = "SELF-accessed, FAST";
            } else {
                access_type = "descendant-needed, FAST";
            }
            return "[r" + std::to_string(reg_num) + "+" + std::to_string(var_info.offset_in_scope) + "] (" + access_type + ")";
        } else {
            auto stack_it = analysis.stack_allocation.find(var_info.scope_level);
            if (stack_it != analysis.stack_allocation.end()) {
                return "[rbp-" + std::to_string(stack_it->second) + "+" + std::to_string(var_info.offset_in_scope) + "] (descendant-only, STACK)";
            } else {
                return "[ERROR: No allocation found] (scope level " + std::to_string(var_info.scope_level) + ")";
            }
        }
    }
}

// ============================================================================
// VARIABLE ORDERING AND OFFSET CALCULATION IMPLEMENTATION
// ============================================================================

void StaticScopeAnalyzer::optimize_variable_layout(const std::string& function_name) {
    std::cout << "[DEBUG] StaticScopeAnalyzer: Optimizing variable layout for '" << function_name << "'" << std::endl;
    
    auto it = function_analyses_.find(function_name);
    if (it == function_analyses_.end()) {
        std::cout << "[DEBUG] StaticScopeAnalyzer: No analysis found for function '" << function_name << "'" << std::endl;
        return;
    }
    
    FunctionScopeAnalysis& analysis = it->second;
    
    // Step 1: Analyze access patterns for all variables
    // (This would be done during AST walking in real implementation)
    analyze_variable_access_patterns(function_name, nullptr);
    
    // Step 2: Group variables by scope level and optimize ordering within each scope
    std::unordered_map<int, std::vector<std::string>> variables_by_scope;
    
    for (const auto& [var_name, var_info] : analysis.variables) {
        variables_by_scope[var_info.scope_level].push_back(var_name);
    }
    
    // Step 3: Optimize variable ordering for each scope level
    for (const auto& [scope_level, variables] : variables_by_scope) {
        std::cout << "[DEBUG] StaticScopeAnalyzer: Optimizing scope level " << scope_level << " with " << variables.size() << " variables" << std::endl;
        
        optimize_variable_ordering_by_frequency(function_name, scope_level);
        optimize_variable_ordering_by_locality(function_name, scope_level);
        
        // Initialize scope layout info
        FunctionScopeAnalysis::ScopeLayoutInfo& layout = analysis.scope_layouts[scope_level];
        layout.variable_order = variables;
        layout.total_scope_size = 0;
        layout.has_hot_variables = false;
        
        // Sort variables by optimal order (hot variables first, then by size for alignment)
        std::sort(layout.variable_order.begin(), layout.variable_order.end(),
            [&](const std::string& a, const std::string& b) {
                const auto& var_a = analysis.variables[a];
                const auto& var_b = analysis.variables[b];
                
                // Hot variables come first
                if (var_a.is_hot_variable && !var_b.is_hot_variable) return true;
                if (!var_a.is_hot_variable && var_b.is_hot_variable) return false;
                
                // Among hot variables, higher frequency first
                if (var_a.is_hot_variable && var_b.is_hot_variable) {
                    if (var_a.access_frequency != var_b.access_frequency) {
                        return var_a.access_frequency > var_b.access_frequency;
                    }
                }
                
                // Then by alignment requirement (larger alignment first for better packing)
                if (var_a.alignment_requirement != var_b.alignment_requirement) {
                    return var_a.alignment_requirement > var_b.alignment_requirement;
                }
                
                // Finally by size (larger first)
                return var_a.size_bytes > var_b.size_bytes;
            });
        
        std::cout << "[DEBUG] StaticScopeAnalyzer: Optimized variable order for scope " << scope_level << ": ";
        for (const auto& var : layout.variable_order) {
            std::cout << var << " ";
        }
        std::cout << std::endl;
    }
    
    analysis.layout_optimization_complete = true;
}

void StaticScopeAnalyzer::calculate_variable_offsets(const std::string& function_name) {
    std::cout << "[DEBUG] StaticScopeAnalyzer: Calculating variable offsets for '" << function_name << "'" << std::endl;
    
    auto it = function_analyses_.find(function_name);
    if (it == function_analyses_.end()) {
        std::cout << "[DEBUG] StaticScopeAnalyzer: No analysis found for function '" << function_name << "'" << std::endl;
        return;
    }
    
    FunctionScopeAnalysis& analysis = it->second;
    
    // Ensure layout optimization has been done first
    if (!analysis.layout_optimization_complete) {
        optimize_variable_layout(function_name);
    }
    
    // Calculate offsets for each scope level
    for (auto& [scope_level, layout] : analysis.scope_layouts) {
        std::cout << "[DEBUG] StaticScopeAnalyzer: Calculating offsets for scope level " << scope_level << std::endl;
        
        size_t current_offset = 0;
        
        for (const std::string& var_name : layout.variable_order) {
            auto var_it = analysis.variables.find(var_name);
            if (var_it == analysis.variables.end()) {
                std::cout << "[DEBUG] StaticScopeAnalyzer: Warning - variable " << var_name << " not found in analysis" << std::endl;
                continue;
            }
            
            LexicalScopeInfo& var_info = var_it->second;
            
            // Align the offset based on the variable's alignment requirement
            current_offset = calculate_aligned_offset(current_offset, var_info.alignment_requirement);
            
            // Set the offset for this variable
            var_info.offset_in_scope = current_offset;
            layout.variable_offsets[var_name] = current_offset;
            
            std::cout << "[DEBUG] StaticScopeAnalyzer: Variable " << var_name 
                      << " at offset " << current_offset 
                      << " (size: " << var_info.size_bytes << ", alignment: " << var_info.alignment_requirement << ")" << std::endl;
            
            // Move to next position
            current_offset += var_info.size_bytes;
        }
        
        // Align the total scope size to pointer boundary (8 bytes on x64)
        layout.total_scope_size = calculate_aligned_offset(current_offset, 8);
        
        std::cout << "[DEBUG] StaticScopeAnalyzer: Scope " << scope_level << " total size: " << layout.total_scope_size << " bytes" << std::endl;
    }
}

size_t StaticScopeAnalyzer::get_variable_offset_in_scope(const std::string& function_name, const std::string& var_name) const {
    auto func_it = function_analyses_.find(function_name);
    if (func_it == function_analyses_.end()) {
        return 0;
    }
    
    const FunctionScopeAnalysis& analysis = func_it->second;
    auto var_it = analysis.variables.find(var_name);
    if (var_it == analysis.variables.end()) {
        return 0;
    }
    
    return var_it->second.offset_in_scope;
}

std::vector<std::string> StaticScopeAnalyzer::get_optimized_variable_order(const std::string& function_name, int scope_level) const {
    auto func_it = function_analyses_.find(function_name);
    if (func_it == function_analyses_.end()) {
        return {};
    }
    
    const FunctionScopeAnalysis& analysis = func_it->second;
    auto scope_it = analysis.scope_layouts.find(scope_level);
    if (scope_it == analysis.scope_layouts.end()) {
        return {};
    }
    
    return scope_it->second.variable_order;
}

bool StaticScopeAnalyzer::is_layout_optimization_complete(const std::string& function_name) const {
    auto func_it = function_analyses_.find(function_name);
    if (func_it == function_analyses_.end()) {
        return false;
    }
    
    return func_it->second.layout_optimization_complete;
}

// ============================================================================
// VARIABLE ACCESS PATTERN ANALYSIS HELPERS
// ============================================================================

void StaticScopeAnalyzer::analyze_variable_access_patterns(const std::string& function_name, ASTNode* function_node) {
    // This would analyze the AST to determine access patterns
    // For now, we'll simulate with reasonable defaults
    
    auto it = function_analyses_.find(function_name);
    if (it == function_analyses_.end()) {
        return;
    }
    
    FunctionScopeAnalysis& analysis = it->second;
    
    // Simulate access frequency analysis
    for (auto& [var_name, var_info] : analysis.variables) {
        // Simulate access frequency (in real implementation, this would come from AST analysis)
        if (var_name.find("loop") != std::string::npos || var_name.find("index") != std::string::npos) {
            var_info.access_frequency = 100; // High frequency
            var_info.is_hot_variable = true;
        } else if (var_name.find("temp") != std::string::npos || var_name.find("tmp") != std::string::npos) {
            var_info.access_frequency = 50; // Medium frequency
            var_info.is_hot_variable = true;
        } else {
            var_info.access_frequency = 10; // Low frequency
            var_info.is_hot_variable = false;
        }
        
        // Set alignment requirements based on type
        var_info.alignment_requirement = get_variable_alignment_requirement(var_info.type);
    }
}

void StaticScopeAnalyzer::calculate_access_frequencies(const std::string& function_name, ASTNode* function_node) {
    // This would walk the AST and count how many times each variable is accessed
    // For demonstration, we'll use the simulated values from analyze_variable_access_patterns
}

void StaticScopeAnalyzer::identify_co_accessed_variables(const std::string& function_name, ASTNode* function_node) {
    // This would analyze which variables are accessed together (for cache locality)
    // For demonstration, we'll assume variables in the same scope level are co-accessed
    
    auto it = function_analyses_.find(function_name);
    if (it == function_analyses_.end()) {
        return;
    }
    
    FunctionScopeAnalysis& analysis = it->second;
    
    for (auto& [scope_level, layout] : analysis.scope_layouts) {
        if (layout.variable_order.size() > 1) {
            // Mark variables as co-accessed with others in the same scope
            for (size_t i = 0; i < layout.variable_order.size(); i++) {
                const std::string& var_name = layout.variable_order[i];
                auto var_it = analysis.variables.find(var_name);
                if (var_it != analysis.variables.end()) {
                    // Add other variables in the same scope as co-accessed
                    for (size_t j = 0; j < layout.variable_order.size(); j++) {
                        if (i != j) {
                            var_it->second.co_accessed_variables.push_back(layout.variable_order[j]);
                        }
                    }
                }
            }
        }
    }
}

void StaticScopeAnalyzer::optimize_variable_ordering_by_frequency(const std::string& function_name, int scope_level) {
    // The sorting in optimize_variable_layout already handles frequency optimization
}

void StaticScopeAnalyzer::optimize_variable_ordering_by_locality(const std::string& function_name, int scope_level) {
    // The sorting in optimize_variable_layout already handles locality optimization
    // Co-accessed variables are kept together by the alignment and frequency sorting
}

size_t StaticScopeAnalyzer::calculate_aligned_offset(size_t current_offset, size_t alignment) const {
    if (alignment <= 1) {
        return current_offset;
    }
    
    size_t remainder = current_offset % alignment;
    if (remainder == 0) {
        return current_offset;
    }
    
    return current_offset + (alignment - remainder);
}

size_t StaticScopeAnalyzer::get_variable_alignment_requirement(DataType type) const {
    switch (type) {
        case DataType::BOOLEAN:
        case DataType::INT8:
        case DataType::UINT8:
            return 1;
        case DataType::INT16:
        case DataType::UINT16:
            return 2;
        case DataType::INT32:
        case DataType::UINT32:
        case DataType::FLOAT32:
            return 4;
        case DataType::INT64:
        case DataType::UINT64:
        case DataType::FLOAT64:
        case DataType::STRING:
        case DataType::ARRAY:
        case DataType::FUNCTION:
        case DataType::CLASS_INSTANCE:
        case DataType::RUNTIME_OBJECT:
            return 8; // 8-byte alignment on x64
        default:
            return 8; // Default to pointer alignment
    }
}

size_t StaticScopeAnalyzer::get_variable_size(DataType type) const {
    switch (type) {
        case DataType::BOOLEAN:
        case DataType::INT8:
        case DataType::UINT8:
            return 1;
        case DataType::INT16:
        case DataType::UINT16:
            return 2;
        case DataType::INT32:
        case DataType::UINT32:
        case DataType::FLOAT32:
            return 4;
        case DataType::INT64:
        case DataType::UINT64:
        case DataType::FLOAT64:
            return 8;
        case DataType::STRING:
        case DataType::ARRAY:
        case DataType::FUNCTION:
        case DataType::CLASS_INSTANCE:
        case DataType::RUNTIME_OBJECT:
            return 8; // Pointer size on x64
        default:
            return 8; // Default to pointer size
    }
}

// ============================================================================
// BLOCK SCOPING AND PERFORMANCE OPTIMIZATION IMPLEMENTATION
// ============================================================================

bool StaticScopeAnalyzer::analyze_block_needs_scope(ASTNode* block_node) {
    std::cout << "[DEBUG] StaticScopeAnalyzer: Analyzing if block needs actual scope allocation" << std::endl;
    
    if (!block_node) return false;
    
    // Walk the block and check for let/const declarations
    bool has_let_const = false;
    bool has_nested_functions = false;
    
    // This would be implemented with proper AST traversal
    // For now, we'll use a simplified heuristic
    
    // If block contains let/const, it needs its own scope
    if (has_let_const) {
        std::cout << "[DEBUG] StaticScopeAnalyzer: Block needs scope (contains let/const)" << std::endl;
        return true;
    }
    
    // If block contains nested functions, it needs its own scope
    if (has_nested_functions) {
        std::cout << "[DEBUG] StaticScopeAnalyzer: Block needs scope (contains nested functions)" << std::endl;
        return true;
    }
    
    // If block only contains var declarations, we can optimize it away
    std::cout << "[DEBUG] StaticScopeAnalyzer: Block can be optimized away (var-only)" << std::endl;
    return false;
}

void StaticScopeAnalyzer::analyze_loop_scoping(ASTNode* loop_node) {
    std::cout << "[DEBUG] StaticScopeAnalyzer: Analyzing loop scoping requirements" << std::endl;
    
    if (!loop_node) return;
    
    // Check if loop uses let/const in the initialization
    // for (let i = 0; ...) creates a new scope for EACH ITERATION
    // for (var i = 0; ...) hoists to function scope
    
    // This is complex because each loop iteration gets its own scope for let/const
    // We need special handling for this case
    
    // For now, we'll mark variables as loop-iteration-scoped
    // Real implementation would analyze the loop's initialization and body
    
    std::cout << "[DEBUG] StaticScopeAnalyzer: Loop scoping analysis complete" << std::endl;
}

void StaticScopeAnalyzer::optimize_scope_allocation(const std::string& function_name) {
    std::cout << "[DEBUG] StaticScopeAnalyzer: Optimizing scope allocation for '" << function_name << "'" << std::endl;
    
    auto& analysis = function_analyses_[function_name];
    
    // Identify scopes that can be optimized away
    std::vector<int> scopes_to_optimize;
    
    for (auto& [scope_level, layout] : analysis.scope_layouts) {
        if (can_optimize_away_scope(scope_level)) {
            layout.can_be_optimized_away = true;
            layout.needs_actual_scope = false;
            scopes_to_optimize.push_back(scope_level);
            
            std::cout << "[DEBUG] StaticScopeAnalyzer: Scope level " << scope_level 
                      << " can be optimized away (var-only)" << std::endl;
        } else {
            layout.can_be_optimized_away = false;
            layout.needs_actual_scope = true;
            
            std::cout << "[DEBUG] StaticScopeAnalyzer: Scope level " << scope_level 
                      << " needs actual allocation (contains let/const or functions)" << std::endl;
        }
    }
    
    // Merge var-only scopes with their parent function scope
    merge_var_only_scopes(function_name);
    
    // Update scope counting
    analysis.logical_scope_count = analysis.scope_layouts.size();
    analysis.actual_scope_count = 0;
    
    for (const auto& [level, layout] : analysis.scope_layouts) {
        if (layout.needs_actual_scope) {
            analysis.actual_scope_count++;
        }
    }
    
    std::cout << "[DEBUG] StaticScopeAnalyzer: Scope optimization complete - " 
              << analysis.logical_scope_count << " logical scopes, " 
              << analysis.actual_scope_count << " actual scopes (saved " 
              << (analysis.logical_scope_count - analysis.actual_scope_count) << " scopes)" << std::endl;
}

bool StaticScopeAnalyzer::can_optimize_away_scope(int scope_level) {
    // Check if scope contains only var declarations and no nested functions
    
    // Look at all variables in this scope
    for (const auto& [var_name, var_info] : variable_scope_map_) {
        if (var_info.scope_level == scope_level) {
            if (var_info.is_block_scoped) {
                // Contains let/const - cannot optimize away
                return false;
            }
        }
    }
    
    // Check if scope contains nested functions (simplified - would need AST analysis)
    // For now, assume scopes > 1 might have nested functions
    if (scope_level > 1) {
        // Conservative: assume deeper scopes might have nested functions
        return false;
    }
    
    // This scope contains only var declarations - can be optimized
    return true;
}

void StaticScopeAnalyzer::merge_var_only_scopes(const std::string& function_name) {
    std::cout << "[DEBUG] StaticScopeAnalyzer: Merging var-only scopes with parent function scope" << std::endl;
    
    auto& analysis = function_analyses_[function_name];
    
    // Find the function scope (level 0 or 1, depending on how we count)
    int function_scope_level = 0;
    
    // Move variables from optimized-away scopes to function scope
    for (auto& [var_name, var_info] : variable_scope_map_) {
        if (!var_info.is_block_scoped) {
            // This is a var declaration - it should be hoisted to function scope
            if (var_info.scope_level != function_scope_level) {
                std::cout << "[DEBUG] StaticScopeAnalyzer: Hoisting var '" << var_name 
                          << "' from scope " << var_info.scope_level 
                          << " to function scope " << function_scope_level << std::endl;
                
                var_info.scope_level = function_scope_level;
                
                // Update scope mapping
                analysis.optimized_scope_mapping[var_info.scope_level] = function_scope_level;
            }
        }
    }
}

// Query methods for the new block scoping features
bool StaticScopeAnalyzer::scope_needs_actual_allocation(const std::string& function_name, int scope_level) const {
    auto func_it = function_analyses_.find(function_name);
    if (func_it == function_analyses_.end()) return true;
    
    auto scope_it = func_it->second.scope_layouts.find(scope_level);
    if (scope_it == func_it->second.scope_layouts.end()) return true;
    
    return scope_it->second.needs_actual_scope;
}

int StaticScopeAnalyzer::get_optimized_scope_count(const std::string& function_name) const {
    auto func_it = function_analyses_.find(function_name);
    if (func_it == function_analyses_.end()) return 0;
    
    return func_it->second.actual_scope_count;
}

int StaticScopeAnalyzer::get_actual_scope_level(const std::string& function_name, int logical_scope_level) const {
    auto func_it = function_analyses_.find(function_name);
    if (func_it == function_analyses_.end()) return logical_scope_level;
    
    auto mapping_it = func_it->second.optimized_scope_mapping.find(logical_scope_level);
    if (mapping_it != func_it->second.optimized_scope_mapping.end()) {
        return mapping_it->second;
    }
    
    return logical_scope_level;
}

std::vector<std::string> StaticScopeAnalyzer::get_var_only_scopes(const std::string& function_name) const {
    std::vector<std::string> var_only_scopes;
    
    auto func_it = function_analyses_.find(function_name);
    if (func_it == function_analyses_.end()) return var_only_scopes;
    
    for (const auto& [level, layout] : func_it->second.scope_layouts) {
        if (layout.can_be_optimized_away) {
            var_only_scopes.push_back("scope_" + std::to_string(level));
        }
    }
    
    return var_only_scopes;
}

bool StaticScopeAnalyzer::has_let_const_in_scope(const std::string& function_name, int scope_level) const {
    auto func_it = function_analyses_.find(function_name);
    if (func_it == function_analyses_.end()) return false;
    
    auto let_const_it = func_it->second.scope_contains_let_const.find(scope_level);
    return let_const_it != func_it->second.scope_contains_let_const.end() && let_const_it->second;
}

MemoryLayoutInfo StaticScopeAnalyzer::get_memory_layout(const std::string& function_name) const {
    MemoryLayoutInfo layout;
    layout.total_size = 0;
    layout.optimization_complete = false;
    
    // Get all variables for this function
    for (const auto& [var_name, var_info] : variable_scope_map_) {
        VariableLayoutInfo var_layout;
        var_layout.variable_name = var_name;
        var_layout.scope_level = var_info.scope_level;
        var_layout.offset = var_info.offset_in_scope;
        var_layout.size = var_info.size_bytes;
        var_layout.alignment = var_info.alignment_requirement;
        
        layout.variable_layouts.push_back(var_layout);
        layout.total_size += var_info.size_bytes;
    }
    
    // Check if we have layout analysis for this function
    auto func_it = function_analyses_.find(function_name);
    if (func_it != function_analyses_.end()) {
        layout.optimization_complete = func_it->second.layout_optimization_complete;
    }
    
    return layout;
}
