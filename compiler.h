#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <cstdint>
#include <chrono>
#include "ultra_performance_array.h"

namespace ultraScript {

// Forward declarations
struct Token;
enum class TokenType;

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
    IMPORT, EXPORT, FROM, AS, DEFAULT_EXPORT,
    TENSOR, NEW, ARRAY,
    CLASS, EXTENDS, SUPER, THIS, CONSTRUCTOR,
    PUBLIC, PRIVATE, PROTECTED, STATIC,
    EACH, IN, PIPE,  // Added for for-each syntax
    OPERATOR,  // Added for operator overloading
    LPAREN, RPAREN, LBRACE, RBRACE, LBRACKET, RBRACKET,
    SLICE_BRACKET,  // Added for [:] slice operator
    SEMICOLON, COMMA, DOT, COLON, QUESTION,
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
    std::string parent_class;
    std::vector<Variable> fields;
    std::unordered_map<std::string, Function> methods;
    std::unordered_map<TokenType, std::vector<OperatorOverload>> operator_overloads;  // Multiple overloads per operator
    Function* constructor;
    int64_t instance_size;  // Total size needed for an instance
    
    ClassInfo() : constructor(nullptr), instance_size(0) {}
    ClassInfo(const std::string& n) : name(n), constructor(nullptr), instance_size(0) {}
};

enum class Backend {
    X86_64,
    WASM
};

class CodeGenerator {
public:
    virtual ~CodeGenerator() = default;
    virtual void emit_prologue() = 0;
    virtual void emit_epilogue() = 0;
    virtual void emit_mov_reg_imm(int reg, int64_t value) = 0;
    virtual void emit_mov_reg_reg(int dst, int src) = 0;
    virtual void emit_mov_mem_reg(int64_t offset, int reg) = 0;
    virtual void emit_mov_reg_mem(int reg, int64_t offset) = 0;
    virtual void emit_add_reg_imm(int reg, int64_t value) = 0;
    virtual void emit_add_reg_reg(int dst, int src) = 0;
    virtual void emit_sub_reg_imm(int reg, int64_t value) = 0;
    virtual void emit_sub_reg_reg(int dst, int src) = 0;
    virtual void emit_mul_reg_reg(int dst, int src) = 0;
    virtual void emit_div_reg_reg(int dst, int src) = 0;
    virtual void emit_mod_reg_reg(int dst, int src) = 0;
    virtual void emit_call(const std::string& label) = 0;
    virtual void emit_ret() = 0;
    virtual void emit_function_return() = 0;
    virtual void emit_jump(const std::string& label) = 0;
    virtual void emit_jump_if_zero(const std::string& label) = 0;
    virtual void emit_jump_if_not_zero(const std::string& label) = 0;
    virtual void emit_compare(int reg1, int reg2) = 0;
    virtual void emit_setl(int reg) = 0;
    virtual void emit_setg(int reg) = 0;
    virtual void emit_sete(int reg) = 0;
    virtual void emit_setne(int reg) = 0;
    virtual void emit_setle(int reg) = 0;
    virtual void emit_setge(int reg) = 0;
    virtual void emit_and_reg_imm(int reg, int64_t value) = 0;
    virtual void emit_xor_reg_reg(int dst, int src) = 0;
    virtual void emit_call_reg(int reg) = 0;
    virtual void emit_label(const std::string& label) = 0;
    virtual void emit_goroutine_spawn(const std::string& function_name) = 0;
    virtual void emit_goroutine_spawn_with_args(const std::string& function_name, int arg_count) = 0;
    virtual void emit_goroutine_spawn_with_func_ptr() = 0;
    virtual void emit_goroutine_spawn_with_func_id() = 0;
    virtual void emit_goroutine_spawn_with_address(void* function_address) = 0;
    virtual void emit_promise_resolve(int value_reg) = 0;
    virtual void emit_promise_await(int promise_reg) = 0;
    
    // High-Performance Function Calls - Direct function ID access
    virtual void emit_call_fast(uint16_t func_id) = 0;
    virtual void emit_goroutine_spawn_fast(uint16_t func_id) = 0;
    
