#pragma once
#include "compiler.h"
#include "gc_system.h"
#include <string>
#include <vector>
#include <memory>

namespace ultraScript {

/**
 * Static API for GC Parser Integration
 * This provides the static methods expected by tests while delegating to the actual implementation
 */
class GCParserIntegration {
private:
    static std::unique_ptr<class ParserGCIntegration> instance_;
    
public:
    // Static API that delegates to the instance
    static void on_enter_scope(const std::string& scope_name, bool is_function = false);
    static void on_exit_scope();
    
    static void on_variable_declaration(const std::string& name, DataType type);
    static void on_variable_assignment(const std::string& name, const std::string& value_expr);
    static void on_variable_use(const std::string& name);
    
    static void on_function_call(const std::string& func_name, const std::vector<std::string>& args);
    static void on_callback_creation(const std::vector<std::string>& captured_vars);
    static void on_goroutine_creation(const std::vector<std::string>& captured_vars);
    static void on_return_statement(const std::string& returned_var);
    
    static void finalize_escape_analysis();
    static void clear();
    
private:
    static void ensure_initialized();
};

} // namespace ultraScript
