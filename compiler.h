#pragma once

#include "minimal_parser_gc.h"
#include "codegen_forward.h"
#include "simple_lexical_scope.h"  // NEW SIMPLE LEXICAL SCOPE SYSTEM
#include <variant>
#include <memory>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <functional>
#include <fstream>
#include <string>
#include <iostream>
#include <cstring>
#include <regex>
#include <optional>
#include <chrono>
#include <stack>

// Forward declarations
struct Token;
enum class TokenType;
struct ASTNode;
struct VariableDeclarationInfo;  // Forward declaration for direct pointer access

// Forward declaration for new lexical scope system
class ExpressionOptimizer;
class SimpleLexicalScopeAnalyzer;  // Add forward declaration

// ANSI color codes for syntax highlighting
namespace Colors {
    // Check if terminal supports colors
    bool supports_color();
    
    // Color codes
    extern const char* RESET;
    extern const char* BOLD;
    extern const char* DIM;
    
    // Foreground colors
    extern const char* RED;
    extern const char* GREEN;
    extern const char* YELLOW;
    extern const char* BLUE;
    extern const char* MAGENTA;
    extern const char* CYAN;
    extern const char* WHITE;
    extern const char* GRAY;
    
    // Bright colors
    extern const char* BRIGHT_RED;
    extern const char* BRIGHT_GREEN;
    extern const char* BRIGHT_YELLOW;
    extern const char* BRIGHT_BLUE;
    extern const char* BRIGHT_MAGENTA;
    extern const char* BRIGHT_CYAN;
}

// Fast syntax highlighter for error context
class SyntaxHighlighter {
private:
    bool use_colors;
    
public:
    SyntaxHighlighter();
    std::string highlight_line(const std::string& line) const;
    
private:
    std::string colorize_token(const std::string& token, TokenType type) const;
    TokenType classify_token(const std::string& token) const;
    bool is_keyword(const std::string& token) const;
    bool is_number(const std::string& token) const;
    bool is_string_delimiter(char ch) const;
};

// Enhanced error reporting system
class ErrorReporter {
private:
    std::string source_code;
    std::string file_path;
    SyntaxHighlighter highlighter;
    
public:
    ErrorReporter(const std::string& source, const std::string& file = "") 
        : source_code(source), file_path(file) {}
    
    void report_error(const std::string& message, int line, int column) const;
    void report_parse_error(const std::string& message, const Token& token) const;
    void report_lexer_error(const std::string& message, int line, int column, char unexpected_char) const;
    
private:
    std::string get_line_content(int line_number) const;
    std::string format_error_context(const std::string& message, int line, int column, const std::string& line_content, char problematic_char = '\0') const;
};

enum class TokenType {
    IDENTIFIER, NUMBER, STRING, TEMPLATE_LITERAL, BOOLEAN, REGEX,
    FUNCTION, GO, AWAIT, LET, VAR, CONST,
    IF, FOR, WHILE, RETURN,
    SWITCH, CASE, DEFAULT, BREAK,
    TRY, CATCH, THROW, FINALLY,  // Added for exception handling
    IMPORT, EXPORT, FROM, AS, DEFAULT_EXPORT,
    TENSOR, NEW, ARRAY,
    CLASS, EXTENDS, SUPER, THIS, CONSTRUCTOR,
    PUBLIC, PRIVATE, PROTECTED, STATIC,
    EACH, IN, PIPE,  // Added for for-each syntax
    OPERATOR,  // Added for operator overloading
    FREE, SHALLOW,  // Added for memory management
    LPAREN, RPAREN, LBRACE, RBRACE, LBRACKET, RBRACKET,
    SLICE_BRACKET,  // Added for [:] slice operator
    SEMICOLON, COMMA, DOT, COLON, QUESTION, ARROW,
    ASSIGN, PLUS, MINUS, MULTIPLY, DIVIDE, MODULO, POWER,
    EQUAL, NOT_EQUAL, STRICT_EQUAL, LESS, GREATER, LESS_EQUAL, GREATER_EQUAL,
    AND, OR, NOT,
    PLUS_ASSIGN, MINUS_ASSIGN, MULTIPLY_ASSIGN, DIVIDE_ASSIGN,
    INCREMENT, DECREMENT,
    EOF_TOKEN
};

enum class DataType {
    ANY, VOID,
    INT8, INT16, INT32, INT64,
    UINT8, UINT16, UINT32, UINT64,
    FLOAT32, FLOAT64,
    BOOLEAN, STRING, REGEX,
    TENSOR, PROMISE, FUNCTION, SLICE, ARRAY,
    CLASS_INSTANCE,  // For class instances
    RUNTIME_OBJECT,  // For runtime.x property access optimization
    UNKNOWN = ANY     // UNKNOWN is an alias for ANY (untyped variables)
};

struct Token {
    TokenType type;
    std::string value;
    int line, column;
};

// Forward declarations
struct ExpressionNode;
struct OperatorOverloadDecl;
class GoTSCompiler;

struct Variable {
    std::string name;
    DataType type;
    int64_t stack_offset;
    bool is_global;
    bool is_mutable;
    bool is_static = false;
    std::string class_name;  // For CLASS_INSTANCE type, stores the class name
    std::shared_ptr<ExpressionNode> default_value;  // Default value for class fields
};

struct Function {
    std::string name;
    DataType return_type;
    std::vector<Variable> parameters;
    std::vector<uint8_t> machine_code;
    int64_t stack_size;
    int parameter_count = 0;
    bool is_method = false;
    bool is_unmanaged = false;
    bool is_inline = false;
    bool is_operator_overload = false;
    uint64_t address = 0;
};

struct OperatorOverload {
    TokenType operator_type;
    std::vector<Variable> parameters;
    DataType return_type;
    std::vector<uint8_t> machine_code;
    std::string function_name;  // Generated name for the operator function
    
    OperatorOverload(TokenType op, const std::vector<Variable>& params, DataType ret_type)
        : operator_type(op), parameters(params), return_type(ret_type) {}
};

struct ClassInfo {
    std::string name;
    std::vector<std::string> parent_classes;
    std::vector<Variable> fields;
    std::unordered_map<std::string, Function> methods;
    std::unordered_map<TokenType, std::vector<OperatorOverload>> operator_overloads;  // Multiple overloads per operator
    Function* constructor;
    int64_t instance_size;  // Total size needed for an instance
    
    ClassInfo() : constructor(nullptr), instance_size(0) {}
    ClassInfo(const std::string& n) : name(n), constructor(nullptr), instance_size(0) {}
};

enum class Backend {
    X86_64
};

// NOTE: CodeGenerator interface is now provided by codegen_forward.h
// NOTE: X86CodeGen class is now provided by the compatibility layer
// See x86_codegen_compat.h for the abstraction-based implementation

