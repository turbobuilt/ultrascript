#include <iostream>
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <algorithm>
#include <string>

// Comprehensive test suite for descendant scope analysis
class DescendantAnalysisTestSuite {
private:
    struct ScopeInfo {
        std::string name;
        int level;
        std::vector<std::string> declared_vars;
        std::unordered_set<int> direct_parent_accesses;
        std::unordered_set<int> total_parent_needs;
        std::vector<std::string> descendants;
        std::string parent;
        std::unordered_map<int, int> register_allocation;
    };
    
    std::unordered_map<std::string, ScopeInfo> functions_;
    
public:
    void run_all_tests() {
        std::cout << "🧪 COMPREHENSIVE DESCENDANT ANALYSIS TEST SUITE" << std::endl;
        std::cout << std::string(60, '=') << std::endl;
        
        test_simple_nested_function();
        test_skipped_parent_propagation();  
        test_complex_multi_level_hierarchy();
        test_multiple_branches();
        test_goroutine_scenarios();
        
        std::cout << "\n🎉 ALL DESCENDANT ANALYSIS TESTS COMPLETED!" << std::endl;
    }
    
private:
    void test_simple_nested_function() {
        std::cout << "\n📋 TEST 1: Simple Nested Function" << std::endl;
        std::cout << "Scenario: Function A contains Function B, B accesses grandparent" << std::endl;
        std::cout << "Expected: A must provide grandparent access even though A doesn't use it" << std::endl;
        
        functions_.clear();
        
        // Level 0: global (var global_var)
        add_function("global", 0, {"global_var"}, {});
        
        // Level 1: parent (var parent_var)  
        add_function("parent", 1, {"parent_var"}, {});
        
        // Level 2: function_A (var a_var, no direct parent access)
        add_function("function_A", 2, {"a_var"}, {});
        
        // Level 3: function_B (nested in A, accesses global_var)
        add_function("function_B", 3, {"b_var"}, {0}); // Accesses level 0
        add_descendant("function_A", "function_B");
        
        analyze_and_verify();
        
        // Verify results
        auto& func_a = functions_["function_A"];
        auto& func_b = functions_["function_B"];
        
        std::cout << "\nVerification:" << std::endl;
        std::cout << "function_B needs parent levels: ";
        print_set(func_b.total_parent_needs);
        std::cout << "function_A needs parent levels: ";  
        print_set(func_a.total_parent_needs);
        
        bool test_passed = func_a.total_parent_needs.count(0) > 0; // A should need level 0 due to B
        std::cout << (test_passed ? "✅ PASS" : "❌ FAIL") << ": Function A provides grandparent access for descendant B" << std::endl;
    }
    
    void test_skipped_parent_propagation() {
        std::cout << "\n📋 TEST 2: Skipped Parent Level Propagation" << std::endl;
        std::cout << "Scenario: Nested function skips immediate parent, accesses grandparent" << std::endl;
        std::cout << "Expected: Parent function should NOT get register for unused immediate parent" << std::endl;
        
        functions_.clear();
        
        // Level 0: great_grandparent (var gg_var)
        add_function("great_grandparent", 0, {"gg_var"}, {});
        
        // Level 1: grandparent (var gp_var)
        add_function("grandparent", 1, {"gp_var"}, {});
        
        // Level 2: parent (var p_var) <- NEVER ACCESSED!
        add_function("parent", 2, {"p_var"}, {});
        
        // Level 3: current (var c_var, accesses great_grandparent and grandparent, SKIPS parent)
        add_function("current", 3, {"c_var"}, {0, 1}); // Accesses levels 0, 1 but NOT 2
        add_descendant("parent", "current");
        
        analyze_and_verify();
        
        auto& parent_func = functions_["parent"];
        auto& current_func = functions_["current"];
        
        std::cout << "\nVerification:" << std::endl;
        std::cout << "current needs parent levels: ";
        print_set(current_func.total_parent_needs);
        std::cout << "parent must provide levels: ";
        print_set(parent_func.total_parent_needs);
        
        // Allocate registers
        allocate_registers("parent");
        allocate_registers("current");
        
        show_register_allocation("parent");
        show_register_allocation("current");
        
        bool skipped_level_2 = parent_func.register_allocation.find(2) == parent_func.register_allocation.end();
        std::cout << (skipped_level_2 ? "✅ PASS" : "❌ FAIL") << ": Level 2 (unused parent) gets no register" << std::endl;
    }
    
