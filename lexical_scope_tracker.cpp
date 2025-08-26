#include "lexical_scope_tracker.h"
#include "compiler.h"  // Contains AST node definitions
#include <iostream>
#include <algorithm>

void LexicalScopeTracker::on_function_analysis_start(FunctionExpression* func) {
    current_analyzing_function_ = func;
    std::cout << "[LexicalScopeTracker] Starting analysis for goroutine function" << std::endl;
    
    // Create capture info for this function
    goroutine_captures_[func] = std::make_unique<GoroutineCaptureInfo>(func);
}

void LexicalScopeTracker::on_variable_escaped(const std::string& var_name, 
                                            FunctionExpression* capturing_func,
                                            const std::string& var_type) {
    std::cout << "[LexicalScopeTracker] Variable ESCAPED: " << var_name 
              << " (type: " << var_type << ")" << std::endl;
    
    // Find or create capture info for this function
    auto it = goroutine_captures_.find(capturing_func);
    if (it == goroutine_captures_.end()) {
        goroutine_captures_[capturing_func] = std::make_unique<GoroutineCaptureInfo>(capturing_func);
        it = goroutine_captures_.find(capturing_func);
    }
    
    GoroutineCaptureInfo* info = it->second.get();
    
    // Check if we already have this variable (avoid duplicates)
    for (const auto& captured : info->captured_vars) {
        if (captured.name == var_name) {
            std::cout << "[LexicalScopeTracker] Variable " << var_name << " already captured, skipping" << std::endl;
            return;
        }
    }
    
    // Add the captured variable
    info->captured_vars.emplace_back(var_name, var_type);
    std::cout << "[LexicalScopeTracker] Added captured variable: " << var_name 
              << " (now " << info->captured_vars.size() << " total)" << std::endl;
}

void LexicalScopeTracker::on_function_analysis_complete(FunctionExpression* func) {
    std::cout << "[LexicalScopeTracker] Completing analysis for goroutine function" << std::endl;
    
    auto it = goroutine_captures_.find(func);
    if (it != goroutine_captures_.end()) {
        GoroutineCaptureInfo* info = it->second.get();
        
        std::cout << "[LexicalScopeTracker] Function captures " 
                  << info->captured_vars.size() << " variables" << std::endl;
        
        // Allocate storage (registers vs stack) for captured variables
        allocate_storage_for_captures(info);
        
        // Calculate total lexical scope size
        calculate_lexical_scope_size(info);
        
        // Print capture summary
        std::cout << "[LexicalScopeTracker] CAPTURE SUMMARY:" << std::endl;
        std::cout << "[LexicalScopeTracker]   Total variables: " << info->captured_vars.size() << std::endl;
        std::cout << "[LexicalScopeTracker]   Lexical scope size: " << info->total_lexical_scope_size << " bytes" << std::endl;
        
        for (const auto& captured : info->captured_vars) {
            std::cout << "[LexicalScopeTracker]   - " << captured.name 
                      << " (" << captured.type << ")";
            if (captured.use_register) {
                std::cout << " -> Register R" << captured.register_index;
            } else {
                std::cout << " -> Stack offset " << captured.offset_in_parent_stack;
            }
            std::cout << std::endl;
        }
    }
    
    current_analyzing_function_ = nullptr;
}

const GoroutineCaptureInfo* LexicalScopeTracker::get_capture_info(FunctionExpression* func) const {
    auto it = goroutine_captures_.find(func);
    return (it != goroutine_captures_.end()) ? it->second.get() : nullptr;
}

const std::vector<CapturedVariable>* LexicalScopeTracker::get_captured_variables(FunctionExpression* func) const {
    const GoroutineCaptureInfo* info = get_capture_info(func);
    return info ? &info->captured_vars : nullptr;
}

bool LexicalScopeTracker::has_captures(FunctionExpression* func) const {
    const GoroutineCaptureInfo* info = get_capture_info(func);
    return info && !info->captured_vars.empty();
}

size_t LexicalScopeTracker::get_capture_count(FunctionExpression* func) const {
    const GoroutineCaptureInfo* info = get_capture_info(func);
    return info ? info->captured_vars.size() : 0;
}