class TypeInference {
public:
    // LEXICAL SCOPE SYSTEM: Storage types for escape analysis
    enum class VariableStorage {
        STACK,           // Variable stays on stack - no escape
        HEAP_LEXICAL     // Variable escapes - stored in heap lexical scope
    };

private:
    std::unordered_map<std::string, DataType> variable_types;
    std::unordered_map<std::string, uint32_t> variable_class_type_ids;  // For CLASS_INSTANCE variables - use type IDs instead of names
    std::unordered_map<std::string, std::string> variable_class_names;  // For CLASS_INSTANCE variables - class names for direct destructor calls
    std::unordered_map<std::string, DataType> variable_array_element_types;  // For ARRAY variables
    std::unordered_map<std::string, int64_t> variable_offsets;
    int64_t current_offset = -8; // Start at -8 (RBP-8)
    
    // Function parameter tracking for keyword arguments
    std::unordered_map<std::string, std::vector<std::string>> function_param_names;
    
    // Assignment context tracking for type-aware array creation
    DataType current_assignment_target_type = DataType::ANY;
    DataType current_assignment_array_element_type = DataType::ANY;  // For [type] arrays
    DataType current_element_type_context = DataType::ANY;  // For array element type inference
    DataType current_property_assignment_type = DataType::ANY;  // For property assignment type inference
    
    // Current class context for 'this' handling
    std::string current_class_name;
    
    // LEXICAL SCOPE SYSTEM: Escape analysis for stack vs heap allocation
    std::unordered_map<std::string, VariableStorage> variable_storage;  // Track storage location
    std::unordered_set<std::string> escaped_variables;                  // Variables that escape current scope
    std::vector<std::unordered_set<std::string>> scope_stack;           // Track variables per scope level
    std::unordered_map<std::string, int> variable_scope_depth;          // Track which scope level each variable was declared in
    int current_scope_depth = 0;                                        // Current nesting level
    bool inside_function_call = false;                                  // Track if we're analyzing function call arguments
    bool inside_callback = false;                                       // Track if we're analyzing callback functions
    bool inside_goroutine = false;                                      // Track if we're analyzing goroutine functions
    
    // Legacy lexical scope analysis methods removed - now handled by SimpleLexicalScopeAnalyzer
    std::string current_function_being_analyzed_;  // Track current function for analysis
    std::stack<std::string> function_context_stack_;  // Track nested function contexts

public:
    // Constructor and destructor need to be explicitly declared due to unique_ptr with forward declaration
    TypeInference();
    ~TypeInference();
    
public:
    DataType infer_type(const std::string& expression);
    DataType get_cast_type(DataType t1, DataType t2);
    bool needs_casting(DataType from, DataType to);
    void set_variable_type(const std::string& name, DataType type);
    void set_variable_class_type(const std::string& name, uint32_t class_type_id);
    void set_variable_class_name(const std::string& name, const std::string& class_name);
    DataType get_variable_type(const std::string& name);
    uint32_t get_variable_class_type_id(const std::string& name);
    std::string get_variable_class_name(const std::string& name);
    
    // Array element type tracking for typed arrays
    void set_variable_array_element_type(const std::string& name, DataType element_type);
    DataType get_variable_array_element_type(const std::string& name);
    
    // Variable storage management
    void set_variable_offset(const std::string& name, int64_t offset);
    int64_t get_variable_offset(const std::string& name) const;
    int64_t allocate_variable(const std::string& name, DataType type);
    bool variable_exists(const std::string& name);
    void enter_scope();
    void exit_scope();
    void reset_for_function();
    void reset_for_function_with_params(int param_count);
    
    // Access to all variables for scope cleanup
    const std::unordered_map<std::string, DataType>& get_all_variable_types() const { return variable_types; }
    const std::unordered_map<std::string, int64_t>& get_all_variable_offsets() const { return variable_offsets; }
    const std::unordered_map<std::string, std::string>& get_all_variable_class_names() const { return variable_class_names; }
    
    // Assignment context tracking for type-aware array creation
    void set_current_assignment_target_type(DataType type);
    DataType get_current_assignment_target_type() const;
    void set_current_assignment_array_element_type(DataType element_type);
    DataType get_current_assignment_array_element_type() const;
    void clear_assignment_context();
    
    // Element type context for array literals
    void set_current_element_type_context(DataType element_type);
    DataType get_current_element_type_context() const;
    void clear_element_type_context();
    
    // Property assignment type context for typed property assignment
    void set_current_property_assignment_type(DataType property_type);
    DataType get_current_property_assignment_type() const;
    void clear_property_assignment_context();
    
    // Current class context for 'this' handling
    void set_current_class_context(const std::string& class_name);
    std::string get_current_class_context() const;
    void clear_current_class_context();
    
    // Function parameter tracking for keyword arguments
    void register_function_params(const std::string& func_name, const std::vector<std::string>& param_names);
    std::vector<std::string> get_function_params(const std::string& func_name) const;
    
    // Operator overloading type inference
    DataType infer_operator_result_type(const std::string& class_name, TokenType operator_type, 
                                       const std::vector<DataType>& operand_types);
    bool can_use_operator_overload(const std::string& class_name, TokenType operator_type, 
                                  const std::vector<DataType>& operand_types);
    
    // Enhanced operator overloading type inference for [] operator
    DataType infer_operator_index_type(const std::string& class_name, const std::string& index_expression);
    DataType infer_operator_index_type(uint32_t class_type_id, const std::string& index_expression);  // Type ID version
    bool is_deterministic_expression(const std::string& expression);
    bool is_array_comparison_expression(const std::string& expression);
    DataType infer_expression_type(const std::string& expression);
    DataType infer_complex_expression_type(const std::string& expression);
    DataType get_best_numeric_operator_type(const std::string& class_name, const std::string& numeric_literal);
    DataType get_best_numeric_operator_type(uint32_t class_type_id, const std::string& numeric_literal);  // Type ID version
    TokenType string_to_operator_token(const std::string& op_str);
    
    // LEXICAL SCOPE SYSTEM: Escape analysis interface
    void enter_lexical_scope();                                            // Push new scope level
    void exit_lexical_scope();                                             // Pop scope, mark escaped variables
    void mark_variable_used(const std::string& name);                      // Variable referenced - check if escapes
    void mark_variable_passed_to_function(const std::string& name);        // Variable passed as argument - escapes
    void mark_variable_passed_to_callback(const std::string& name);        // Variable in callback - escapes  
    void mark_variable_in_goroutine(const std::string& name);              // Variable in goroutine - escapes
    void set_analyzing_function_call(bool analyzing);                      // Set function call analysis mode
    void set_analyzing_callback(bool analyzing);                           // Set callback analysis mode
    void set_analyzing_goroutine(bool analyzing);                          // Set goroutine analysis mode
    