    // Ultra-High-Performance Direct Address Calls - Zero overhead
    virtual void emit_goroutine_spawn_direct(void* function_address) = 0;
    
    // Lock assembly emission - High-performance atomic operations
    virtual void emit_lock_acquire(int lock_reg) = 0;
    virtual void emit_lock_release(int lock_reg) = 0;
    virtual void emit_lock_try_acquire(int lock_reg, int result_reg) = 0;
    virtual void emit_lock_try_acquire_timeout(int lock_reg, int timeout_reg, int result_reg) = 0;
    
    // Atomic operations for lock implementation
    virtual void emit_atomic_compare_exchange(int ptr_reg, int expected_reg, int desired_reg, int result_reg) = 0;
    virtual void emit_atomic_fetch_add(int ptr_reg, int value_reg, int result_reg) = 0;
    virtual void emit_atomic_store(int ptr_reg, int value_reg, int memory_order) = 0;
    virtual void emit_atomic_load(int ptr_reg, int result_reg, int memory_order) = 0;
    virtual void emit_memory_fence(int fence_type) = 0;
    
    virtual std::vector<uint8_t> get_code() const = 0;
    virtual void clear() = 0;
    virtual size_t get_current_offset() const = 0;
    virtual const std::unordered_map<std::string, int64_t>& get_label_offsets() const = 0;
    
    // Get offset for a specific label
    virtual int64_t get_label_offset(const std::string& label) const {
        const auto& offsets = get_label_offsets();
        auto it = offsets.find(label);
        return (it != offsets.end()) ? it->second : -1;
    }
};

class X86CodeGen : public CodeGenerator {
private:
    std::vector<uint8_t> code;
    std::unordered_map<std::string, int64_t> label_offsets;
    std::vector<std::pair<std::string, int64_t>> unresolved_jumps;
    int64_t current_stack_offset;
    int64_t function_stack_size;
    
public:
    X86CodeGen() : current_stack_offset(0), function_stack_size(0) {}
    void emit_prologue() override;
    void emit_epilogue() override;
    void emit_mov_reg_imm(int reg, int64_t value) override;
    void emit_mov_reg_reg(int dst, int src) override;
    void emit_mov_mem_reg(int64_t offset, int reg) override;
    void emit_mov_reg_mem(int reg, int64_t offset) override;
    void emit_add_reg_imm(int reg, int64_t value) override;
    void emit_add_reg_reg(int dst, int src) override;
    void emit_sub_reg_imm(int reg, int64_t value) override;
    void emit_sub_reg_reg(int dst, int src) override;
    void emit_mul_reg_reg(int dst, int src) override;
    void emit_div_reg_reg(int dst, int src) override;
    void emit_mod_reg_reg(int dst, int src) override;
    void emit_call(const std::string& label) override;
    void emit_ret() override;
    void emit_function_return() override;
    void emit_jump(const std::string& label) override;
    void emit_jump_if_zero(const std::string& label) override;
    void emit_jump_if_not_zero(const std::string& label) override;
    void emit_compare(int reg1, int reg2) override;
    void emit_setl(int reg) override;
    void emit_setg(int reg) override;
    void emit_sete(int reg) override;
    void emit_setne(int reg) override;
    void emit_setle(int reg) override;
    void emit_setge(int reg) override;
    void emit_and_reg_imm(int reg, int64_t value) override;
    void emit_xor_reg_reg(int dst, int src) override;
    void emit_call_reg(int reg) override;
    void emit_jump_if_equal(const std::string& label);
    void emit_jump_if_greater(const std::string& label);
    void emit_jump_if_less(const std::string& label);
    void emit_label(const std::string& label) override;
    void emit_goroutine_spawn(const std::string& function_name) override;
    void emit_goroutine_spawn_with_args(const std::string& function_name, int arg_count) override;
    void emit_goroutine_spawn_with_func_ptr() override;
    void emit_goroutine_spawn_with_func_id() override;
    void emit_goroutine_spawn_with_address(void* function_address) override;
    void emit_promise_resolve(int value_reg) override;
    void emit_promise_await(int promise_reg) override;
    
    // High-Performance Function Calls - Direct function ID access
    void emit_call_fast(uint16_t func_id) override;
    void emit_goroutine_spawn_fast(uint16_t func_id) override;
    
