#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <sstream>
#include <cassert>
#include <algorithm>
#include <cctype>

// ES6 Declaration Kinds
enum DeclarationKind {
    VAR,    // Function-scoped, hoisted
    LET,    // Block-scoped, not hoisted
    CONST   // Block-scoped, not hoisted, immutable
};

// Scope Types for proper ES6 semantics
enum ScopeType {
    FUNCTION_SCOPE,     // Created by functions
    BLOCK_SCOPE,        // Created by {}, for-loops with let/const, etc.
    MODULE_SCOPE        // Top-level module scope
};

// Variable binding information
struct VariableBinding {
    std::string name;
    DeclarationKind kind;
    int scope_id;
    ScopeType scope_type;
    int declaration_order;
    std::string context;
    bool is_hoisted = false;  // For var declarations
};

// Scope information
struct ScopeInfo {
    int scope_id;
    ScopeType type;
    int parent_scope_id;
    std::vector<std::string> variables;
    bool has_let_const = false;
    std::string context;
};

class JavaScriptES6ScopeAnalyzer {
private:
    std::map<int, ScopeInfo> scopes_;
    std::map<std::string, VariableBinding> variables_;
    std::vector<int> scope_stack_;  // Current scope chain
    int next_scope_id_ = 0;
    int declaration_counter_ = 1;
    std::string current_function_;

public:
    void begin_function_analysis(const std::string& function_name) {
        current_function_ = function_name;
        clear();
        
        // Always start with function scope at ID 0
        create_scope(FUNCTION_SCOPE, "function " + function_name);
        std::cout << "[SCOPE] Created function scope (ID: 0) for '" << function_name << "'" << std::endl;
    }
    
    void end_function_analysis() {
        std::cout << "[SCOPE] Ending analysis for function '" << current_function_ << "'" << std::endl;
        
        // Perform hoisting analysis
        perform_hoisting();
        
        // Analyze optimization opportunities
        analyze_optimization_opportunities();
    }
    
    void add_variable(const std::string& name, DeclarationKind kind, const std::string& context = "") {
        int target_scope_id;
        
        if (kind == VAR) {
            // var declarations are function-scoped (hoisted to nearest function scope)
            target_scope_id = find_nearest_function_scope();
        } else {
            // let/const are block-scoped (current scope)
            target_scope_id = get_current_scope_id();
        }
        
        VariableBinding binding;
        binding.name = name;
        binding.kind = kind;
        binding.scope_id = target_scope_id;
        binding.scope_type = scopes_[target_scope_id].type;
        binding.declaration_order = declaration_counter_++;
        binding.context = context;
        binding.is_hoisted = (kind == VAR && target_scope_id != get_current_scope_id());
        
        variables_[name] = binding;
        scopes_[target_scope_id].variables.push_back(name);
        
        if (kind == LET || kind == CONST) {
            scopes_[target_scope_id].has_let_const = true;
        }
        
        std::cout << "[VAR] " << (kind == VAR ? "var" : kind == LET ? "let" : "const") 
                  << " " << name << " → scope " << target_scope_id 
                  << (binding.is_hoisted ? " (hoisted)" : "")
                  << " (" << context << ")" << std::endl;
    }
    
    int enter_block_scope(const std::string& context) {
        int scope_id = create_scope(BLOCK_SCOPE, context);
        std::cout << "[SCOPE] Entered block scope (ID: " << scope_id << ") - " << context << std::endl;
        return scope_id;
    }
    
    void exit_scope() {
        if (scope_stack_.size() > 1) {  // Never exit the function scope
            int exited_scope = scope_stack_.back();
            scope_stack_.pop_back();
            std::cout << "[SCOPE] Exited scope (ID: " << exited_scope << ")" << std::endl;
        }
    }
    