    // Query escape analysis results
    bool variable_escapes(const std::string& name) const;                  // True if variable needs heap allocation
    VariableStorage get_variable_storage(const std::string& name) const;   // Get storage location
    std::vector<std::string> get_escaped_variables_in_scope() const;       // Get all escaped vars in current scope
    std::vector<std::string> get_stack_variables_in_scope() const;         // Get all stack vars in current scope
    int get_variable_scope_depth(const std::string& name) const;           // Get declaration scope depth
    
    // Updated variable access methods with function context
    bool variable_escapes_in_function(const std::string& function_name, const std::string& var_name) const;
    int64_t get_variable_offset_in_function(const std::string& function_name, const std::string& var_name) const;
    
    // HIGH-PERFORMANCE REGISTER-BASED SCOPE ACCESS METHODS
    int get_register_for_scope_level(const std::string& function_name, int scope_level) const;
    std::unordered_set<int> get_used_scope_registers(const std::string& function_name) const;
    bool needs_stack_fallback_for_scopes(const std::string& function_name) const;
    
    // DEBUG AND DEVELOPMENT METHODS
    void debug_print_all_variables() const;                                // Print all variables for debugging
    void inherit_escaped_variables_from_parent(const TypeInference& parent_types);  // Copy escaped variables from parent
    void import_escaped_variables_from_gc_system();                        // Import escaped variables from GC system
    
    // CONTEXT TRACKING FOR CODE GENERATION
    void push_function_context(const std::string& function_name);
    void pop_function_context();
    std::string get_current_function_context() const;
    
    // For debugging escape analysis
    void debug_print_escape_info() const;
    
    // Expression string extraction helpers
    std::string extract_expression_string(ExpressionNode* node);
    std::string token_type_to_string(TokenType token);
    bool is_numeric_literal(const std::string& expression);
    
    // LEXICAL SCOPE ADDRESS TRACKER INTEGRATION
    // Methods to access LexicalScopeAddressTracker via compiler context
    void set_compiler_context(class GoTSCompiler* compiler);  // Forward declaration
    int determine_variable_scope_level(const std::string& var_name, const std::string& accessing_function) const;
    void register_variable_declaration_in_function(const std::string& var_name, const std::string& declaring_function);
    std::string generate_variable_access_asm_with_static_analysis(const std::string& var_name, const std::string& accessing_function) const;

private:
    class GoTSCompiler* compiler_context_ = nullptr;  // Reference to compiler for accessing LexicalScopeAddressTracker
};

// Forward declaration for scope context
struct ScopeContext {
    // Current scope information from SimpleLexicalScopeAnalyzer
    LexicalScopeNode* current_scope = nullptr;
    
    // Scope register management
    // r15 always points to current scope
    // r12, r13, r14 point to parent scopes in order of frequency
    std::unordered_map<int, int> scope_depth_to_register;  // depth -> register (12,13,14)
    std::vector<int> available_scope_registers = {12, 13, 14};
    
    // Stack management for deep nesting (when more than 3 parent scopes)
    std::vector<int> stack_stored_scopes;  // scope depths stored on stack
    
    // Current function context for variable resolution
    std::string current_function_name;
    
    // Reference to the lexical scope analyzer for variable lookup
    SimpleLexicalScopeAnalyzer* scope_analyzer = nullptr;
    
    // Type information extracted from parse phase
    std::unordered_map<std::string, DataType> variable_types;
    std::unordered_map<std::string, DataType> variable_array_element_types;
    std::unordered_map<std::string, std::string> variable_class_names;
    
    // Assignment context tracking (similar to old TypeInference)
    DataType current_assignment_target_type = DataType::ANY;
    DataType current_assignment_array_element_type = DataType::ANY;
    DataType current_element_type_context = DataType::ANY;
    DataType current_property_assignment_type = DataType::ANY;
    
    // Current class context for 'this' handling
    std::string current_class_name;
};

struct ASTNode {
    DataType result_type = DataType::ANY;  // Type of value this node produces
    virtual ~ASTNode() = default;
    virtual void generate_code(CodeGenerator& gen) = 0;  // New scope-aware interface
};

// Include dependency and variable declaration structures for scope analysis
#include <unordered_set>

// Forward declarations - structures defined in simple_lexical_scope.h
struct ScopeDependency;
struct VariableDeclarationInfo;

// Comprehensive lexical scope node containing all scope analysis information
struct LexicalScopeNode : ASTNode {
    // Basic scope information
    int scope_depth;                                           // Absolute depth of this scope
    std::unordered_set<std::string> declared_variables;       // Variables declared in THIS scope
    
    // Advanced dependency tracking (moved from LexicalScopeInfo)
    std::vector<ScopeDependency> self_dependencies;           // Variables accessed in this scope from outer scopes
    std::vector<ScopeDependency> descendant_dependencies;     // Variables needed by all descendant scopes
    
    // Priority-sorted scope levels (backend-agnostic, computed after analysis)
    std::vector<int> priority_sorted_parent_scopes;           // Scope levels/depths in order of access frequency
    
    // Variable packing and memory layout (NEW)
    std::unordered_map<std::string, size_t> variable_offsets; // identifier -> byte offset in scope frame
    size_t total_scope_frame_size = 0;                        // Total size of all variables in this scope
    std::vector<std::string> packed_variable_order;           // Variables in memory layout order
    
    // Legacy compatibility
    std::unordered_map<std::string, int> variable_access_depths; // var_name -> definition depth
    
    LexicalScopeNode(int depth) : scope_depth(depth) {}
    
    void declare_variable(const std::string& name) {
        declared_variables.insert(name);
    }
    
    void record_variable_access(const std::string& name, int definition_depth) {
        variable_access_depths[name] = definition_depth;
    }
    
    // Add dependency tracking methods
    void add_self_dependency(const std::string& var_name, int def_depth, size_t access_count = 1);
    void add_descendant_dependency(const std::string& var_name, int def_depth, size_t access_count = 1);
    void set_priority_sorted_scopes(const std::vector<int>& scopes) { 
        priority_sorted_parent_scopes = scopes; 
    }
    
    void generate_code(CodeGenerator& gen) override {
        // LexicalScopeNode doesn't generate code directly
        // It contains scope analysis information used by code generation
    }
};

struct ExpressionNode : ASTNode {
    DataType result_type = DataType::ANY;
};

struct NumberLiteral : ExpressionNode {
    double value;
    NumberLiteral(double v) : value(v) {}
    void generate_code(CodeGenerator& gen) override;
};

struct StringLiteral : ExpressionNode {
    std::string value;
    StringLiteral(const std::string& v) : value(v) {}
    void generate_code(CodeGenerator& gen) override;
};

struct RegexLiteral : ExpressionNode {
    std::string pattern;
    std::string flags;
    RegexLiteral(const std::string& p, const std::string& f = "") : pattern(p), flags(f) {}
    void generate_code(CodeGenerator& gen) override;
};