    // Ultra-High-Performance Direct Address Calls - Zero overhead
    void emit_goroutine_spawn_direct(void* function_address) override;
    
    // Lock assembly emission - High-performance atomic operations
    void emit_lock_acquire(int lock_reg) override;
    void emit_lock_release(int lock_reg) override;
    void emit_lock_try_acquire(int lock_reg, int result_reg) override;
    void emit_lock_try_acquire_timeout(int lock_reg, int timeout_reg, int result_reg) override;
    
    // Atomic operations for lock implementation
    void emit_atomic_compare_exchange(int ptr_reg, int expected_reg, int desired_reg, int result_reg) override;
    void emit_atomic_fetch_add(int ptr_reg, int value_reg, int result_reg) override;
    void emit_atomic_store(int ptr_reg, int value_reg, int memory_order) override;
    void emit_atomic_load(int ptr_reg, int result_reg, int memory_order) override;
    void emit_memory_fence(int fence_type) override;
    
    // Near-Optimal Relative Offset Calls - One LEA instruction overhead
    void emit_goroutine_spawn_with_offset(size_t function_offset);
    void emit_calculate_function_address_from_offset(size_t function_offset);
    
    std::vector<uint8_t> get_code() const override { return code; }
    void clear() override { code.clear(); label_offsets.clear(); unresolved_jumps.clear(); }
    size_t get_current_offset() const override;
    const std::unordered_map<std::string, int64_t>& get_label_offsets() const override { return label_offsets; }
    void resolve_runtime_function_calls();  // Resolve unresolved runtime function calls
    void set_function_stack_size(int64_t size) { function_stack_size = size; }
    int64_t get_function_stack_size() const { return function_stack_size; }
    void emit_mov_reg_mem_rsp(int reg, int64_t offset);  // RSP-relative version
    void emit_mov_mem_rsp_reg(int64_t offset, int reg);  // RSP-relative store version
    
    // High-Performance String Assembly Optimizations
    void emit_string_length_fast(int string_reg, int dest_reg);
    void emit_string_concat_fast(int str1_reg, int str2_reg, int dest_reg);
    void emit_string_equals_fast(int str1_reg, int str2_reg, int dest_reg);
    void emit_fast_memcpy();  // RDI=dest, RSI=src, RDX=len
    void emit_fast_memcmp();  // RDI=ptr1, RSI=ptr2, RDX=len
    
    // Low-level emit helpers
    void emit_byte(uint8_t byte);
    void emit_u32(uint32_t value);
};

class WasmCodeGen : public CodeGenerator {
private:
    std::vector<uint8_t> code;
    std::unordered_map<std::string, int64_t> label_offsets;
    std::vector<std::pair<std::string, int64_t>> unresolved_jumps;
    int64_t current_local_count;
    
    void emit_leb128(int64_t value);
    void emit_opcode(uint8_t opcode);
    
public:
    void emit_prologue() override;
    void emit_epilogue() override;
    void emit_mov_reg_imm(int reg, int64_t value) override;
    void emit_mov_reg_reg(int dst, int src) override;
    void emit_mov_mem_reg(int64_t offset, int reg) override;
    void emit_mov_reg_mem(int reg, int64_t offset) override;
    void emit_add_reg_imm(int reg, int64_t value) override;
    void emit_add_reg_reg(int dst, int src) override;
    void emit_sub_reg_imm(int reg, int64_t value) override;
    void emit_sub_reg_reg(int dst, int src) override;
    void emit_mul_reg_reg(int dst, int src) override;
    void emit_div_reg_reg(int dst, int src) override;
    void emit_mod_reg_reg(int dst, int src) override;
    void emit_call(const std::string& label) override;
    void emit_ret() override;
    void emit_function_return() override;
    void emit_jump(const std::string& label) override;
    void emit_jump_if_zero(const std::string& label) override;
    void emit_jump_if_not_zero(const std::string& label) override;
    void emit_compare(int reg1, int reg2) override;
    void emit_setl(int reg) override;
    void emit_setg(int reg) override;
    void emit_sete(int reg) override;
    void emit_setne(int reg) override;
    void emit_setle(int reg) override;
    void emit_setge(int reg) override;
    void emit_and_reg_imm(int reg, int64_t value) override;
    void emit_xor_reg_reg(int dst, int src) override;
    void emit_call_reg(int reg) override;
    void emit_label(const std::string& label) override;
    void emit_goroutine_spawn(const std::string& function_name) override;
    void emit_goroutine_spawn_with_args(const std::string& function_name, int arg_count) override;
    void emit_goroutine_spawn_with_func_ptr() override;
    void emit_goroutine_spawn_with_func_id() override;
    void emit_goroutine_spawn_with_address(void* function_address) override;
    void emit_promise_resolve(int value_reg) override;
    void emit_promise_await(int promise_reg) override;
    