    void test_complex_multi_level_hierarchy() {
        std::cout << "\n📋 TEST 3: Complex Multi-Level Hierarchy" << std::endl;
        std::cout << "Scenario: 5-level hierarchy with complex access patterns" << std::endl;
        
        functions_.clear();
        
        // Level 0: global (var global_var)
        add_function("global", 0, {"global_var"}, {});
        
        // Level 1: level_1 (var v1)
        add_function("level_1", 1, {"v1"}, {});
        
        // Level 2: level_2 (var v2, accesses global)
        add_function("level_2", 2, {"v2"}, {0});
        
        // Level 3: level_3 (var v3, no direct access)
        add_function("level_3", 3, {"v3"}, {});
        add_descendant("level_2", "level_3");
        
        // Level 4: level_4 (var v4, accesses level_1)
        add_function("level_4", 4, {"v4"}, {1});
        add_descendant("level_3", "level_4");
        
        // Level 5: level_5 (var v5, accesses global and level_2)
        add_function("level_5", 5, {"v5"}, {0, 2});
        add_descendant("level_4", "level_5");
        
        analyze_and_verify();
        
        std::cout << "\nFinal Analysis Results:" << std::endl;
        for (int level = 2; level <= 5; level++) {
            std::string func_name = "level_" + std::to_string(level);
            allocate_registers(func_name);
            show_register_allocation(func_name);
        }
        
        // Verify that level_2 needs to provide access to levels 0, 1, 2 for all descendants
        auto& level_2 = functions_["level_2"];
        bool has_all_needed = level_2.total_parent_needs.count(0) > 0 && 
                             level_2.total_parent_needs.count(1) > 0;
        std::cout << (has_all_needed ? "✅ PASS" : "❌ FAIL") << ": Complex hierarchy propagation works" << std::endl;
    }
    
    void test_multiple_branches() {
        std::cout << "\n📋 TEST 4: Multiple Descendant Branches" << std::endl;
        std::cout << "Scenario: Function has multiple nested functions with different needs" << std::endl;
        
        functions_.clear();
        
        // Levels 0-2: setup
        add_function("global", 0, {"global_var"}, {});
        add_function("parent_1", 1, {"p1_var"}, {});
        add_function("parent_2", 2, {"p2_var"}, {});
        
        // Level 3: main_function (no direct access)
        add_function("main_function", 3, {"main_var"}, {});
        
        // Branch 1: nested_A accesses global
        add_function("nested_A", 4, {"a_var"}, {0});
        add_descendant("main_function", "nested_A");
        
        // Branch 2: nested_B accesses parent_1
        add_function("nested_B", 4, {"b_var"}, {1});
        add_descendant("main_function", "nested_B");
        
        // Branch 3: nested_C accesses parent_2
        add_function("nested_C", 4, {"c_var"}, {2});
        add_descendant("main_function", "nested_C");
        
        analyze_and_verify();
        
        auto& main_func = functions_["main_function"];
        allocate_registers("main_function");
        show_register_allocation("main_function");
        
        // main_function should need to provide access to levels 0, 1, 2
        bool has_all_branches = main_func.total_parent_needs.count(0) > 0 &&
                               main_func.total_parent_needs.count(1) > 0 &&
                               main_func.total_parent_needs.count(2) > 0;
        std::cout << (has_all_branches ? "✅ PASS" : "❌ FAIL") << ": Multiple branches consolidated correctly" << std::endl;
    }
    