struct Identifier : ExpressionNode {
    std::string name;
    int definition_depth;     // Lexical scope depth where variable was defined (legacy)
    int access_depth;         // Lexical scope depth where variable is being accessed (legacy)
    
    // NEW: raw pointers to lexical scope nodes for safe access
    LexicalScopeNode* definition_scope;    // Scope where variable was defined
    LexicalScopeNode* access_scope;        // Scope where variable is being accessed
    
    // ULTRA-FAST: Direct pointer to variable declaration info (zero lookup overhead)
    VariableDeclarationInfo* variable_declaration_info;  // Direct pointer to the variable's declaration info
    
    Identifier(const std::string& n, int def_depth = -1, int acc_depth = -1) 
        : name(n), definition_depth(def_depth), access_depth(acc_depth), 
          definition_scope(nullptr), access_scope(nullptr), variable_declaration_info(nullptr) {}
        
    // Enhanced constructor with raw pointers to scope nodes
    Identifier(const std::string& n, LexicalScopeNode* def_scope, LexicalScopeNode* acc_scope,
               int def_depth = -1, int acc_depth = -1) 
        : name(n), definition_depth(def_depth), access_depth(acc_depth),
          definition_scope(def_scope), access_scope(acc_scope), variable_declaration_info(nullptr) {}
          
    // Ultra-fast constructor with direct variable declaration pointer
    Identifier(const std::string& n, VariableDeclarationInfo* var_info,
               LexicalScopeNode* def_scope = nullptr, LexicalScopeNode* acc_scope = nullptr) 
        : name(n), definition_depth(var_info ? var_info->depth : -1), access_depth(-1),
          definition_scope(def_scope), access_scope(acc_scope), variable_declaration_info(var_info) {}
    
    void generate_code(CodeGenerator& gen) override;
};

struct BinaryOp : ExpressionNode {
    std::unique_ptr<ExpressionNode> left, right;
    TokenType op;
    BinaryOp(std::unique_ptr<ExpressionNode> l, TokenType o, std::unique_ptr<ExpressionNode> r)
        : left(std::move(l)), right(std::move(r)), op(o) {}
    void generate_code(CodeGenerator& gen) override;
};

struct TernaryOperator : ExpressionNode {
    std::unique_ptr<ExpressionNode> condition, true_expr, false_expr;
    TernaryOperator(std::unique_ptr<ExpressionNode> cond, std::unique_ptr<ExpressionNode> true_val, std::unique_ptr<ExpressionNode> false_val)
        : condition(std::move(cond)), true_expr(std::move(true_val)), false_expr(std::move(false_val)) {}
    void generate_code(CodeGenerator& gen) override;
};

struct FunctionCall : ExpressionNode {
    std::string name;
    std::vector<std::unique_ptr<ExpressionNode>> arguments;
    std::vector<std::string> keyword_names;  // Names for keyword arguments (empty string for positional)
    bool is_goroutine = false;
    bool is_awaited = false;
    FunctionCall(const std::string& n) : name(n) {}
    void generate_code(CodeGenerator& gen) override;
};

struct FunctionExpression : ExpressionNode {
    std::string name;  // Optional name for debugging/recursion
    std::vector<Variable> parameters;
    DataType return_type = DataType::ANY;
    std::vector<std::unique_ptr<ASTNode>> body;
    bool is_goroutine = false;
    bool is_awaited = false;
    
    // NEW: For three-phase compilation system
    std::string compilation_assigned_name_;  // Name assigned during Phase 1
    
    // Lexical scope information for this function
    std::unique_ptr<LexicalScopeNode> lexical_scope;
    
    FunctionExpression() : name("") {}
    FunctionExpression(const std::string& n) : name(n) {}
    void generate_code(CodeGenerator& gen) override;
    void compile_function_body(CodeGenerator& gen, const std::string& func_name);
    
    // NEW: For three-phase compilation system
    void set_compilation_assigned_name(const std::string& assigned_name) {
        compilation_assigned_name_ = assigned_name;
    }
};

struct ArrowFunction : ExpressionNode {
    std::vector<Variable> parameters;
    DataType return_type = DataType::ANY;
    std::vector<std::unique_ptr<ASTNode>> body;
    bool is_single_expression = false;  // true for: x => x + 1, false for: x => { return x + 1; }
    std::unique_ptr<ExpressionNode> expression;  // for single expression arrows
    bool is_goroutine = false;
    bool is_awaited = false;
    
    // NEW: For three-phase compilation system  
    std::string compilation_assigned_name_;  // Name assigned during Phase 1
    
    // Lexical scope information for this function (even single expression arrows create scope)
    std::unique_ptr<LexicalScopeNode> lexical_scope;
    
    ArrowFunction() {}
    void generate_code(CodeGenerator& gen) override;
    void compile_function_body(CodeGenerator& gen, const std::string& func_name);
    
    // NEW: For three-phase compilation system
    void set_compilation_assigned_name(const std::string& assigned_name) {
        compilation_assigned_name_ = assigned_name;
    }
};

struct MethodCall : ExpressionNode {
    std::string object_name;
    std::string method_name;
    std::vector<std::unique_ptr<ExpressionNode>> arguments;
    std::vector<std::string> keyword_names;  // Names for keyword arguments (empty string for positional)
    bool is_goroutine = false;
    bool is_awaited = false;
    MethodCall(const std::string& obj, const std::string& method) 
        : object_name(obj), method_name(method) {}
    void generate_code(CodeGenerator& gen) override;
};

struct ExpressionMethodCall : ExpressionNode {
    std::unique_ptr<ExpressionNode> object;
    std::string method_name;
    std::vector<std::unique_ptr<ExpressionNode>> arguments;
    std::vector<std::string> keyword_names;  // Names for keyword arguments (empty string for positional)
    bool is_goroutine = false;
    bool is_awaited = false;
    ExpressionMethodCall(std::unique_ptr<ExpressionNode> obj, const std::string& method) 
        : object(std::move(obj)), method_name(method) {}
    void generate_code(CodeGenerator& gen) override;
};

struct ArrayLiteral : ExpressionNode {
    std::vector<std::unique_ptr<ExpressionNode>> elements;
    ArrayLiteral() {}
    void generate_code(CodeGenerator& gen) override;
};

struct ObjectLiteral : ExpressionNode {
    std::vector<std::pair<std::string, std::unique_ptr<ExpressionNode>>> properties;
    ObjectLiteral() {}
    void generate_code(CodeGenerator& gen) override;
};

struct TypedArrayLiteral : ExpressionNode {
    std::vector<std::unique_ptr<ExpressionNode>> elements;
    DataType array_type;
    TypedArrayLiteral(DataType type) : array_type(type) {}
    void generate_code(CodeGenerator& gen) override;
};

struct SliceExpression : ExpressionNode {
    int64_t start;
    int64_t end;
    int64_t step;
    bool start_specified;
    bool end_specified;
    bool step_specified;
    
