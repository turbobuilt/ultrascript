#pragma once

#include "compiler.h"
#include "simple_lexical_scope.h"
#include <vector>
#include <memory>
#include <unordered_map>
#include <unordered_set>

// Forward declarations
class SimpleLexicalScopeAnalyzer;
struct ASTNode;
struct LexicalScopeNode;
struct FunctionDecl;
struct FunctionExpression;
struct Identifier;
struct Assignment;

/**
 * Static Analysis Pass - Phase 2 of compilation
 * 
 * This class performs a complete traversal of the AST after parsing to:
 * 1. Resolve all variable references and compute access depths
 * 2. Perform variable packing and memory layout computation
 * 3. Compute function static analysis for pure machine code generation
 * 4. Build complete lexical scope dependency graphs
 * 
 * This separates static analysis from parsing, allowing us to handle
 * forward references and complex scope relationships that can't be
 * resolved during parse time.
 */
class StaticAnalyzer {
public:
    StaticAnalyzer();
    ~StaticAnalyzer();
    
    /**
     * Main entry point: perform complete static analysis on the pure AST
     * Builds all scope information from scratch since no scope tracking was done during parsing
     */
    void analyze(std::vector<std::unique_ptr<ASTNode>>& ast);
    
private:
    // Analysis state
    std::vector<LexicalScopeNode*> scope_stack_;
    LexicalScopeNode* current_scope_;
    int current_depth_;
    
    // Scope nodes built from AST analysis
    std::unordered_map<int, std::unique_ptr<LexicalScopeNode>> depth_to_scope_node_;
    
    // Variable resolution tracking
    std::unordered_map<std::string, std::vector<VariableDeclarationInfo*>> all_variable_declarations_;
    std::unordered_set<std::string> unresolved_variables_;
    
    // Phase 1: Build complete scope hierarchy from pure AST analysis
    void build_scope_hierarchy_from_ast(const std::vector<std::unique_ptr<ASTNode>>& ast);
    
    // Phase 2: Resolve all variable references using complete scope information  
    void resolve_all_variable_references_from_ast(const std::vector<std::unique_ptr<ASTNode>>& ast);
    
    // Phase 3: Perform complete variable packing for all scopes
    void perform_complete_variable_packing_from_scopes();
    
    // Phase 4: Compute complete function static analysis
    void compute_complete_function_analysis_from_scopes();
    
    // AST traversal helpers
    void traverse_ast_node_for_scopes(ASTNode* node);
    void traverse_ast_node_for_variables(ASTNode* node);
    
    // Variable packing and function analysis helpers
    void perform_optimal_packing_for_scope(LexicalScopeNode* scope);
    void analyze_function_dependencies(LexicalScopeNode* scope);
    
    // Helper methods for scope management and variable lookup
    void enter_scope(LexicalScopeNode* scope);
    void exit_scope();
    LexicalScopeNode* find_variable_definition_scope(const std::string& variable_name);
    VariableDeclarationInfo* find_variable_declaration(const std::string& name, int access_depth);
    int compute_access_depth_between_scopes(LexicalScopeNode* definition_scope, LexicalScopeNode* access_scope);
    
    // DataType size and alignment utilities
    size_t get_datatype_size(DataType type) const;
    size_t get_datatype_alignment(DataType type) const;
    
public:
    // Public interface for code generation (compatible with SimpleLexicalScopeAnalyzer)
    LexicalScopeNode* get_scope_node_for_depth(int depth) const;
    LexicalScopeNode* get_definition_scope_for_variable(const std::string& name) const;
    VariableDeclarationInfo* get_variable_declaration_info(const std::string& name, int access_depth);
    void perform_deferred_packing_for_scope(LexicalScopeNode* scope_node);
    
private:
    
    // Debug helpers
    void print_analysis_results() const;
};