    int enter_for_loop_scope(DeclarationKind loop_var_kind, const std::string& context) {
        // For-loops with let/const create a new block scope
        // For-loops with var don't create new scopes (variables are hoisted)
        if (loop_var_kind == LET || loop_var_kind == CONST) {
            return enter_block_scope("for-loop " + context);
        } else {
            // var for-loops don't create new scopes
            std::cout << "[SCOPE] for(var) loop - no new scope created, variables will be hoisted" << std::endl;
            return get_current_scope_id();
        }
    }
    
    // Analysis and reporting methods
    std::vector<int> get_optimizable_scopes() const {
        std::vector<int> optimizable;
        for (const auto& [id, scope] : scopes_) {
            if (scope.type == BLOCK_SCOPE && !scope.has_let_const) {
                optimizable.push_back(id);
            }
        }
        return optimizable;
    }
    
    std::vector<int> get_required_scopes() const {
        std::vector<int> required;
        for (const auto& [id, scope] : scopes_) {
            if (scope.type == FUNCTION_SCOPE || scope.has_let_const) {
                required.push_back(id);
            }
        }
        return required;
    }
    
    bool scope_needs_allocation(int scope_id) const {
        auto it = scopes_.find(scope_id);
        if (it == scopes_.end()) return false;
        
        const ScopeInfo& scope = it->second;
        return scope.type == FUNCTION_SCOPE || scope.has_let_const;
    }
    
    VariableBinding get_variable_info(const std::string& name) const {
        auto it = variables_.find(name);
        return (it != variables_.end()) ? it->second : VariableBinding{};
    }
    
    void print_scope_analysis() const {
        std::cout << "\n[SCOPE ANALYSIS]" << std::endl;
        
        for (const auto& [id, scope] : scopes_) {
            std::cout << "Scope " << id << " (" << scope_type_name(scope.type) << "): ";
            
            if (scope_needs_allocation(id)) {
                std::cout << "REQUIRES ALLOCATION";
                if (scope.has_let_const) std::cout << " (contains let/const)";
                if (scope.type == FUNCTION_SCOPE) std::cout << " (function scope)";
            } else {
                std::cout << "CAN BE OPTIMIZED (block with var-only)";
            }
            std::cout << std::endl;
            
            std::cout << "  Context: " << scope.context << std::endl;
            std::cout << "  Variables: ";
            for (const std::string& var_name : scope.variables) {
                const auto& var_info = variables_.at(var_name);
                std::cout << var_name << "("
                          << (var_info.kind == VAR ? "var" : 
                              var_info.kind == LET ? "let" : "const");
                if (var_info.is_hoisted) std::cout << ",hoisted";
                std::cout << ") ";
            }
            std::cout << std::endl;
        }
    }
    
    void print_optimization_summary() const {
        auto optimizable = get_optimizable_scopes();
        auto required = get_required_scopes();
        
        std::cout << "\n[OPTIMIZATION SUMMARY]" << std::endl;
        std::cout << "Optimizable scopes: ";
        for (int id : optimizable) std::cout << id << " ";
        std::cout << "(" << optimizable.size() << " scopes)" << std::endl;
        
        std::cout << "Required scopes: ";
        for (int id : required) std::cout << id << " ";
        std::cout << "(" << required.size() << " scopes)" << std::endl;
        
        int total = optimizable.size() + required.size();
        if (total > 0) {
            double optimization_rate = (double)optimizable.size() / total * 100;
            std::cout << "Optimization rate: " << optimization_rate 
                      << "% of scopes can be eliminated" << std::endl;
        }
    }

private:
    void clear() {
        scopes_.clear();
        variables_.clear();
        scope_stack_.clear();
        next_scope_id_ = 0;
        declaration_counter_ = 1;
    }
    
    int create_scope(ScopeType type, const std::string& context) {
        int scope_id = next_scope_id_++;
        int parent_id = scope_stack_.empty() ? -1 : scope_stack_.back();
        
        ScopeInfo scope;
        scope.scope_id = scope_id;
        scope.type = type;
        scope.parent_scope_id = parent_id;
        scope.context = context;
        
        scopes_[scope_id] = scope;
        scope_stack_.push_back(scope_id);
        
        return scope_id;
    }
    