    SliceExpression() : start(0), end(-1), step(1), start_specified(false), end_specified(false), step_specified(false) {}
    SliceExpression(int64_t s, int64_t e, int64_t st) : start(s), end(e), step(st), start_specified(true), end_specified(true), step_specified(true) {}
    void generate_code(CodeGenerator& gen) override;
};

struct ArrayAccess : ExpressionNode {
    std::unique_ptr<ExpressionNode> object;
    std::unique_ptr<ExpressionNode> index;
    std::vector<std::unique_ptr<SliceExpression>> slices;  // For multi-dimensional slicing
    bool is_slice_expression = false;  // True if index contains colons, ellipsis, etc.
    std::string slice_expression;       // Raw string representation for complex indexing
    ArrayAccess(std::unique_ptr<ExpressionNode> obj, std::unique_ptr<ExpressionNode> idx)
        : object(std::move(obj)), index(std::move(idx)) {}
    void generate_code(CodeGenerator& gen) override;
};

struct OperatorCall : ExpressionNode {
    std::unique_ptr<ExpressionNode> left_operand;
    std::unique_ptr<ExpressionNode> right_operand;  // For binary operators, null for unary
    TokenType operator_type;
    std::string class_name;  // Class that defines the operator
    OperatorCall(std::unique_ptr<ExpressionNode> left, TokenType op, std::unique_ptr<ExpressionNode> right, const std::string& cls)
        : left_operand(std::move(left)), right_operand(std::move(right)), operator_type(op), class_name(cls) {}
    void generate_code(CodeGenerator& gen) override;
};

struct Assignment : ExpressionNode {
    std::string variable_name;
    std::unique_ptr<ExpressionNode> value;
    DataType declared_type = DataType::ANY;
    DataType declared_element_type = DataType::ANY;  // For [element_type] arrays
    int definition_depth = -1;     // Lexical scope depth where variable was defined (legacy)
    int assignment_depth = -1;     // Lexical scope depth where assignment occurs (legacy)
    
    // NEW: raw pointers to lexical scope nodes for safe access
    LexicalScopeNode* definition_scope;    // Scope where variable was defined
    LexicalScopeNode* assignment_scope;    // Scope where assignment occurs
    
    // ULTRA-FAST: Direct pointer to variable declaration info (zero lookup overhead)
    VariableDeclarationInfo* variable_declaration_info;  // Direct pointer to the variable's declaration info
    
    // ES6 declaration kind for proper block scoping
    enum DeclarationKind {
        VAR,    // Function-scoped, hoisted
        LET,    // Block-scoped, not hoisted  
        CONST   // Block-scoped, not hoisted, immutable
    };
    DeclarationKind declaration_kind = VAR;  // Default to var for compatibility
    
    Assignment(const std::string& name, std::unique_ptr<ExpressionNode> val, DeclarationKind kind = VAR)
        : variable_name(name), value(std::move(val)), declaration_kind(kind), 
          definition_scope(nullptr), assignment_scope(nullptr), variable_declaration_info(nullptr) {}
    void generate_code(CodeGenerator& gen) override;
};

struct PropertyAssignment : ExpressionNode {
    std::string object_name;
    std::string property_name;
    std::unique_ptr<ExpressionNode> value;
    PropertyAssignment(const std::string& obj, const std::string& prop, std::unique_ptr<ExpressionNode> val)
        : object_name(obj), property_name(prop), value(std::move(val)) {}
    void generate_code(CodeGenerator& gen) override;
};

struct ExpressionPropertyAssignment : ExpressionNode {
    std::unique_ptr<ExpressionNode> object;
    std::string property_name;
    std::unique_ptr<ExpressionNode> value;
    ExpressionPropertyAssignment(std::unique_ptr<ExpressionNode> obj, const std::string& prop, std::unique_ptr<ExpressionNode> val)
        : object(std::move(obj)), property_name(prop), value(std::move(val)) {}
    void generate_code(CodeGenerator& gen) override;
};

struct PostfixIncrement : ExpressionNode {
    std::string variable_name;
    PostfixIncrement(const std::string& name) : variable_name(name) {}
    void generate_code(CodeGenerator& gen) override;
};

struct PostfixDecrement : ExpressionNode {
    std::string variable_name;
    PostfixDecrement(const std::string& name) : variable_name(name) {}
    void generate_code(CodeGenerator& gen) override;
};

struct FunctionDecl : ASTNode {
    std::string name;
    std::vector<Variable> parameters;
    DataType return_type = DataType::ANY;
    std::vector<std::unique_ptr<ASTNode>> body;
    
    // Lexical scope information for this function
    std::unique_ptr<LexicalScopeNode> lexical_scope;
    
    FunctionDecl(const std::string& n) : name(n) {}
    void generate_code(CodeGenerator& gen) override;
};

struct IfStatement : ASTNode {
    std::unique_ptr<ExpressionNode> condition;
    std::vector<std::unique_ptr<ASTNode>> then_body;
    std::vector<std::unique_ptr<ASTNode>> else_body;
    
    // Lexical scope information (if/else bodies can create scopes even without {})
    std::unique_ptr<LexicalScopeNode> then_lexical_scope;
    std::unique_ptr<LexicalScopeNode> else_lexical_scope;
    
    void generate_code(CodeGenerator& gen) override;
};

struct ForLoop : ASTNode {
    std::unique_ptr<ASTNode> init;
    std::unique_ptr<ExpressionNode> condition;
    std::unique_ptr<ASTNode> update;
    std::vector<std::unique_ptr<ASTNode>> body;
    
    // ES6 for-loop scoping information
    Assignment::DeclarationKind init_declaration_kind = Assignment::VAR;  // Default to var
    bool creates_block_scope = false;  // true for let/const loops
    
    // Lexical scope information for for-loop (creates scope for let/const in init)
    std::unique_ptr<LexicalScopeNode> lexical_scope;
    
    void generate_code(CodeGenerator& gen) override;
};

struct ForEachLoop : ASTNode {
    std::string index_var_name;   // index for arrays, key for objects
    std::string value_var_name;   // value variable name
    std::unique_ptr<ExpressionNode> iterable;  // the array/object to iterate over
    std::vector<std::unique_ptr<ASTNode>> body;
    
    // Lexical scope information for for-each loop
    std::unique_ptr<LexicalScopeNode> lexical_scope;
    
    ForEachLoop(const std::string& index_name, const std::string& value_name)
        : index_var_name(index_name), value_var_name(value_name) {}
    void generate_code(CodeGenerator& gen) override;
};

struct ForInStatement : ASTNode {
    std::string key_var_name;     // the key variable name
    std::unique_ptr<ExpressionNode> object;  // the object to iterate over
    std::vector<std::unique_ptr<ASTNode>> body;
    
    // Lexical scope information for for-in loop
    std::unique_ptr<LexicalScopeNode> lexical_scope;
    