void LexicalScopeTracker::print_all_captures() const {
    std::cout << "[LexicalScopeTracker] === ALL CAPTURE INFO ===" << std::endl;
    std::cout << "[LexicalScopeTracker] Total functions with captures: " << goroutine_captures_.size() << std::endl;
    
    for (const auto& [func, info] : goroutine_captures_) {
        std::cout << "[LexicalScopeTracker] Function @ " << func << ":" << std::endl;
        std::cout << "[LexicalScopeTracker]   Variables: " << info->captured_vars.size() << std::endl;
        std::cout << "[LexicalScopeTracker]   Scope size: " << info->total_lexical_scope_size << " bytes" << std::endl;
        
        for (const auto& captured : info->captured_vars) {
            std::cout << "[LexicalScopeTracker]     - " << captured.name 
                      << " (" << captured.type << ")";
            if (captured.use_register) {
                std::cout << " -> R" << captured.register_index;
            } else {
                std::cout << " -> Stack+" << captured.offset_in_parent_stack;
            }
            std::cout << std::endl;
        }
    }
    std::cout << "[LexicalScopeTracker] ==========================" << std::endl;
}

void LexicalScopeTracker::allocate_storage_for_captures(GoroutineCaptureInfo* info) {
    if (!info) return;
    
    std::cout << "[LexicalScopeTracker] Allocating storage for " << info->captured_vars.size() << " captured variables" << std::endl;
    
    int next_register = 0;
    const int MAX_REGISTER_PARAMS = 6;  // x86_64 calling convention
    
    for (auto& captured : info->captured_vars) {
        // For now, use simple allocation strategy:
        // - Small types (int, float, pointers) prefer registers if available
        // - Large types go to stack
        
        if (should_use_register(captured.type) && next_register < MAX_REGISTER_PARAMS) {
            captured.use_register = true;
            captured.register_index = next_register++;
            std::cout << "[LexicalScopeTracker]   " << captured.name 
                      << " -> Register R" << captured.register_index << std::endl;
        } else {
            captured.use_register = false;
            captured.register_index = -1;
            // Stack offset will be calculated later during code generation
            std::cout << "[LexicalScopeTracker]   " << captured.name 
                      << " -> Stack (offset TBD)" << std::endl;
        }
    }
}

void LexicalScopeTracker::calculate_lexical_scope_size(GoroutineCaptureInfo* info) {
    if (!info) return;
    
    size_t total_size = 0;
    
    // Calculate size needed for stack-allocated variables
    for (const auto& captured : info->captured_vars) {
        if (!captured.use_register) {
            size_t var_size = get_type_size(captured.type);
            total_size += var_size;
            std::cout << "[LexicalScopeTracker]   " << captured.name 
                      << " (" << captured.type << ") needs " << var_size << " bytes" << std::endl;
        }
    }
    
    // Round up to 8-byte alignment
    total_size = (total_size + 7) & ~7;
    
    info->total_lexical_scope_size = total_size;
    std::cout << "[LexicalScopeTracker] Total lexical scope size: " << total_size << " bytes (aligned)" << std::endl;
}

size_t LexicalScopeTracker::get_type_size(const std::string& type) const {
    if (type.empty() || type == "auto" || type == "any") {
        return 8;  // Pointer to dynamic value
    } else if (type == "int" || type == "int32") {
        return 4;
    } else if (type == "int64" || type == "float64" || type == "number") {
        return 8;
    } else if (type == "float32") {
        return 4;
    } else if (type == "string") {
        return 8;  // Pointer to string object
    } else if (type == "bool") {
        return 1;
    } else {
        return 8;  // Default to pointer size for objects
    }
}

bool LexicalScopeTracker::should_use_register(const std::string& type) const {
    // Small primitive types prefer registers
    return type == "int" || type == "int32" || type == "int64" || 
           type == "float32" || type == "float64" || type == "number" ||
           type == "bool" || type == "string" || 
           type.empty() || type == "auto" || type == "any";
}