    // High-Performance Function Calls - Direct function ID access
    void emit_call_fast(uint16_t func_id) override;
    void emit_goroutine_spawn_fast(uint16_t func_id) override;
    
    // Ultra-High-Performance Direct Address Calls - Zero overhead
    void emit_goroutine_spawn_direct(void* function_address) override;
    
    // Lock assembly emission - High-performance atomic operations
    void emit_lock_acquire(int lock_reg) override;
    void emit_lock_release(int lock_reg) override;
    void emit_lock_try_acquire(int lock_reg, int result_reg) override;
    void emit_lock_try_acquire_timeout(int lock_reg, int timeout_reg, int result_reg) override;
    
    // Atomic operations for lock implementation
    void emit_atomic_compare_exchange(int ptr_reg, int expected_reg, int desired_reg, int result_reg) override;
    void emit_atomic_fetch_add(int ptr_reg, int value_reg, int result_reg) override;
    void emit_atomic_store(int ptr_reg, int value_reg, int memory_order) override;
    void emit_atomic_load(int ptr_reg, int result_reg, int memory_order) override;
    void emit_memory_fence(int fence_type) override;
    
    std::vector<uint8_t> get_code() const override { return code; }
    void clear() override { code.clear(); label_offsets.clear(); unresolved_jumps.clear(); }
    size_t get_current_offset() const override;
    const std::unordered_map<std::string, int64_t>& get_label_offsets() const override { return label_offsets; }
};

class TypeInference {
private:
    std::unordered_map<std::string, DataType> variable_types;
    std::unordered_map<std::string, std::string> variable_class_names;  // For CLASS_INSTANCE variables
    std::unordered_map<std::string, int64_t> variable_offsets;
    int64_t current_offset = -8; // Start at -8 (RBP-8)
    
    // Function parameter tracking for keyword arguments
    std::unordered_map<std::string, std::vector<std::string>> function_param_names;
    
public:
    DataType infer_type(const std::string& expression);
    DataType get_cast_type(DataType t1, DataType t2);
    bool needs_casting(DataType from, DataType to);
    void set_variable_type(const std::string& name, DataType type);
    void set_variable_class_type(const std::string& name, const std::string& class_name);
    DataType get_variable_type(const std::string& name);
    std::string get_variable_class_name(const std::string& name);
    
    // Variable storage management
    void set_variable_offset(const std::string& name, int64_t offset);
    int64_t get_variable_offset(const std::string& name);
    int64_t allocate_variable(const std::string& name, DataType type);
    bool variable_exists(const std::string& name);
    void enter_scope();
    void exit_scope();
    void reset_for_function();
    void reset_for_function_with_params(int param_count);
    
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
    bool is_deterministic_expression(const std::string& expression);
    bool is_array_comparison_expression(const std::string& expression);
    DataType infer_expression_type(const std::string& expression);
    DataType infer_complex_expression_type(const std::string& expression);
    DataType get_best_numeric_operator_type(const std::string& class_name, const std::string& numeric_literal);
    TokenType string_to_operator_token(const std::string& op_str);
    
    // Expression string extraction helpers
    std::string extract_expression_string(ExpressionNode* node);
    std::string token_type_to_string(TokenType token);
    bool is_numeric_literal(const std::string& expression);
};

struct ASTNode {
    virtual ~ASTNode() = default;
    virtual void generate_code(CodeGenerator& gen, TypeInference& types) = 0;
};