    ForInStatement(const std::string& key_name)
        : key_var_name(key_name) {}
    void generate_code(CodeGenerator& gen) override;
};

struct WhileLoop : ASTNode {
    std::unique_ptr<ExpressionNode> condition;   // while condition
    std::vector<std::unique_ptr<ASTNode>> body;  // loop body
    
    // ES6 while-loop scoping - while loops create block scope in ES6
    bool creates_block_scope = true;  // while loops always create block scope for let/const
    
    // Lexical scope information for while loop
    std::unique_ptr<LexicalScopeNode> lexical_scope;
    
    WhileLoop(std::unique_ptr<ExpressionNode> cond) : condition(std::move(cond)) {}
    void generate_code(CodeGenerator& gen) override;
};

struct ReturnStatement : ASTNode {
    std::unique_ptr<ExpressionNode> value;
    ReturnStatement(std::unique_ptr<ExpressionNode> val) : value(std::move(val)) {}
    void generate_code(CodeGenerator& gen) override;
};

struct BreakStatement : ASTNode {
    BreakStatement() {}
    void generate_code(CodeGenerator& gen) override;
};

struct FreeStatement : ASTNode {
    std::unique_ptr<ExpressionNode> target;
    bool is_shallow;
    
    FreeStatement(std::unique_ptr<ExpressionNode> tgt, bool shallow = false) 
        : target(std::move(tgt)), is_shallow(shallow) {}
    void generate_code(CodeGenerator& gen) override;
};

// Exception handling AST nodes
struct ThrowStatement : ASTNode {
    std::unique_ptr<ExpressionNode> value;
    
    ThrowStatement(std::unique_ptr<ExpressionNode> val) : value(std::move(val)) {}
    void generate_code(CodeGenerator& gen) override;
};

struct CatchClause : ASTNode {
    std::string parameter;  // catch parameter name (e.g., "error" in catch(error))
    std::vector<std::unique_ptr<ASTNode>> body;
    
    CatchClause(const std::string& param) : parameter(param) {}
    void generate_code(CodeGenerator& gen) override;
};

struct TryStatement : ASTNode {
    std::vector<std::unique_ptr<ASTNode>> try_body;
    std::unique_ptr<CatchClause> catch_clause;  // Optional
    std::vector<std::unique_ptr<ASTNode>> finally_body;  // Optional
    
    TryStatement() {}
    void generate_code(CodeGenerator& gen) override;
};

// Block statement for standalone blocks { }
struct BlockStatement : ASTNode {
    std::vector<std::unique_ptr<ASTNode>> body;
    bool creates_scope = true;  // Block statements always create new scope
    
    // Lexical scope information for this block
    std::unique_ptr<LexicalScopeNode> lexical_scope;
    
    BlockStatement() {}
    void generate_code(CodeGenerator& gen) override;
};

struct CaseClause : ASTNode {
    std::unique_ptr<ExpressionNode> value;  // nullptr for default case
    std::vector<std::unique_ptr<ASTNode>> body;
    std::unique_ptr<BlockStatement> block_body;  // Optional for case 0: { ... } syntax
    bool is_default = false;
    
    CaseClause(std::unique_ptr<ExpressionNode> val) : value(std::move(val)) {}
    CaseClause() : is_default(true) {}  // Default constructor for default case
    void generate_code(CodeGenerator& gen) override;
};

struct SwitchStatement : ASTNode {
    std::unique_ptr<ExpressionNode> discriminant;
    std::vector<std::unique_ptr<CaseClause>> cases;
    
    SwitchStatement(std::unique_ptr<ExpressionNode> disc) : discriminant(std::move(disc)) {}
    void generate_code(CodeGenerator& gen) override;
};

// Import/Export AST Nodes
struct ImportSpecifier {
    std::string imported_name;  // Name in source module
    std::string local_name;     // Name in current module (for "as" renaming)
    bool is_default = false;
    
    ImportSpecifier(const std::string& name) : imported_name(name), local_name(name) {}
    ImportSpecifier(const std::string& imported, const std::string& local) 
        : imported_name(imported), local_name(local) {}
};

struct ImportStatement : ASTNode {
    std::vector<ImportSpecifier> specifiers;
    std::string module_path;
    bool is_namespace_import = false;  // import * as name from "module"
    std::string namespace_name;        // For namespace imports
    
    ImportStatement(const std::string& path) : module_path(path) {}
    void generate_code(CodeGenerator& gen) override;
};

struct ExportSpecifier {
    std::string local_name;     // Name in current module
    std::string exported_name;  // Name in export (for "as" renaming)
    
    ExportSpecifier(const std::string& name) : local_name(name), exported_name(name) {}
    ExportSpecifier(const std::string& local, const std::string& exported)
        : local_name(local), exported_name(exported) {}
};

struct ExportStatement : ASTNode {
    std::vector<ExportSpecifier> specifiers;
    std::unique_ptr<ASTNode> declaration;  // For export declarations
    bool is_default = false;
    
    ExportStatement() {}
    void generate_code(CodeGenerator& gen) override;
};

struct PropertyAccess : ExpressionNode {
    std::string object_name;
    std::string property_name;
    PropertyAccess(const std::string& obj, const std::string& prop) 
        : object_name(obj), property_name(prop) {}
    void generate_code(CodeGenerator& gen) override;
};

struct ExpressionPropertyAccess : ExpressionNode {
    std::unique_ptr<ExpressionNode> object;
    std::string property_name;
    ExpressionPropertyAccess(std::unique_ptr<ExpressionNode> obj, const std::string& prop) 
        : object(std::move(obj)), property_name(prop) {}
    void generate_code(CodeGenerator& gen) override;
};

struct ThisExpression : ExpressionNode {
    ThisExpression() {}
    void generate_code(CodeGenerator& gen) override;
};

struct NewExpression : ExpressionNode {
    std::string class_name;
    std::vector<std::unique_ptr<ExpressionNode>> arguments;
    bool is_dart_style = false; // For new Person{name: "bob"} syntax
    std::vector<std::pair<std::string, std::unique_ptr<ExpressionNode>>> dart_args;
    NewExpression(const std::string& name) : class_name(name) {}
    void generate_code(CodeGenerator& gen) override;
};

struct ConstructorDecl : ASTNode {
    std::string class_name;
    std::vector<Variable> parameters;
    std::vector<std::unique_ptr<ASTNode>> body;
    ConstructorDecl(const std::string& cn) : class_name(cn) {}
    void generate_code(CodeGenerator& gen) override;
    
    // Helper to get class info during code generation
    static GoTSCompiler* current_compiler_context;
    static void set_compiler_context(GoTSCompiler* compiler) { current_compiler_context = compiler; }
};

