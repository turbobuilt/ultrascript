#include "ultrascript_phase_integration.h"
#include "compiler.h"
#include <iostream>
#include <algorithm>

// Global integration instance
UltraScriptPhaseIntegration g_phase_integration;

void UltraScriptPhaseIntegration::perform_complete_static_analysis(
    std::vector<std::unique_ptr<ASTNode>>& ast,
    StaticAnalyzer* static_analyzer) {
    
    std::cout << "[PHASE_INTEGRATION] Performing complete static analysis with StaticAnalyzer" << std::endl;
    
    // Step 1: Collect all scope nodes from the analyzer
    collect_all_scopes_from_analyzer(static_analyzer);
    
    // Step 2: Collect all function declarations and expressions from AST
    collect_all_functions_from_ast(ast);
    
    // Step 3: Compute complete function analysis for all functions
    compute_all_function_dependencies();
    
    // Step 4: Compute function variable strategies
    compute_all_variable_strategies();
    
    analysis_complete_ = true;
    
    std::cout << "[PHASE_INTEGRATION] Static analysis complete: " 
              << all_function_declarations_.size() << " function declarations, "
              << all_function_expressions_.size() << " function expressions analyzed" << std::endl;
}

void UltraScriptPhaseIntegration::perform_complete_static_analysis(
    std::vector<std::unique_ptr<ASTNode>>& ast,
    SimpleLexicalScopeAnalyzer* scope_analyzer) {
    
    std::cout << "[PHASE_INTEGRATION] Performing complete static analysis with SimpleLexicalScopeAnalyzer" << std::endl;
    
    // Step 1: Collect all scope nodes from the analyzer
    collect_all_scopes_from_analyzer(scope_analyzer);
    
    // Step 2: Collect all function declarations and expressions from AST
    collect_all_functions_from_ast(ast);
    
    // Step 3: Compute complete function analysis for all functions
    compute_all_function_dependencies();
    
    // Step 4: Compute function variable strategies
    compute_all_variable_strategies();
    
    analysis_complete_ = true;
    
    std::cout << "[PHASE_INTEGRATION] Static analysis complete: " 
              << all_function_declarations_.size() << " function declarations, "
              << all_function_expressions_.size() << " function expressions analyzed" << std::endl;
}

void UltraScriptPhaseIntegration::collect_all_scopes_from_analyzer(StaticAnalyzer* analyzer) {
    std::cout << "[PHASE_INTEGRATION] Collecting scopes from StaticAnalyzer" << std::endl;
    
    // Collect all available scope nodes from the analyzer
    for (int depth = 1; depth <= 10; ++depth) { // Check reasonable depth range
        LexicalScopeNode* scope = analyzer->get_scope_node_for_depth(depth);
        if (scope) {
            all_scope_nodes_[depth] = scope;
            std::cout << "[PHASE_INTEGRATION]   Found scope at depth " << depth << std::endl;
        }
    }
    
    std::cout << "[PHASE_INTEGRATION] Collected " << all_scope_nodes_.size() << " scopes" << std::endl;
}

void UltraScriptPhaseIntegration::collect_all_scopes_from_analyzer(SimpleLexicalScopeAnalyzer* analyzer) {
    std::cout << "[PHASE_INTEGRATION] Collecting scopes from SimpleLexicalScopeAnalyzer" << std::endl;
    
    // Collect all available scope nodes from the analyzer
    for (int depth = 1; depth <= 10; ++depth) { // Check reasonable depth range
        LexicalScopeNode* scope = analyzer->get_scope_node_for_depth(depth);
        if (scope) {
            all_scope_nodes_[depth] = scope;
            std::cout << "[PHASE_INTEGRATION]   Found scope at depth " << depth << std::endl;
        }
    }
    
    std::cout << "[PHASE_INTEGRATION] Collected " << all_scope_nodes_.size() << " scopes" << std::endl;
}

void UltraScriptPhaseIntegration::collect_all_functions_from_ast(const std::vector<std::unique_ptr<ASTNode>>& ast) {
    std::cout << "[PHASE_INTEGRATION] Collecting functions from AST" << std::endl;
    
    all_function_declarations_.clear();
    all_function_expressions_.clear();
    
    // Traverse AST to find all function declarations and expressions
    for (const auto& node : ast) {
        traverse_ast_for_functions(node.get());
    }
    
    // Also collect functions declared in scopes
    for (const auto& scope_pair : all_scope_nodes_) {
        collect_functions_from_scope(scope_pair.second);
    }
    
    std::cout << "[PHASE_INTEGRATION] Collected " << all_function_declarations_.size() 
              << " function declarations and " << all_function_expressions_.size() 
              << " function expressions" << std::endl;
}

void UltraScriptPhaseIntegration::traverse_ast_for_functions(ASTNode* node) {
    if (!node) return;
    
    // Check if this node is a function declaration
    if (auto func_decl = dynamic_cast<FunctionDecl*>(node)) {
        all_function_declarations_.push_back(func_decl);
        std::cout << "[PHASE_INTEGRATION]   Found function declaration: " << func_decl->name << std::endl;
        
        // Traverse function body
        for (const auto& stmt : func_decl->body) {
            traverse_ast_for_functions(stmt.get());
        }
    }
    // Check if this node is a function expression
    else if (auto func_expr = dynamic_cast<FunctionExpression*>(node)) {
        all_function_expressions_.push_back(func_expr);
        std::cout << "[PHASE_INTEGRATION]   Found function expression" << std::endl;
        
        // Traverse function body
        for (const auto& stmt : func_expr->body) {
            traverse_ast_for_functions(stmt.get());
        }
    }
    // Traverse other node types that can contain functions
    else if (auto if_stmt = dynamic_cast<IfStatement*>(node)) {
        if (if_stmt->condition) traverse_ast_for_functions(if_stmt->condition.get());
        for (const auto& stmt : if_stmt->then_body) {
            traverse_ast_for_functions(stmt.get());
        }
        for (const auto& stmt : if_stmt->else_body) {
            traverse_ast_for_functions(stmt.get());
        }
    }
    else if (auto while_stmt = dynamic_cast<WhileStatement*>(node)) {
        if (while_stmt->condition) traverse_ast_for_functions(while_stmt->condition.get());
        for (const auto& stmt : while_stmt->body) {
            traverse_ast_for_functions(stmt.get());
        }
    }
    // Add more node types as needed...
}