struct ExpressionNode : ASTNode {
    DataType result_type = DataType::ANY;
};

struct NumberLiteral : ExpressionNode {
    double value;
    NumberLiteral(double v) : value(v) {}
    void generate_code(CodeGenerator& gen, TypeInference& types) override;
};

struct StringLiteral : ExpressionNode {
    std::string value;
    StringLiteral(const std::string& v) : value(v) {}
    void generate_code(CodeGenerator& gen, TypeInference& types) override;
};

struct RegexLiteral : ExpressionNode {
    std::string pattern;
    std::string flags;
    RegexLiteral(const std::string& p, const std::string& f = "") : pattern(p), flags(f) {}
    void generate_code(CodeGenerator& gen, TypeInference& types) override;
};

struct Identifier : ExpressionNode {
    std::string name;
    Identifier(const std::string& n) : name(n) {}
    void generate_code(CodeGenerator& gen, TypeInference& types) override;
};

struct BinaryOp : ExpressionNode {
    std::unique_ptr<ExpressionNode> left, right;
    TokenType op;
    BinaryOp(std::unique_ptr<ExpressionNode> l, TokenType o, std::unique_ptr<ExpressionNode> r)
        : op(o), left(std::move(l)), right(std::move(r)) {}
    void generate_code(CodeGenerator& gen, TypeInference& types) override;
};

struct TernaryOperator : ExpressionNode {
    std::unique_ptr<ExpressionNode> condition, true_expr, false_expr;
    TernaryOperator(std::unique_ptr<ExpressionNode> cond, std::unique_ptr<ExpressionNode> true_val, std::unique_ptr<ExpressionNode> false_val)
        : condition(std::move(cond)), true_expr(std::move(true_val)), false_expr(std::move(false_val)) {}
    void generate_code(CodeGenerator& gen, TypeInference& types) override;
};

struct FunctionCall : ExpressionNode {
    std::string name;
    std::vector<std::unique_ptr<ExpressionNode>> arguments;
    std::vector<std::string> keyword_names;  // Names for keyword arguments (empty string for positional)
    bool is_goroutine = false;
    bool is_awaited = false;
    FunctionCall(const std::string& n) : name(n) {}
    void generate_code(CodeGenerator& gen, TypeInference& types) override;
};

struct FunctionExpression : ExpressionNode {
    std::string name;  // Optional name for debugging/recursion
    std::vector<Variable> parameters;
    DataType return_type = DataType::ANY;
    std::vector<std::unique_ptr<ASTNode>> body;
    bool is_goroutine = false;
    
    // NEW: For three-phase compilation system
    std::string compilation_assigned_name_;  // Name assigned during Phase 1
    
    FunctionExpression() : name("") {}
    FunctionExpression(const std::string& n) : name(n) {}
    void generate_code(CodeGenerator& gen, TypeInference& types) override;
    void compile_function_body(CodeGenerator& gen, TypeInference& types, const std::string& func_name);
    
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
    void generate_code(CodeGenerator& gen, TypeInference& types) override;
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
    void generate_code(CodeGenerator& gen, TypeInference& types) override;
};

struct ArrayLiteral : ExpressionNode {
    std::vector<std::unique_ptr<ExpressionNode>> elements;
    ArrayLiteral() {}
    void generate_code(CodeGenerator& gen, TypeInference& types) override;
};

struct ObjectLiteral : ExpressionNode {
    std::vector<std::pair<std::string, std::unique_ptr<ExpressionNode>>> properties;
    ObjectLiteral() {}
    void generate_code(CodeGenerator& gen, TypeInference& types) override;
};

struct TypedArrayLiteral : ExpressionNode {
    std::vector<std::unique_ptr<ExpressionNode>> elements;
    DataType array_type;
    TypedArrayLiteral(DataType type) : array_type(type) {}
    void generate_code(CodeGenerator& gen, TypeInference& types) override;
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
    void generate_code(CodeGenerator& gen, TypeInference& types) override;
};