struct MethodDecl : ASTNode {
    std::string name;
    std::string class_name;  // Class this method belongs to
    std::vector<Variable> parameters;
    DataType return_type = DataType::ANY;
    std::vector<std::unique_ptr<ASTNode>> body;
    bool is_static = false;
    bool is_private = false;
    bool is_protected = false;
    MethodDecl(const std::string& n) : name(n) {}
    MethodDecl(const std::string& n, const std::string& cls) : name(n), class_name(cls) {}
    void generate_code(CodeGenerator& gen) override;
};

struct SuperCall : ExpressionNode {
    std::vector<std::unique_ptr<ExpressionNode>> arguments;
    SuperCall() {}
    void generate_code(CodeGenerator& gen) override;
};

struct SuperMethodCall : ExpressionNode {
    std::string method_name;
    std::vector<std::unique_ptr<ExpressionNode>> arguments;
    std::vector<std::string> keyword_names;  // Names for keyword arguments (empty string for positional)
    SuperMethodCall(const std::string& name) : method_name(name) {}
    void generate_code(CodeGenerator& gen) override;
};

struct ClassDecl : ASTNode {
    std::string name;
    std::vector<std::string> parent_classes; // For multiple inheritance
    std::vector<Variable> fields;
    std::unique_ptr<ConstructorDecl> constructor;
    std::vector<std::unique_ptr<MethodDecl>> methods;
    std::vector<std::unique_ptr<OperatorOverloadDecl>> operator_overloads;  // Operator overloads
    ClassDecl(const std::string& n) : name(n) {}
    void generate_code(CodeGenerator& gen) override;
};

struct OperatorOverloadDecl : ASTNode {
    TokenType operator_type;  // The operator being overloaded (+, -, *, /, [], etc.)
    std::vector<Variable> parameters;  // Parameters for the operator
    DataType return_type = DataType::ANY;
    std::vector<std::unique_ptr<ASTNode>> body;
    std::string class_name;  // Class this operator belongs to
    OperatorOverloadDecl(TokenType op, const std::string& class_name) 
        : operator_type(op), class_name(class_name) {}
    void generate_code(CodeGenerator& gen) override;
};

class Lexer {
private:
    std::string source;
    size_t pos = 0;
    int line = 1, column = 1;
    ErrorReporter* error_reporter = nullptr;
    
    char current_char();
    char peek_char(int offset = 1);
    void advance();
    void skip_whitespace();
    void skip_comment();
    Token make_number();
    Token make_string();
    Token make_template_literal();
    Token make_identifier();
    Token make_regex();
    
public:
    Lexer(const std::string& src) : source(src) {}
    Lexer(const std::string& src, ErrorReporter* reporter) : source(src), error_reporter(reporter) {}
    std::vector<Token> tokenize();
};

class Parser {
private:
    std::vector<Token> tokens;
    size_t pos = 0;
    ErrorReporter* error_reporter = nullptr;
    DataType last_parsed_array_element_type = DataType::ANY;  // Track element type from [type] syntax
    
    // GC Integration - track variable lifetimes and escapes during parsing
    std::unique_ptr<MinimalParserGCIntegration> gc_integration_;
    
    // NEW Simple Lexical Scope System - parse-time analysis
    std::unique_ptr<class SimpleLexicalScopeAnalyzer> lexical_scope_analyzer_;
    
    // Track current scope variables during parsing for escape analysis (legacy, will be removed)
    std::unordered_map<std::string, std::string> current_scope_variables_;
    
    Token& current_token();
    Token& peek_token(int offset = 1);
    void advance();
    bool match(TokenType type);
    bool check(TokenType type);
    bool is_at_end();
    
    std::unique_ptr<ExpressionNode> parse_expression();
    std::unique_ptr<ExpressionNode> parse_assignment_expression();
    std::unique_ptr<ExpressionNode> parse_ternary();
    std::unique_ptr<ExpressionNode> parse_logical_or();
    std::unique_ptr<ExpressionNode> parse_logical_and();
    std::unique_ptr<ExpressionNode> parse_equality();
    std::unique_ptr<ExpressionNode> parse_comparison();
    std::unique_ptr<ExpressionNode> parse_addition();
    std::unique_ptr<ExpressionNode> parse_multiplication();
    std::unique_ptr<ExpressionNode> parse_exponentiation();
    std::unique_ptr<ExpressionNode> parse_unary();
    std::unique_ptr<ExpressionNode> parse_primary();
    std::unique_ptr<ExpressionNode> parse_call();
    
    std::unique_ptr<ASTNode> parse_statement();
    std::unique_ptr<ASTNode> parse_import_statement();
    std::unique_ptr<ASTNode> parse_export_statement();
    std::unique_ptr<ASTNode> parse_function_declaration();
    std::unique_ptr<ASTNode> parse_variable_declaration();
    std::unique_ptr<ASTNode> parse_if_statement();
    std::unique_ptr<ASTNode> parse_for_statement();
    std::unique_ptr<ASTNode> parse_for_each_statement();
    std::unique_ptr<ASTNode> parse_for_in_statement();
    std::unique_ptr<ASTNode> parse_while_statement();
    std::unique_ptr<ASTNode> parse_switch_statement();
    std::unique_ptr<CaseClause> parse_case_clause();
    std::unique_ptr<ASTNode> parse_try_statement();
    std::unique_ptr<CatchClause> parse_catch_clause();
    std::unique_ptr<ASTNode> parse_throw_statement();
    std::unique_ptr<ASTNode> parse_block_statement();
    std::unique_ptr<ASTNode> parse_return_statement();
    std::unique_ptr<ASTNode> parse_break_statement();
    std::unique_ptr<ASTNode> parse_free_statement();
    
    // LEXICAL SCOPE ANALYSIS METHODS
    std::vector<std::string> analyze_function_variable_captures(FunctionExpression* func_expr);
    void find_variable_references_in_node(ASTNode* node, std::vector<std::string>& variables);
    std::unique_ptr<ASTNode> parse_expression_statement();
    std::unique_ptr<ASTNode> parse_class_declaration();
    std::unique_ptr<MethodDecl> parse_method_declaration(const std::string& class_name);
    std::unique_ptr<ConstructorDecl> parse_constructor_declaration(const std::string& class_name);
    std::unique_ptr<OperatorOverloadDecl> parse_operator_overload_declaration(const std::string& class_name);
    std::unique_ptr<SliceExpression> parse_slice_expression();
    
    // Arrow function parsing methods
    std::unique_ptr<ArrowFunction> parse_arrow_function_from_identifier(const std::string& param_name);
    std::unique_ptr<ArrowFunction> parse_arrow_function_from_params(const std::vector<Variable>& params);
    
