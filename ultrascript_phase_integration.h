#pragma once

#include "function_instance_system.h"
#include "static_analyzer.h"
#include "simple_lexical_scope.h"

/**
 * Integration layer between the 3-phase system and the new function instance system
 * 
 * This class bridges:
 * - Phase 1 (Parse): Creates basic AST with minimal scope info
 * - Phase 2 (Analyze): Performs complete static analysis including function dependencies  
 * - Phase 3 (Codegen): Uses analysis results for pure ASM generation
 */
class UltraScriptPhaseIntegration {
private:
    // Phase 2 analysis results
    std::unordered_map<int, LexicalScopeNode*> all_scope_nodes_;
    std::vector<FunctionDecl*> all_function_declarations_;
    std::vector<FunctionExpression*> all_function_expressions_;
    
    // Integration state
    bool analysis_complete_ = false;
    
public:
    /**
     * Phase 2: Complete Static Analysis Integration
     * Called after parsing completes, before code generation begins
     */
    void perform_complete_static_analysis(std::vector<std::unique_ptr<ASTNode>>& ast,
                                         StaticAnalyzer* static_analyzer);
    void perform_complete_static_analysis(std::vector<std::unique_ptr<ASTNode>>& ast,
                                         SimpleLexicalScopeAnalyzer* scope_analyzer);
    
    /**
     * Phase 3: Code Generation Integration
     * Ensures analysis results are available during code generation
     */
    void prepare_for_code_generation();
    
    /**
     * Function Analysis Queries (for code generation phase)
     */
    bool has_function_analysis(const std::string& function_name) const;
    const CompleteFunctionAnalysis& get_function_analysis(const std::string& function_name) const;
    FunctionVariableStrategy get_variable_strategy(const std::string& variable_name) const;
    
    /**
     * Utility Methods
     */
    void print_analysis_summary() const;
    bool is_analysis_complete() const { return analysis_complete_; }

private:
    // AST traversal for analysis integration
    void collect_all_functions_from_ast(const std::vector<std::unique_ptr<ASTNode>>& ast);
    void collect_all_scopes_from_analyzer(StaticAnalyzer* analyzer);
    void collect_all_scopes_from_analyzer(SimpleLexicalScopeAnalyzer* analyzer);
    
    // Analysis computation orchestration  
    void compute_all_function_dependencies();
    void compute_all_variable_strategies();
    
    // AST traversal helpers
    void traverse_ast_for_functions(ASTNode* node);
    void collect_functions_from_scope(LexicalScopeNode* scope);
};

// Global integration instance
extern UltraScriptPhaseIntegration g_phase_integration;

// Convenience functions for the main compilation pipeline
void initialize_function_analysis_phase(std::vector<std::unique_ptr<ASTNode>>& ast, StaticAnalyzer* analyzer);
void initialize_function_analysis_phase(std::vector<std::unique_ptr<ASTNode>>& ast, SimpleLexicalScopeAnalyzer* analyzer);
void finalize_function_analysis_phase();
