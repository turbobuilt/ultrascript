// Minimal TypeInference stub to satisfy linker while transitioning to new scope system
// This provides empty implementations of the methods that other files still reference
// TODO: Eventually remove this file and replace all TypeInference usage with GlobalScopeContext

#include "compiler.h"

// Constructor
TypeInference::TypeInference() {
    std::cout << "[STUB] TypeInference constructor (transitioning to GlobalScopeContext)" << std::endl;
}

// Destructor
TypeInference::~TypeInference() = default;

// Method stubs that do nothing but allow linking
void TypeInference::set_compiler_context(GoTSCompiler* compiler) {
    std::cout << "[STUB] TypeInference::set_compiler_context (no-op)" << std::endl;
    (void)compiler;  // Suppress unused parameter warning
}

DataType TypeInference::get_cast_type(DataType t1, DataType t2) {
    std::cout << "[STUB] TypeInference::get_cast_type returning ANY" << std::endl;
    (void)t1; (void)t2;  // Suppress unused parameter warnings
    return DataType::ANY;
}

void TypeInference::set_variable_offset(const std::string& name, int64_t offset) {
    std::cout << "[STUB] TypeInference::set_variable_offset (no-op)" << std::endl;
    (void)name; (void)offset;  // Suppress unused parameter warnings
}

void TypeInference::set_current_class_context(const std::string& class_name) {
    std::cout << "[STUB] TypeInference::set_current_class_context (no-op)" << std::endl;
    (void)class_name;  // Suppress unused parameter warning
}

void TypeInference::reset_for_function() {
    std::cout << "[STUB] TypeInference::reset_for_function (no-op)" << std::endl;
}

// Add any other methods that show up as missing during linking
DataType TypeInference::infer_type(const std::string& expression) {
    std::cout << "[STUB] TypeInference::infer_type returning ANY for: " << expression << std::endl;
    return DataType::ANY;
}

void TypeInference::set_variable_type(const std::string& name, DataType type) {
    std::cout << "[STUB] TypeInference::set_variable_type (no-op)" << std::endl;
    (void)name; (void)type;
}

DataType TypeInference::get_variable_type(const std::string& name) {
    std::cout << "[STUB] TypeInference::get_variable_type returning ANY for: " << name << std::endl;
    return DataType::ANY;
}

bool TypeInference::variable_exists(const std::string& name) {
    std::cout << "[STUB] TypeInference::variable_exists returning false for: " << name << std::endl;
    return false;
}

int64_t TypeInference::get_variable_offset(const std::string& name) const {
    std::cout << "[STUB] TypeInference::get_variable_offset returning 0 for: " << name << std::endl;
    return 0;
}

int64_t TypeInference::allocate_variable(const std::string& name, DataType type) {
    std::cout << "[STUB] TypeInference::allocate_variable returning 0 for: " << name << std::endl;
    (void)type;
    return 0;
}

// Other methods that might be needed
bool TypeInference::needs_casting(DataType from, DataType to) {
    (void)from; (void)to;
    return false;
}

DataType TypeInference::infer_expression_type(const std::string& expression) {
    (void)expression;
    return DataType::ANY;
}

DataType TypeInference::infer_operator_index_type(const std::string& class_name, const std::string& index_expression) {
    (void)class_name; (void)index_expression;
    return DataType::ANY;
}

DataType TypeInference::infer_operator_index_type(uint32_t class_type_id, const std::string& index_expression) {
    (void)class_type_id; (void)index_expression;
    return DataType::ANY;
}