    DataType parse_type();
    
public:
    Parser(std::vector<Token> toks) : tokens(std::move(toks)) {
        initialize_gc_integration();
        initialize_simple_lexical_scope_system();
    }
    Parser(std::vector<Token> toks, ErrorReporter* reporter) : tokens(std::move(toks)), error_reporter(reporter) {
        initialize_gc_integration();
        initialize_simple_lexical_scope_system();
    }
    ~Parser(); // Destructor declaration to handle unique_ptr with incomplete type
    std::vector<std::unique_ptr<ASTNode>> parse();
    
    // GC Integration methods
    void initialize_gc_integration();
    void finalize_gc_analysis();
    MinimalParserGCIntegration* get_gc_integration() { return gc_integration_.get(); }
    
    // NEW Simple Lexical Scope System methods
    void initialize_simple_lexical_scope_system();
    void finalize_simple_lexical_scope_analysis();
    class SimpleLexicalScopeAnalyzer* get_lexical_scope_analyzer() { return lexical_scope_analyzer_.get(); }
    
    // Current scope variable tracking for escape analysis
    void add_variable_to_current_scope(const std::string& name, const std::string& type);
    void set_current_scope_variables(const std::unordered_map<std::string, std::string>& variables);
    const std::unordered_map<std::string, std::string>& get_current_scope_variables() const { return current_scope_variables_; }
    
    // Scope management for function bodies
    void enter_function_scope();
    void exit_function_scope(const std::unordered_map<std::string, std::string>& parent_scope);
};

// Module system structures
enum class ModuleState {
    NOT_LOADED,      // Module not yet loaded
    LOADING,         // Currently being loaded (for circular import detection)
    LOADED,          // Fully loaded and ready
    ERROR,           // Failed to load
    PARTIAL_LOADED   // Partially loaded due to circular import
};

struct ModuleLoadInfo {
    std::string error_message;
    std::vector<std::string> import_stack;  // Stack trace for circular imports
    std::chrono::steady_clock::time_point load_start_time;
    
    ModuleLoadInfo() : load_start_time(std::chrono::steady_clock::now()) {}
};

struct FunctionExpression;  // Forward declaration for deferred compilation

struct Module {
    std::string path;
    std::unordered_map<std::string, Variable> exports;
    std::unordered_map<std::string, Function> exported_functions;
    bool has_default_export = false;
    std::string default_export_name;
    bool loaded = false;  // Keep for backward compatibility
    std::vector<std::unique_ptr<ASTNode>> ast;
    
    // New lazy loading system
    ModuleState state = ModuleState::NOT_LOADED;
    ModuleLoadInfo load_info;
    bool exports_partial = false;  // True if exports are incomplete due to circular import
    std::vector<std::string> pending_imports;  // List of imports this module depends on
    
    // Lazy loading flag - only execute module code when exports are accessed
    bool code_executed = false;
    
    Module() = default;
    Module(const std::string& module_path) : path(module_path) {}
    
    bool is_ready() const { return state == ModuleState::LOADED; }
    bool is_loading() const { return state == ModuleState::LOADING; }
    bool has_error() const { return state == ModuleState::ERROR; }
    bool is_partial() const { return state == ModuleState::PARTIAL_LOADED; }
};

class GoTSCompiler {
private:
    std::unique_ptr<CodeGenerator> codegen;
    TypeInference type_system;
    std::unordered_map<std::string, Function> functions;
    std::unordered_map<std::string, Variable> global_variables;
    std::unordered_map<std::string, ClassInfo> classes;  // Class registry
    std::unordered_map<std::string, Module> modules;     // Module cache
    Backend target_backend;
    std::string current_file_path;  // Track current file being compiled
    Parser* current_parser;  // Reference to current parser for lexical scope access
    
    // NEW: Hold active scope references during code generation to keep them alive
    std::vector<std::shared_ptr<LexicalScopeNode>> active_scopes_;
    
public:
    GoTSCompiler(Backend backend = Backend::X86_64);
    ~GoTSCompiler();  // Add explicit destructor for debugging
    void compile(const std::string& source);
    void compile_file(const std::string& file_path);
    std::vector<uint8_t> get_machine_code();
    void execute();
    void set_backend(Backend backend);
    
    // Parse-only method for testing scope analysis
    std::vector<std::unique_ptr<ASTNode>> parse_javascript(const std::string& source);
    
    // Parser access for lexical scope integration
    Parser* get_current_parser() const { return current_parser; }
    
    // Module system
    std::string resolve_module_path(const std::string& module_path, const std::string& current_file = "");
    Module* load_module(const std::string& module_path);
    void create_synthetic_default_export(Module& module);
    void set_current_file(const std::string& file_path);
    const std::string& get_current_file() const { return current_file_path; }
    
    // Enhanced lazy loading system
    Module* load_module_lazy(const std::string& module_path);
    bool is_circular_import(const std::string& module_path);
    void handle_circular_import(const std::string& module_path);
    Module* handle_circular_import_and_return(const std::string& module_path);
    std::string get_import_stack_trace() const;
    void execute_module_code(Module& module);
    void prepare_partial_exports(Module& module);
    
    // Automatic scope cleanup for reference counting
    void generate_scope_cleanup_code(CodeGenerator& gen, TypeInference& types);
    
private:
    std::vector<std::string> current_loading_stack;  // Track circular imports
    
public:
    
    // Function management
    void register_function(const std::string& name, const Function& func);
    Function* get_function(const std::string& name);
    bool is_function_defined(const std::string& name) const;
    
    // Class management
    void register_class(const ClassInfo& class_info);
    ClassInfo* get_class(const std::string& class_name);
    bool is_class_defined(const std::string& class_name);
    
    // Class type ID management
    uint32_t get_class_type_id(const std::string& class_name);
    std::string get_class_name_from_type_id(uint32_t type_id);
    
    // Specialized method generation for inheritance
    void generate_specialized_inherited_methods(const ClassDecl& class_decl, CodeGenerator& gen, TypeInference& types);
    bool needs_specialized_methods(const ClassDecl& class_decl) const;
    
    // Operator overloading management
    void register_operator_overload(const std::string& class_name, const OperatorOverload& overload);
    const std::vector<OperatorOverload>* get_operator_overloads(const std::string& class_name, TokenType operator_type);
    bool has_operator_overload(const std::string& class_name, TokenType operator_type);
    bool has_operator_overload(uint32_t class_type_id, TokenType operator_type);  // Type ID version for efficiency
    const OperatorOverload* find_best_operator_overload(const std::string& class_name, TokenType operator_type, 
                                                      const std::vector<DataType>& arg_types);
    
};

// Global function for setting compiler context during AST generation
void set_current_compiler(GoTSCompiler* compiler);
GoTSCompiler* get_current_compiler();

// Scope context initialization for new lexical scope system
void initialize_scope_context(SimpleLexicalScopeAnalyzer* analyzer);
void set_current_scope(LexicalScopeNode* scope);
LexicalScopeNode* get_current_scope();

// Function to compile all deferred function expressions
void compile_deferred_function_expressions(CodeGenerator& gen, TypeInference& types);