struct ArrayAccess : ExpressionNode {
    std::unique_ptr<ExpressionNode> object;
    std::unique_ptr<ExpressionNode> index;
    std::vector<std::unique_ptr<SliceExpression>> slices;  // For multi-dimensional slicing
    bool is_slice_expression = false;  // True if index contains colons, ellipsis, etc.
    std::string slice_expression;       // Raw string representation for complex indexing
    ArrayAccess(std::unique_ptr<ExpressionNode> obj, std::unique_ptr<ExpressionNode> idx)
        : object(std::move(obj)), index(std::move(idx)) {}
    void generate_code(CodeGenerator& gen, TypeInference& types) override;
};

struct OperatorCall : ExpressionNode {
    std::unique_ptr<ExpressionNode> left_operand;
    std::unique_ptr<ExpressionNode> right_operand;  // For binary operators, null for unary
    TokenType operator_type;
    std::string class_name;  // Class that defines the operator
    OperatorCall(std::unique_ptr<ExpressionNode> left, TokenType op, std::unique_ptr<ExpressionNode> right, const std::string& cls)
        : left_operand(std::move(left)), operator_type(op), right_operand(std::move(right)), class_name(cls) {}
    void generate_code(CodeGenerator& gen, TypeInference& types) override;
};

struct Assignment : ExpressionNode {
    std::string variable_name;
    std::unique_ptr<ExpressionNode> value;
    DataType declared_type = DataType::ANY;
    Assignment(const std::string& name, std::unique_ptr<ExpressionNode> val)
        : variable_name(name), value(std::move(val)) {}
    void generate_code(CodeGenerator& gen, TypeInference& types) override;
};

struct PropertyAssignment : ExpressionNode {
    std::string object_name;
    std::string property_name;
    std::unique_ptr<ExpressionNode> value;
    PropertyAssignment(const std::string& obj, const std::string& prop, std::unique_ptr<ExpressionNode> val)
        : object_name(obj), property_name(prop), value(std::move(val)) {}
    void generate_code(CodeGenerator& gen, TypeInference& types) override;
};

struct ExpressionPropertyAssignment : ExpressionNode {
    std::unique_ptr<ExpressionNode> object;
    std::string property_name;
    std::unique_ptr<ExpressionNode> value;
    ExpressionPropertyAssignment(std::unique_ptr<ExpressionNode> obj, const std::string& prop, std::unique_ptr<ExpressionNode> val)
        : object(std::move(obj)), property_name(prop), value(std::move(val)) {}
    void generate_code(CodeGenerator& gen, TypeInference& types) override;
};

struct PostfixIncrement : ExpressionNode {
    std::string variable_name;
    PostfixIncrement(const std::string& name) : variable_name(name) {}
    void generate_code(CodeGenerator& gen, TypeInference& types) override;
};

struct PostfixDecrement : ExpressionNode {
    std::string variable_name;
    PostfixDecrement(const std::string& name) : variable_name(name) {}
    void generate_code(CodeGenerator& gen, TypeInference& types) override;
};

struct FunctionDecl : ASTNode {
    std::string name;
    std::vector<Variable> parameters;
    DataType return_type = DataType::ANY;
    std::vector<std::unique_ptr<ASTNode>> body;
    FunctionDecl(const std::string& n) : name(n) {}
    void generate_code(CodeGenerator& gen, TypeInference& types) override;
};

struct IfStatement : ASTNode {
    std::unique_ptr<ExpressionNode> condition;
    std::vector<std::unique_ptr<ASTNode>> then_body;
    std::vector<std::unique_ptr<ASTNode>> else_body;
    void generate_code(CodeGenerator& gen, TypeInference& types) override;
};

struct ForLoop : ASTNode {
    std::unique_ptr<ASTNode> init;
    std::unique_ptr<ExpressionNode> condition;
    std::unique_ptr<ASTNode> update;
    std::vector<std::unique_ptr<ASTNode>> body;
    void generate_code(CodeGenerator& gen, TypeInference& types) override;
};

struct ForEachLoop : ASTNode {
    std::string index_var_name;   // index for arrays, key for objects
    std::string value_var_name;   // value variable name
    std::unique_ptr<ExpressionNode> iterable;  // the array/object to iterate over
    std::vector<std::unique_ptr<ASTNode>> body;
    ForEachLoop(const std::string& index_name, const std::string& value_name)
        : index_var_name(index_name), value_var_name(value_name) {}
    void generate_code(CodeGenerator& gen, TypeInference& types) override;
};

