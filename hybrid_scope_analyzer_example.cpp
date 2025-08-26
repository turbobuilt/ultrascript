// hybrid_scope_analyzer.cpp - Best of both worlds approach

class HybridScopeAnalyzer {
private:
    // Phase 1: Incremental variable collection (simple, streaming)
    std::vector<VariableInfo> collected_variables_;
    std::map<int, std::vector<std::string>> scope_variables_;
    
    // Phase 2: Function-level optimization (complex, holistic)
    FunctionScopeAnalysis complete_analysis_;
    
public:
    // Phase 1: Process variables incrementally as we encounter them
    void collect_variable(const std::string& name, int scope_level, DataType type, ASTNode* context) {
        std::cout << "📝 Collecting variable: " << name << " at scope " << scope_level << "\n";
        
        VariableInfo info;
        info.name = name;
        info.scope_level = scope_level;
        info.type = type;
        info.size_bytes = get_variable_size(type);
        info.alignment_requirement = get_alignment_requirement(type);
        
        // Simple individual analysis
        info.access_frequency = estimate_frequency_from_context(name, context);
        info.is_hot_variable = (info.access_frequency > 50);
        
        collected_variables_.push_back(info);
        scope_variables_[scope_level].push_back(name);
    }
    
    // Phase 2: Function-level optimization after all variables collected
    void optimize_complete_function(const std::string& function_name) {
        std::cout << "🎯 Optimizing complete function: " << function_name << "\n";
        
        // Now we have complete picture - do sophisticated optimizations
        analyze_cross_variable_patterns();
        optimize_variable_ordering();
        calculate_optimal_offsets();
        determine_register_allocation();
        generate_jit_metadata();
        
        std::cout << "✅ Function-level optimization complete\n";
    }
    
private:
    void analyze_cross_variable_patterns() {
        // This is where function-level analysis shines
        std::cout << "  🔍 Analyzing cross-variable patterns...\n";
        
        for (auto& var : collected_variables_) {
            // Look for variables accessed together
            for (auto& other_var : collected_variables_) {
                if (var.name != other_var.name && 
                    are_co_accessed(var.name, other_var.name)) {
                    var.co_accessed_variables.push_back(other_var.name);
                }
            }
        }
    }
    
    void optimize_variable_ordering() {
        std::cout << "  📊 Optimizing variable ordering across all scopes...\n";
        
        for (auto& [scope_level, var_names] : scope_variables_) {
            // Function-level optimization: see all variables in scope
            std::sort(var_names.begin(), var_names.end(), 
                [this](const std::string& a, const std::string& b) {
                    auto& var_a = find_variable(a);
                    auto& var_b = find_variable(b);
                    
                    // Hot variables first
                    if (var_a.is_hot_variable != var_b.is_hot_variable) {
                        return var_a.is_hot_variable > var_b.is_hot_variable;
                    }
                    
                    // Then by frequency
                    if (var_a.access_frequency != var_b.access_frequency) {
                        return var_a.access_frequency > var_b.access_frequency;
                    }
                    
                    // Then by alignment for packing
                    return var_a.alignment_requirement > var_b.alignment_requirement;
                });
            
            std::cout << "    Scope " << scope_level << " order: ";
            for (const auto& var : var_names) {
                std::cout << var << " ";
            }
            std::cout << "\n";
        }
    }
    
    void calculate_optimal_offsets() {
        std::cout << "  📏 Calculating optimal memory offsets...\n";
        
        for (auto& [scope_level, var_names] : scope_variables_) {
            size_t current_offset = 0;
            
            for (const std::string& var_name : var_names) {
                auto& var_info = find_variable(var_name);
                
                // Align offset
                current_offset = align_offset(current_offset, var_info.alignment_requirement);
                
                // Set offset
                var_info.offset_in_scope = current_offset;
                
                std::cout << "    " << var_name << ": offset=" << current_offset 
                          << ", size=" << var_info.size_bytes << "B\n";
                
                current_offset += var_info.size_bytes;
            }
        }
    }
    
    void determine_register_allocation() {
        std::cout << "  🎯 Determining optimal register allocation...\n";
        
        // Function-level optimization: see all scope levels to prioritize
        std::vector<int> scope_levels;
        for (const auto& [level, vars] : scope_variables_) {
            if (level > 0) scope_levels.push_back(level);
        }
        
        // Sort by optimization criteria
        std::sort(scope_levels.begin(), scope_levels.end(), 
            [this](int a, int b) {
                bool a_has_hot = has_hot_variables_in_scope(a);
                bool b_has_hot = has_hot_variables_in_scope(b);
                
                if (a_has_hot != b_has_hot) return a_has_hot > b_has_hot;
                return a < b; // Lower levels first
            });
        
        // Allocate registers
        std::vector<std::string> registers = {"r12", "r13", "r14"};
        for (size_t i = 0; i < scope_levels.size() && i < 3; i++) {
            complete_analysis_.register_allocation[scope_levels[i]] = registers[i];
            std::cout << "    " << registers[i] << ": Scope level " << scope_levels[i] << "\n";
        }
    }
    
    // Helper methods...
    VariableInfo& find_variable(const std::string& name) {
        for (auto& var : collected_variables_) {
            if (var.name == name) return var;
        }
        throw std::runtime_error("Variable not found: " + name);
    }
    
    bool are_co_accessed(const std::string& var1, const std::string& var2) {
        // Analyze if variables are accessed together (simplified)
        return (var1.find("array") != std::string::npos && var2.find("index") != std::string::npos) ||
               (var1.find("loop") != std::string::npos && var2.find("counter") != std::string::npos);
    }
    
    bool has_hot_variables_in_scope(int scope_level) {
        for (const std::string& var_name : scope_variables_[scope_level]) {
            if (find_variable(var_name).is_hot_variable) return true;
        }
        return false;
    }
    
    size_t get_variable_size(DataType type) { /* implementation */ return 8; }
    size_t get_alignment_requirement(DataType type) { /* implementation */ return 8; }
    int estimate_frequency_from_context(const std::string& name, ASTNode* context) { 
        if (name.find("loop") != std::string::npos) return 100;
        if (name.find("temp") != std::string::npos) return 60;
        return 30;
    }
    size_t align_offset(size_t offset, size_t alignment) { 
        return alignment > 1 ? ((offset + alignment - 1) / alignment) * alignment : offset;
    }
};

// Usage example:
void demonstrate_hybrid_approach() {
    std::cout << "🔧 HYBRID APPROACH DEMONSTRATION\n";
    std::cout << "================================\n\n";
    
    HybridScopeAnalyzer analyzer;
    
    // Phase 1: Incremental collection (simple, as we parse)
    std::cout << "Phase 1: Incremental Variable Collection\n";
    analyzer.collect_variable("loop_index", 1, DataType::INTEGER, nullptr);
    analyzer.collect_variable("array_data", 1, DataType::ARRAY, nullptr);
    analyzer.collect_variable("temp_result", 1, DataType::DOUBLE, nullptr);
    analyzer.collect_variable("config_flag", 1, DataType::BOOLEAN, nullptr);
    
    // Phase 2: Function-level optimization (complex, after complete picture)
    std::cout << "\nPhase 2: Function-Level Optimization\n";
    analyzer.optimize_complete_function("example_function");
    
    std::cout << "\n✅ Hybrid approach complete!\n";
    std::cout << "Benefits: Simple incremental + powerful holistic optimization\n";
}