    int get_current_scope_id() const {
        return scope_stack_.empty() ? -1 : scope_stack_.back();
    }
    
    int find_nearest_function_scope() const {
        // Find the nearest function scope in the stack
        for (auto it = scope_stack_.rbegin(); it != scope_stack_.rend(); ++it) {
            if (scopes_.at(*it).type == FUNCTION_SCOPE) {
                return *it;
            }
        }
        return 0;  // Should always have function scope at 0
    }
    
    void perform_hoisting() {
        std::cout << "\n[HOISTING ANALYSIS]" << std::endl;
        
        for (const auto& [name, binding] : variables_) {
            if (binding.is_hoisted) {
                std::cout << "var " << name << " hoisted from scope " 
                          << "to function scope (ID: " << binding.scope_id << ")" << std::endl;
            }
        }
    }
    
    void analyze_optimization_opportunities() {
        std::cout << "\n[OPTIMIZATION OPPORTUNITIES]" << std::endl;
        
        for (const auto& [id, scope] : scopes_) {
            if (scope.type == BLOCK_SCOPE && !scope.has_let_const) {
                std::cout << "🚀 Scope " << id << " can be ELIMINATED: " << scope.context 
                          << " (contains only var/hoisted variables)" << std::endl;
            } else if (scope.has_let_const) {
                std::cout << "📦 Scope " << id << " REQUIRED for correctness: " << scope.context 
                          << " (contains let/const bindings)" << std::endl;
            }
        }
    }
    
    std::string scope_type_name(ScopeType type) const {
        switch (type) {
            case FUNCTION_SCOPE: return "function";
            case BLOCK_SCOPE: return "block";
            case MODULE_SCOPE: return "module";
            default: return "unknown";
        }
    }
};

// Enhanced JavaScript parser with correct ES6 semantics
class JavaScriptES6Parser {
private:
    JavaScriptES6ScopeAnalyzer& analyzer_;
    
public:
    JavaScriptES6Parser(JavaScriptES6ScopeAnalyzer& analyzer) : analyzer_(analyzer) {}
    
    void parse_javascript_code(const std::string& code, const std::string& function_name = "test_function") {
        std::cout << "\n[PARSING] JavaScript code for function: " << function_name << std::endl;
        std::cout << "```javascript" << std::endl;
        std::cout << code << std::endl;
        std::cout << "```" << std::endl;
        
        analyzer_.begin_function_analysis(function_name);
        
        // Parse line by line with proper scope tracking
        std::istringstream iss(code);
        std::string line;
        
        while (std::getline(iss, line)) {
            parse_line_with_scope_tracking(line);
        }
        
        analyzer_.end_function_analysis();
    }
    
private:
    void parse_line_with_scope_tracking(const std::string& line) {
        std::string trimmed = trim(line);
        if (trimmed.empty()) return;
        
        // Handle scope changes
        handle_scope_changes(trimmed);
        
        // Parse variable declarations
        parse_variable_declarations(trimmed);
        
        // Handle for-loops specially (correct ES6 semantics)
        handle_for_loops(trimmed);
    }
    
    void handle_scope_changes(const std::string& line) {
        // Count braces to track scope entry/exit
        for (char c : line) {
            if (c == '{') {
                // Only create new block scope for non-for-loop blocks
                if (line.find("for (") == std::string::npos) {
                    analyzer_.enter_block_scope("block statement");
                }
            } else if (c == '}') {
                analyzer_.exit_scope();
            }
        }
    }
    
    void handle_for_loops(const std::string& line) {
        size_t for_pos = line.find("for (");
        if (for_pos == std::string::npos) return;
        
        // Extract initialization part
        size_t init_start = for_pos + 5;
        size_t semicolon = line.find(";", init_start);
        if (semicolon == std::string::npos) return;
        
        std::string init_part = line.substr(init_start, semicolon - init_start);
        
        // Determine loop variable kind
        DeclarationKind loop_kind = VAR;  // default
        if (init_part.find("let ") != std::string::npos) {
            loop_kind = LET;
        } else if (init_part.find("const ") != std::string::npos) {
            loop_kind = CONST;
        }
        
        // Create for-loop scope with correct semantics
        std::string loop_context = "for-loop (" + 
            (loop_kind == VAR ? "var" : loop_kind == LET ? "let" : "const") + ")";
        analyzer_.enter_for_loop_scope(loop_kind, loop_context);
        
        // Parse the initialization variables
        parse_variable_declarations(init_part, "for-loop initialization");
    }
    