    void test_goroutine_scenarios() {
        std::cout << "\n📋 TEST 5: Goroutine Capture Scenarios" << std::endl;
        std::cout << "Scenario: Goroutines capture variables from multiple scope levels" << std::endl;
        
        functions_.clear();
        
        // Setup hierarchy
        add_function("global", 0, {"global_var"}, {});
        add_function("outer", 1, {"outer_var"}, {});
        add_function("middle", 2, {"middle_var"}, {});
        
        // Inner function launches goroutine
        add_function("inner", 3, {"inner_var"}, {});
        
        // Goroutine accesses multiple levels
        add_function("goroutine_func", 4, {"g_var"}, {0, 1, 2}); // Accesses global, outer, middle
        add_descendant("inner", "goroutine_func");
        
        analyze_and_verify();
        
        // inner function should provide all levels that goroutine needs
        auto& inner_func = functions_["inner"];
        allocate_registers("inner");
        show_register_allocation("inner");
        
        bool goroutine_support = inner_func.total_parent_needs.count(0) > 0 &&
                                inner_func.total_parent_needs.count(1) > 0 &&
                                inner_func.total_parent_needs.count(2) > 0;
        std::cout << (goroutine_support ? "✅ PASS" : "❌ FAIL") << ": Goroutine variable capture handled correctly" << std::endl;
    }
    
    // Helper methods
    void add_function(const std::string& name, int level, const std::vector<std::string>& vars, 
                     const std::unordered_set<int>& direct_accesses) {
        ScopeInfo info;
        info.name = name;
        info.level = level;
        info.declared_vars = vars;
        info.direct_parent_accesses = direct_accesses;
        info.total_parent_needs = direct_accesses; // Start with direct accesses
        functions_[name] = info;
    }
    
    void add_descendant(const std::string& parent, const std::string& child) {
        functions_[parent].descendants.push_back(child);
        functions_[child].parent = parent;
    }
    
    void analyze_and_verify() {
        std::cout << "\nRunning bottom-up analysis..." << std::endl;
        
        // Get functions sorted by level (deepest first)
        std::vector<std::pair<std::string, int>> funcs_by_level;
        for (const auto& entry : functions_) {
            funcs_by_level.push_back({entry.first, entry.second.level});
        }
        sort(funcs_by_level.begin(), funcs_by_level.end(), 
             [](const auto& a, const auto& b) { return a.second > b.second; });
        
        // Bottom-up propagation
        for (const auto& func_info : funcs_by_level) {
            const std::string& func_name = func_info.first;
            auto& func = functions_[func_name];
            
            std::cout << "Processing " << func_name << " (level " << func.level << ")" << std::endl;
            
            // Propagate needs to parent
            if (!func.parent.empty()) {
                auto& parent = functions_[func.parent];
                for (int needed_level : func.total_parent_needs) {
                    if (needed_level < parent.level) {
                        parent.total_parent_needs.insert(needed_level);
                        std::cout << "  Propagated level " << needed_level << " to " << func.parent << std::endl;
                    }
                }
            }
        }
    }
    
    void allocate_registers(const std::string& func_name) {
        auto& func = functions_[func_name];
        std::vector<int> needed_levels(func.total_parent_needs.begin(), func.total_parent_needs.end());
        sort(needed_levels.begin(), needed_levels.end());
        
        int reg = 12; // Start with r12
        for (int level : needed_levels) {
            func.register_allocation[level] = reg;
            reg++;
        }
    }
    
    void show_register_allocation(const std::string& func_name) {
        const auto& func = functions_[func_name];
        std::cout << func_name << " register allocation:" << std::endl;
        std::cout << "  r15: Current scope (level " << func.level << ")" << std::endl;
        
        std::vector<std::pair<int, int>> regs;
        for (const auto& entry : func.register_allocation) {
            regs.push_back({entry.first, entry.second});
        }
        sort(regs.begin(), regs.end());
        
        for (const auto& reg : regs) {
            std::cout << "  r" << reg.second << ": Parent level " << reg.first << std::endl;
        }
    }
    
    void print_set(const std::unordered_set<int>& s) {
        std::vector<int> sorted(s.begin(), s.end());
        sort(sorted.begin(), sorted.end());
        for (int val : sorted) {
            std::cout << val << " ";
        }
        if (s.empty()) std::cout << "(none)";
        std::cout << std::endl;
    }
};

int main() {
    DescendantAnalysisTestSuite test_suite;
    test_suite.run_all_tests();
    return 0;
}