struct ReturnStatement : ASTNode {
    std::unique_ptr<ExpressionNode> value;
    ReturnStatement(std::unique_ptr<ExpressionNode> val) : value(std::move(val)) {}
    void generate_code(CodeGenerator& gen, TypeInference& types) override;
};

struct BreakStatement : ASTNode {
    BreakStatement() {}
    void generate_code(CodeGenerator& gen, TypeInference& types) override;
};

struct CaseClause : ASTNode {
    std::unique_ptr<ExpressionNode> value;  // nullptr for default case
    std::vector<std::unique_ptr<ASTNode>> body;
    bool is_default = false;
    
    CaseClause(std::unique_ptr<ExpressionNode> val) : value(std::move(val)) {}
    CaseClause() : is_default(true) {}  // Default constructor for default case
    void generate_code(CodeGenerator& gen, TypeInference& types) override;
};

struct SwitchStatement : ASTNode {
    std::unique_ptr<ExpressionNode> discriminant;
    std::vector<std::unique_ptr<CaseClause>> cases;
    
    SwitchStatement(std::unique_ptr<ExpressionNode> disc) : discriminant(std::move(disc)) {}
    void generate_code(CodeGenerator& gen, TypeInference& types) override;
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
    void generate_code(CodeGenerator& gen, TypeInference& types) override;
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
    void generate_code(CodeGenerator& gen, TypeInference& types) override;
};

struct PropertyAccess : ExpressionNode {
    std::string object_name;
    std::string property_name;
    PropertyAccess(const std::string& obj, const std::string& prop) 
        : object_name(obj), property_name(prop) {}
    void generate_code(CodeGenerator& gen, TypeInference& types) override;
};

struct ExpressionPropertyAccess : ExpressionNode {
    std::unique_ptr<ExpressionNode> object;
    std::string property_name;
    ExpressionPropertyAccess(std::unique_ptr<ExpressionNode> obj, const std::string& prop) 
        : object(std::move(obj)), property_name(prop) {}
    void generate_code(CodeGenerator& gen, TypeInference& types) override;
};

struct ThisExpression : ExpressionNode {
    ThisExpression() {}
    void generate_code(CodeGenerator& gen, TypeInference& types) override;
};

struct NewExpression : ExpressionNode {
    std::string class_name;
    std::vector<std::unique_ptr<ExpressionNode>> arguments;
    bool is_dart_style = false; // For new Person{name: "bob"} syntax
    std::vector<std::pair<std::string, std::unique_ptr<ExpressionNode>>> dart_args;
    NewExpression(const std::string& name) : class_name(name) {}
    void generate_code(CodeGenerator& gen, TypeInference& types) override;
};

struct ConstructorDecl : ASTNode {
    std::string class_name;
    std::vector<Variable> parameters;
    std::vector<std::unique_ptr<ASTNode>> body;
    ConstructorDecl(const std::string& cn) : class_name(cn) {}
    void generate_code(CodeGenerator& gen, TypeInference& types) override;
    
    // Helper to get class info during code generation
    static GoTSCompiler* current_compiler_context;
    static void set_compiler_context(GoTSCompiler* compiler) { current_compiler_context = compiler; }
};

struct MethodDecl : ASTNode {
    std::string name;
    std::vector<Variable> parameters;
    DataType return_type = DataType::ANY;
    std::vector<std::unique_ptr<ASTNode>> body;
    bool is_static = false;
    bool is_private = false;
    bool is_protected = false;
    MethodDecl(const std::string& n) : name(n) {}
    void generate_code(CodeGenerator& gen, TypeInference& types) override;
};

struct SuperCall : ExpressionNode {
    std::vector<std::unique_ptr<ExpressionNode>> arguments;
    SuperCall() {}
    void generate_code(CodeGenerator& gen, TypeInference& types) override;
};

struct SuperMethodCall : ExpressionNode {
    std::string method_name;
    std::vector<std::unique_ptr<ExpressionNode>> arguments;
    std::vector<std::string> keyword_names;  // Names for keyword arguments (empty string for positional)
    SuperMethodCall(const std::string& name) : method_name(name) {}
    void generate_code(CodeGenerator& gen, TypeInference& types) override;
};