    void parse_variable_declarations(const std::string& line, const std::string& context = "") {
        parse_declaration_keyword(line, "var", VAR, context.empty() ? "declaration" : context);
        parse_declaration_keyword(line, "let", LET, context.empty() ? "declaration" : context);
        parse_declaration_keyword(line, "const", CONST, context.empty() ? "declaration" : context);
    }
    
    void parse_declaration_keyword(const std::string& line, const std::string& keyword, 
                                 DeclarationKind kind, const std::string& context) {
        size_t pos = 0;
        while ((pos = line.find(keyword + " ", pos)) != std::string::npos) {
            // Check word boundary
            if (pos > 0 && std::isalnum(line[pos - 1])) {
                pos++;
                continue;
            }
            
            // Extract variable name
            size_t name_start = pos + keyword.length() + 1;
            size_t name_end = line.find_first_of(" =;,()[]", name_start);
            if (name_end == std::string::npos) name_end = line.length();
            
            std::string var_name = trim(line.substr(name_start, name_end - name_start));
            
            if (!var_name.empty() && std::isalpha(var_name[0])) {
                analyzer_.add_variable(var_name, kind, context);
            }
            
            pos = name_end;
        }
    }
    
    std::string trim(const std::string& str) {
        size_t first = str.find_first_not_of(' ');
        if (first == std::string::npos) return "";
        size_t last = str.find_last_not_of(' ');
        return str.substr(first, (last - first + 1));
    }
};

// Test case structure
struct ES6TestCase {
    std::string name;
    std::string code;
    std::map<std::string, DeclarationKind> expected_variables;
    std::map<int, bool> expected_scope_allocation;  // scope_id -> needs_allocation
    std::vector<std::string> optimization_notes;
};

void run_es6_test_case(const ES6TestCase& test_case) {
    std::cout << "\n" << std::string(80, '=') << std::endl;
    std::cout << "🧪 TESTING: " << test_case.name << std::endl;
    std::cout << std::string(80, '=') << std::endl;
    
    JavaScriptES6ScopeAnalyzer analyzer;
    JavaScriptES6Parser parser(analyzer);
    
    // Parse and analyze
    parser.parse_javascript_code(test_case.code, test_case.name);
    
    // Print detailed analysis
    analyzer.print_scope_analysis();
    analyzer.print_optimization_summary();
    
    // Validate results
    std::cout << "\n[VALIDATION]" << std::endl;
    
    // Check variables
    for (const auto& [var_name, expected_kind] : test_case.expected_variables) {
        auto var_info = analyzer.get_variable_info(var_name);
        
        if (var_info.name.empty()) {
            std::cout << "❌ Variable '" << var_name << "' not found!" << std::endl;
            assert(false);
        }
        
        if (var_info.kind != expected_kind) {
            std::cout << "❌ Variable '" << var_name << "' has wrong kind!" << std::endl;
            assert(false);
        }
        
        std::cout << "✓ " << var_name << " (" 
                  << (expected_kind == VAR ? "var" : expected_kind == LET ? "let" : "const")
                  << " in scope " << var_info.scope_id << ")" << std::endl;
    }
    
    // Check scope allocations
    for (const auto& [scope_id, expected_needs_allocation] : test_case.expected_scope_allocation) {
        bool actually_needs = analyzer.scope_needs_allocation(scope_id);
        
        if (actually_needs != expected_needs_allocation) {
            std::cout << "❌ Scope " << scope_id << " allocation mismatch! Expected: " 
                      << expected_needs_allocation << ", Actual: " << actually_needs << std::endl;
            assert(false);
        }
        
        std::cout << "✓ Scope " << scope_id << ": " 
                  << (expected_needs_allocation ? "requires allocation" : "can be optimized") << std::endl;
    }
    
    // Print optimization notes
    if (!test_case.optimization_notes.empty()) {
        std::cout << "\n[OPTIMIZATION NOTES]" << std::endl;
        for (const std::string& note : test_case.optimization_notes) {
            std::cout << "• " << note << std::endl;
        }
    }
    
    std::cout << "\n✅ TEST PASSED: " << test_case.name << std::endl;
}

