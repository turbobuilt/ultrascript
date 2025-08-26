// Simple test for lexical scope analysis
#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>

// Simple scope analyzer that doesn't depend on AST nodes
class SimpleScopeAnalyzer {
private:
    struct Variable {
        std::string name;
        int declared_scope_level;
        bool accessed_from_child_scope = false;
    };
    
    struct FunctionInfo {
        std::string name;
        std::vector<Variable> declared_variables;
        std::vector<std::string> accessed_parent_variables;
        std::unordered_set<int> required_parent_scope_levels;
    };
    
    std::unordered_map<std::string, FunctionInfo> function_map;
    
public:
    void add_function(const std::string& func_name) {
        function_map[func_name] = FunctionInfo{func_name, {}, {}, {}};
    }
    
    void declare_variable(const std::string& func_name, const std::string& var_name, int scope_level) {
        if (function_map.find(func_name) != function_map.end()) {
            function_map[func_name].declared_variables.push_back({var_name, scope_level, false});
            std::cout << "[DEBUG] Declared variable '" << var_name << "' at scope level " << scope_level 
                      << " in function '" << func_name << "'" << std::endl;
        }
    }
    
    void access_parent_variable(const std::string& func_name, const std::string& var_name, int access_scope_level) {
        if (function_map.find(func_name) != function_map.end()) {
            function_map[func_name].accessed_parent_variables.push_back(var_name);
            
            // Calculate which parent scope this variable comes from
            // For simplicity, assume parent scope is access_scope_level - 1
            int parent_scope_level = access_scope_level - 1;
            if (parent_scope_level >= 0) {
                function_map[func_name].required_parent_scope_levels.insert(parent_scope_level);
            }
            
            std::cout << "[DEBUG] Function '" << func_name << "' accesses parent variable '" << var_name 
                      << "' from parent scope level " << parent_scope_level << std::endl;
        }
    }
    
    void compute_register_allocation(const std::string& func_name) {
        if (function_map.find(func_name) == function_map.end()) {
            return;
        }
        
        auto& func_info = function_map[func_name];
        std::vector<int> registers = {12, 13, 14, 15}; // r12, r13, r14, r15
        
        std::cout << "[DEBUG] Computing register allocation for function '" << func_name << "'" << std::endl;
        std::cout << "[DEBUG] Function needs parent scope levels: ";
        for (int level : func_info.required_parent_scope_levels) {
            std::cout << level << " ";
        }
        std::cout << std::endl;
        
        int reg_idx = 0;
        for (int scope_level : func_info.required_parent_scope_levels) {
            if (reg_idx < registers.size()) {
                std::cout << "[DEBUG] Assigning parent scope level " << scope_level 
                          << " to register r" << registers[reg_idx] << std::endl;
                reg_idx++;
            } else {
                std::cout << "[DEBUG] Parent scope level " << scope_level 
                          << " requires stack fallback (out of registers)" << std::endl;
            }
        }
    }
    
    void print_analysis() {
        std::cout << "\n=== SCOPE ANALYSIS RESULTS ===" << std::endl;
        for (const auto& [func_name, func_info] : function_map) {
            std::cout << "Function: " << func_name << std::endl;
            std::cout << "  Declared variables:" << std::endl;
            for (const auto& var : func_info.declared_variables) {
                std::cout << "    " << var.name << " (scope level " << var.declared_scope_level << ")" << std::endl;
            }
            std::cout << "  Accesses parent variables:" << std::endl;
            for (const auto& var : func_info.accessed_parent_variables) {
                std::cout << "    " << var << std::endl;
            }
            std::cout << "  Required parent scope levels: ";
            for (int level : func_info.required_parent_scope_levels) {
                std::cout << level << " ";
            }
            std::cout << std::endl << std::endl;
        }
    }
};

int main() {
    std::cout << "=== TESTING LEXICAL SCOPE ANALYSIS ===" << std::endl;
    std::cout << "Analyzing test_scope.gts structure:" << std::endl;
    std::cout << "var x = 5;  // Global scope (level 0)" << std::endl;
    std::cout << "let result = go function() {  // Function scope (level 1)" << std::endl;
    std::cout << "    var y = 0;  // Local to function (level 1)" << std::endl;
    std::cout << "    console.log(y);  // Uses local y" << std::endl;
    std::cout << "    console.log('X is', x);  // Uses parent x" << std::endl;
    std::cout << "}" << std::endl;
    std::cout << std::endl;
    
    SimpleScopeAnalyzer analyzer;
    
    // Simulate global scope (level 0)
    analyzer.add_function("global");
    analyzer.declare_variable("global", "x", 0);
    analyzer.declare_variable("global", "result", 0);
    
    // Simulate the goroutine function (level 1) 
    analyzer.add_function("goroutine_function");
    analyzer.declare_variable("goroutine_function", "y", 1);
    
    // The goroutine function accesses parent variable 'x' from scope level 0
    analyzer.access_parent_variable("goroutine_function", "x", 1);
    
    // Compute register allocation
    analyzer.compute_register_allocation("global");
    analyzer.compute_register_allocation("goroutine_function");
    
    // Print results
    analyzer.print_analysis();
    
    std::cout << "=== EXPECTED RESULTS ===" << std::endl;
    std::cout << "Global scope has variables: x (level 0), result (level 0)" << std::endl;
    std::cout << "Goroutine function has variables: y (level 1)" << std::endl;
    std::cout << "Goroutine function accesses parent variable: x (from level 0)" << std::endl;
    std::cout << "Goroutine function requires parent scope level: 0" << std::endl;
    std::cout << "Register allocation: parent scope level 0 -> r12" << std::endl;
    
    return 0;
}