void UltraScriptPhaseIntegration::collect_functions_from_scope(LexicalScopeNode* scope) {
    if (!scope) return;
    
    // Collect function declarations from scope
    for (FunctionDecl* func_decl : scope->declared_functions) {
        if (std::find(all_function_declarations_.begin(), all_function_declarations_.end(), func_decl) 
            == all_function_declarations_.end()) {
            all_function_declarations_.push_back(func_decl);
            std::cout << "[PHASE_INTEGRATION]   Found function in scope: " << func_decl->name << std::endl;
        }
    }
    
    // Collect function expressions from scope
    for (FunctionExpression* func_expr : scope->declared_function_expressions) {
        if (std::find(all_function_expressions_.begin(), all_function_expressions_.end(), func_expr) 
            == all_function_expressions_.end()) {
            all_function_expressions_.push_back(func_expr);
            std::cout << "[PHASE_INTEGRATION]   Found function expression in scope" << std::endl;
        }
    }
}

void UltraScriptPhaseIntegration::compute_all_function_dependencies() {
    std::cout << "[PHASE_INTEGRATION] Computing function dependencies" << std::endl;
    
    // Compute analysis for all function declarations
    for (FunctionDecl* func_decl : all_function_declarations_) {
        std::cout << "[PHASE_INTEGRATION] Computing analysis for function: " << func_decl->name << std::endl;
        g_function_system.compute_complete_function_analysis(func_decl, all_scope_nodes_);
    }
    
    // For function expressions, we'd need to compute analysis too, but they typically
    // don't have names in the cache. This would be handled during code generation.
    
    std::cout << "[PHASE_INTEGRATION] Function dependencies computed for " 
              << all_function_declarations_.size() << " functions" << std::endl;
}

void UltraScriptPhaseIntegration::compute_all_variable_strategies() {
    std::cout << "[PHASE_INTEGRATION] Computing variable strategies" << std::endl;
    
    // Delegate to the function system
    g_function_system.compute_function_variable_strategies(all_scope_nodes_);
    
    std::cout << "[PHASE_INTEGRATION] Variable strategies computed" << std::endl;
}

void UltraScriptPhaseIntegration::prepare_for_code_generation() {
    std::cout << "[PHASE_INTEGRATION] Preparing for code generation phase" << std::endl;
    
    if (!analysis_complete_) {
        std::cout << "[PHASE_INTEGRATION] WARNING: Analysis not complete, code generation may fail" << std::endl;
        return;
    }
    
    std::cout << "[PHASE_INTEGRATION] Code generation preparation complete" << std::endl;
}

bool UltraScriptPhaseIntegration::has_function_analysis(const std::string& function_name) const {
    const auto& analysis = g_function_system.get_function_analysis(function_name);
    return !analysis.needed_parent_scopes.empty() || analysis.function_instance_size > 0;
}

const CompleteFunctionAnalysis& UltraScriptPhaseIntegration::get_function_analysis(const std::string& function_name) const {
    return g_function_system.get_function_analysis(function_name);
}

FunctionVariableStrategy UltraScriptPhaseIntegration::get_variable_strategy(const std::string& variable_name) const {
    return g_function_system.get_variable_strategy(variable_name);
}

void UltraScriptPhaseIntegration::print_analysis_summary() const {
    std::cout << "[PHASE_INTEGRATION] === ANALYSIS SUMMARY ===" << std::endl;
    std::cout << "[PHASE_INTEGRATION] Scopes analyzed: " << all_scope_nodes_.size() << std::endl;
    std::cout << "[PHASE_INTEGRATION] Function declarations: " << all_function_declarations_.size() << std::endl;
    std::cout << "[PHASE_INTEGRATION] Function expressions: " << all_function_expressions_.size() << std::endl;
    std::cout << "[PHASE_INTEGRATION] Analysis complete: " << (analysis_complete_ ? "YES" : "NO") << std::endl;
    
    // Print some details about analyzed functions
    for (const auto& func_decl : all_function_declarations_) {
        const auto& analysis = get_function_analysis(func_decl->name);
        std::cout << "[PHASE_INTEGRATION]   Function '" << func_decl->name << "': " 
                  << analysis.needed_parent_scopes.size() << " parent scopes, "
                  << analysis.function_instance_size << " bytes" << std::endl;
    }
    
    std::cout << "[PHASE_INTEGRATION] === END SUMMARY ===" << std::endl;
}

// Convenience functions for the main compilation pipeline
void initialize_function_analysis_phase(std::vector<std::unique_ptr<ASTNode>>& ast, StaticAnalyzer* analyzer) {
    g_phase_integration.perform_complete_static_analysis(ast, analyzer);
}

void initialize_function_analysis_phase(std::vector<std::unique_ptr<ASTNode>>& ast, SimpleLexicalScopeAnalyzer* analyzer) {
    g_phase_integration.perform_complete_static_analysis(ast, analyzer);
}

void finalize_function_analysis_phase() {
    g_phase_integration.prepare_for_code_generation();
    g_phase_integration.print_analysis_summary();
}
