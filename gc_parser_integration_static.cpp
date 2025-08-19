#include "gc_parser_integration_static.h"
#include "parser_gc_integration.h"
#include <iostream>


std::unique_ptr<ParserGCIntegration> GCParserIntegration::instance_;

void GCParserIntegration::ensure_initialized() {
    if (!instance_) {
        instance_ = std::make_unique<ParserGCIntegration>();
    }
}

void GCParserIntegration::on_enter_scope(const std::string& scope_name, bool is_function) {
    ensure_initialized();
    instance_->enter_scope(scope_name, is_function);
}

void GCParserIntegration::on_exit_scope() {
    ensure_initialized();
    instance_->exit_scope();
}

void GCParserIntegration::on_variable_declaration(const std::string& name, DataType type) {
    ensure_initialized();
    instance_->declare_variable(name, type);
}

void GCParserIntegration::on_variable_assignment(const std::string& name, const std::string& value_expr) {
    ensure_initialized();
    // For the static API, we don't have access to the ExpressionNode, so we pass nullptr
    // In practice, this would need better integration with the parser
    instance_->assign_variable(name, nullptr);
}

void GCParserIntegration::on_variable_use(const std::string& name) {
    ensure_initialized();
    instance_->use_variable(name);
}

void GCParserIntegration::on_function_call(const std::string& func_name, const std::vector<std::string>& args) {
    ensure_initialized();
    // Create empty vector for the static API
    std::vector<std::unique_ptr<ExpressionNode>> empty_args;
    instance_->mark_function_call(func_name, empty_args);
    
    std::cout << "[GC-Static] Function call '" << func_name << "' with " << args.size() << " arguments" << std::endl;
    for (const auto& arg : args) {
        std::cout << "[GC-Static]   Arg: " << arg << std::endl;
    }
}

void GCParserIntegration::on_callback_creation(const std::vector<std::string>& captured_vars) {
    ensure_initialized();
    instance_->mark_closure_capture(captured_vars);
}

void GCParserIntegration::on_goroutine_creation(const std::vector<std::string>& captured_vars) {
    ensure_initialized();
    instance_->mark_goroutine_capture(captured_vars);
}

void GCParserIntegration::on_return_statement(const std::string& returned_var) {
    ensure_initialized();
    // For the static API, we don't have access to the ExpressionNode, so we pass nullptr
    instance_->mark_return_value(nullptr);
    std::cout << "[GC-Static] Return statement with variable: " << returned_var << std::endl;
}

void GCParserIntegration::finalize_escape_analysis() {
    if (instance_) {
        instance_->finalize_analysis();
    }
}

void GCParserIntegration::clear() {
    instance_.reset();
}

