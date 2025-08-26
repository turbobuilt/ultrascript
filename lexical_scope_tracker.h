#pragma once

#include "escape_analyzer.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>

// Forward declarations
class FunctionExpression;

// Information about a captured variable
struct CapturedVariable {
    std::string name;
    std::string type;
    size_t offset_in_parent_stack;  // Offset from parent stack frame
    bool use_register;              // True if this should go in register, false for stack
    int register_index;             // Which register to use (-1 if stack)
    
    CapturedVariable(const std::string& n, const std::string& t) 
        : name(n), type(t), offset_in_parent_stack(0), use_register(false), register_index(-1) {}
};

// Information about a goroutine function and its captures
struct GoroutineCaptureInfo {
    FunctionExpression* function;
    std::vector<CapturedVariable> captured_vars;
    size_t total_lexical_scope_size;  // Total bytes needed for lexical scope data
    
    GoroutineCaptureInfo(FunctionExpression* func) 
        : function(func), total_lexical_scope_size(0) {}
};

// Lexical scope tracker - tracks variable captures for goroutine parameter passing
class LexicalScopeTracker : public EscapeConsumer {
private:
    // Map from function to its capture info
    std::unordered_map<FunctionExpression*, std::unique_ptr<GoroutineCaptureInfo>> goroutine_captures_;
    
    // Current function being analyzed
    FunctionExpression* current_analyzing_function_;
    
public:
    LexicalScopeTracker() : current_analyzing_function_(nullptr) {}
    ~LexicalScopeTracker() = default;
    
    // EscapeConsumer interface
    void on_variable_escaped(const std::string& var_name, 
                           FunctionExpression* capturing_func,
                           const std::string& var_type = "") override;
    
    void on_function_analysis_start(FunctionExpression* func) override;
    void on_function_analysis_complete(FunctionExpression* func) override;
    
    // Get capture info for a specific goroutine function
    const GoroutineCaptureInfo* get_capture_info(FunctionExpression* func) const;
    
    // Get all captured variables for a function
    const std::vector<CapturedVariable>* get_captured_variables(FunctionExpression* func) const;
    
    // Check if a function captures any variables
    bool has_captures(FunctionExpression* func) const;
    
    // Get the number of captured variables for a function
    size_t get_capture_count(FunctionExpression* func) const;
    
    // Debug: Print all capture information
    void print_all_captures() const;
    
private:
    // Allocate registers and stack slots for captured variables
    void allocate_storage_for_captures(GoroutineCaptureInfo* info);
    
    // Calculate the total size needed for lexical scope data
    void calculate_lexical_scope_size(GoroutineCaptureInfo* info);
    
    // Get the size of a type in bytes
    size_t get_type_size(const std::string& type) const;
    
    // Check if a type should prefer register allocation
    bool should_use_register(const std::string& type) const;
};