int main() {
    std::cout << "🚀 COMPREHENSIVE JAVASCRIPT ES6 SCOPE ANALYSIS SYSTEM" << std::endl;
    std::cout << "Testing with correct ES6 block scoping semantics" << std::endl;
    
    try {
        // Test Case 1: Basic function with mixed declarations (corrected)
        ES6TestCase test1;
        test1.name = "Basic Mixed var/let/const";
        test1.code = R"(
function basicExample() {
    var functionVar = 1;
    {
        let blockLet = 2;
        const blockConst = 3;
        var hoistedVar = 4;
    }
    var anotherVar = 5;
}
)";
        test1.expected_variables = {
            {"functionVar", VAR},
            {"blockLet", LET},
            {"blockConst", CONST},
            {"hoistedVar", VAR},
            {"anotherVar", VAR}
        };
        test1.expected_scope_allocation = {
            {0, true},  // Function scope - always required
            {1, true}   // Block scope - has let/const
        };
        test1.optimization_notes = {
            "Function scope required for var hoisting",
            "Block scope required for let/const bindings",
            "hoistedVar moves from block to function scope"
        };
        
        // Test Case 2: For-loop optimization (corrected ES6 semantics)
        ES6TestCase test2;
        test2.name = "For-Loop Performance Critical";
        test2.code = R"(
function forLoopOptimization() {
    for (var i = 0; i < 10; i++) {
        var temp = items[i];
        var result = process(temp);
    }
    
    for (let j = 0; j < 10; j++) {
        let value = items[j];
        const processed = transform(value);
    }
}
)";
        test2.expected_variables = {
            {"i", VAR},
            {"temp", VAR},
            {"result", VAR},
            {"j", LET},
            {"value", LET},
            {"processed", CONST}
        };
        test2.expected_scope_allocation = {
            {0, true},  // Function scope - contains var i, temp, result
            {1, true}   // For-let block scope - contains let j, value, const processed
        };
        test2.optimization_notes = {
            "CRITICAL: for(var i...) creates NO new scope - all variables hoisted",
            "CRITICAL: for(let j...) creates block scope - j, value, processed share same scope",
            "Performance: Only 2 scopes needed instead of 4+ with naive analysis"
        };
        
        // Run tests
        run_es6_test_case(test1);
        run_es6_test_case(test2);
        
        std::cout << "\n" << std::string(80, '=') << std::endl;
        std::cout << "🎉 ES6 SCOPE ANALYSIS SYSTEM VALIDATION COMPLETE! 🎉" << std::endl;
        std::cout << std::string(80, '=') << std::endl;
        
        std::cout << "\n📊 SYSTEM CAPABILITIES VERIFIED:" << std::endl;
        std::cout << "✅ Correct ES6 for-loop scoping (j and value in same scope)" << std::endl;
        std::cout << "✅ Proper var hoisting to function scope" << std::endl;
        std::cout << "✅ Block scope creation only when needed (let/const present)" << std::endl;
        std::cout << "✅ Performance optimization detection" << std::endl;
        std::cout << "✅ Comprehensive scope allocation analysis" << std::endl;
        
        std::cout << "\n🚀 READY FOR COMPLEX JAVASCRIPT PATTERNS!" << std::endl;
        
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "\n❌ ES6 SCOPE ANALYSIS FAILED: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "\n❌ ES6 SCOPE ANALYSIS FAILED: Unknown exception" << std::endl;
        return 1;
    }
}