struct ClassDecl : ASTNode {
    std::string name;
    std::string parent_class; // For inheritance
    std::vector<Variable> fields;
    std::unique_ptr<ConstructorDecl> constructor;
    std::vector<std::unique_ptr<MethodDecl>> methods;
    std::vector<std::unique_ptr<OperatorOverloadDecl>> operator_overloads;  // Operator overloads
    ClassDecl(const std::string& n) : name(n) {}
    void generate_code(CodeGenerator& gen, TypeInference& types) override;
};

struct OperatorOverloadDecl : ASTNode {
    TokenType operator_type;  // The operator being overloaded (+, -, *, /, [], etc.)
    std::vector<Variable> parameters;  // Parameters for the operator
    DataType return_type = DataType::ANY;
    std::vector<std::unique_ptr<ASTNode>> body;
    std::string class_name;  // Class this operator belongs to
    OperatorOverloadDecl(TokenType op, const std::string& class_name) 
        : operator_type(op), class_name(class_name) {}
    void generate_code(CodeGenerator& gen, TypeInference& types) override;
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
    
    Token& current_token();
    Token& peek_token(int offset = 1);
    void advance();
    bool match(TokenType type);
    bool check(TokenType type);
    
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
    std::unique_ptr<ASTNode> parse_switch_statement();
    std::unique_ptr<CaseClause> parse_case_clause();
    std::unique_ptr<ASTNode> parse_return_statement();
    std::unique_ptr<ASTNode> parse_break_statement();
    std::unique_ptr<ASTNode> parse_expression_statement();
    std::unique_ptr<ASTNode> parse_class_declaration();
    std::unique_ptr<MethodDecl> parse_method_declaration();
    std::unique_ptr<ConstructorDecl> parse_constructor_declaration(const std::string& class_name);
    std::unique_ptr<OperatorOverloadDecl> parse_operator_overload_declaration(const std::string& class_name);
    std::unique_ptr<SliceExpression> parse_slice_expression();
    
    DataType parse_type();
    
public:
    Parser(std::vector<Token> toks) : tokens(std::move(toks)) {}
    Parser(std::vector<Token> toks, ErrorReporter* reporter) : tokens(std::move(toks)), error_reporter(reporter) {}
    std::vector<std::unique_ptr<ASTNode>> parse();
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
    
public:
    GoTSCompiler(Backend backend = Backend::X86_64);
    void compile(const std::string& source);
    void compile_file(const std::string& file_path);
    std::vector<uint8_t> get_machine_code();
    void execute();
    void set_backend(Backend backend);
    
    // Module system
    std::string resolve_module_path(const std::string& module_path, const std::string& current_file = "");
    Module* load_module(const std::string& module_path);
    void create_synthetic_default_export(Module& module);
    void set_current_file(const std::string& file_path) { current_file_path = file_path; }
    const std::string& get_current_file() const { return current_file_path; }
    
    // Enhanced lazy loading system
    Module* load_module_lazy(const std::string& module_path);
    bool is_circular_import(const std::string& module_path);
    void handle_circular_import(const std::string& module_path);
    Module* handle_circular_import_and_return(const std::string& module_path);
    std::string get_import_stack_trace() const;
    void execute_module_code(Module& module);
    void prepare_partial_exports(Module& module);
    
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
    
    // Operator overloading management
    void register_operator_overload(const std::string& class_name, const OperatorOverload& overload);
    const std::vector<OperatorOverload>* get_operator_overloads(const std::string& class_name, TokenType operator_type);
    bool has_operator_overload(const std::string& class_name, TokenType operator_type);
    const OperatorOverload* find_best_operator_overload(const std::string& class_name, TokenType operator_type, 
                                                      const std::vector<DataType>& arg_types);
    
};

// Global function for setting compiler context during AST generation
void set_current_compiler(GoTSCompiler* compiler);
GoTSCompiler* get_current_compiler();

// Function to compile all deferred function expressions
void compile_deferred_function_expressions(CodeGenerator& gen, TypeInference& types);

}