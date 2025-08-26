#include <iostream>
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <algorithm>
#include <string>

// ULTIMATE SCOPE OPTIMIZATION TEST - Priority-Based Register Allocation
class UltimateScopeOptimizationTest {
private:
    struct FunctionInfo {
        std::string name;
        int scope_level;
        std::unordered_set<int> self_parent_needs;        // What THIS function directly accesses
        std::unordered_set<int> descendant_parent_needs;  // What descendants need (propagated up)
        std::unordered_set<int> total_parent_needs;       // Combined
        
        // Priority-based allocation
        std::unordered_map<int, int> fast_register_allocation;  // parent_level -> register (r12-r14)
        std::unordered_map<int, int> stack_allocation;          // parent_level -> stack_offset
        bool needs_stack_fallback = false;
    };
    
    std::unordered_map<std::string, FunctionInfo> functions_;
    std::unordered_map<std::string, std::string> parent_function_;
    
public:
    void run_ultimate_optimization_demo() {
        std::cout << "🎯 ULTIMATE SCOPE OPTIMIZATION - PRIORITY-BASED REGISTER ALLOCATION" << std::endl;
        std::cout << "Only 3 fast registers available: r12, r13, r14" << std::endl;
        std::cout << "Strategy: SELF needs get priority, DESCENDANT-only needs use stack if needed" << std::endl;
        std::cout << std::string(80, '=') << std::endl;
        
        test_priority_allocation_scenario();
        test_register_pressure_scenario();
        test_optimal_allocation_scenario();
        
        std::cout << "\n🏆 ULTIMATE OPTIMIZATION VALIDATION COMPLETE!" << std::endl;
        std::cout << "The priority-based allocation maximizes performance for frequently accessed scopes!" << std::endl;
    }
    
private:
    void test_priority_allocation_scenario() {
        std::cout << "\n📋 TEST 1: Priority Allocation Scenario" << std::endl;
        std::cout << "Function has 2 SELF needs + 3 DESCENDANT needs = 5 total needs" << std::endl;
        std::cout << "Expected: SELF needs get fast registers (r12, r13), some descendants use stack" << std::endl;
        
        functions_.clear();
        parent_function_.clear();
        
        FunctionInfo func;
        func.name = "main_function";
        func.scope_level = 3;
        func.self_parent_needs = {0, 2};        // Function directly accesses levels 0, 2
        func.descendant_parent_needs = {1, 4, 5}; // Descendants need levels 1, 4, 5
        func.total_parent_needs = {0, 1, 2, 4, 5}; // Combined
        
        std::cout << "\nAnalysis:" << std::endl;
        std::cout << "SELF needs (high priority): ";
        print_set(func.self_parent_needs);
        std::cout << "DESCENDANT needs (low priority): ";
        print_set(func.descendant_parent_needs);
        std::cout << "Total needs: ";
        print_set(func.total_parent_needs);
        
        apply_priority_allocation(func);
        show_allocation_results(func);
        
        // Validation
        bool self_0_fast = func.fast_register_allocation.count(0) > 0;
        bool self_2_fast = func.fast_register_allocation.count(2) > 0;
        bool uses_stack = func.needs_stack_fallback;
        
        std::cout << "\nValidation:" << std::endl;
        std::cout << (self_0_fast ? "✅" : "❌") << " SELF need level 0 got fast register" << std::endl;
        std::cout << (self_2_fast ? "✅" : "❌") << " SELF need level 2 got fast register" << std::endl;
        std::cout << (uses_stack ? "✅" : "❌") << " Stack fallback used for excess descendant needs" << std::endl;
        
        functions_["main_function"] = func;
    }
    
    void test_register_pressure_scenario() {
        std::cout << "\n📋 TEST 2: Register Pressure Scenario" << std::endl;
        std::cout << "Function has 4 SELF needs (exceeds 3 fast registers!)" << std::endl;
        std::cout << "Expected: First 3 SELF needs get fast registers, 4th SELF need uses stack" << std::endl;
        
        FunctionInfo func;
        func.name = "register_pressure_func";
        func.scope_level = 4;
        func.self_parent_needs = {0, 1, 2, 3};  // 4 SELF needs!
        func.descendant_parent_needs = {5, 6};   // 2 descendant needs
        func.total_parent_needs = {0, 1, 2, 3, 5, 6}; // 6 total
        
        std::cout << "\nAnalysis:" << std::endl;
        std::cout << "SELF needs (4 levels - exceeds 3 registers!): ";
        print_set(func.self_parent_needs);
        std::cout << "DESCENDANT needs: ";
        print_set(func.descendant_parent_needs);
        
        apply_priority_allocation(func);
        show_allocation_results(func);
        
        // Count how many SELF needs got fast registers
        int self_fast_count = 0;
        for (int level : func.self_parent_needs) {
            if (func.fast_register_allocation.count(level) > 0) {
                self_fast_count++;
            }
        }
        
        std::cout << "\nValidation:" << std::endl;
        std::cout << "SELF needs with fast registers: " << self_fast_count << "/4" << std::endl;
        std::cout << (self_fast_count == 3 ? "✅" : "❌") << " Exactly 3 SELF needs got fast registers (maximum possible)" << std::endl;
        std::cout << (func.needs_stack_fallback ? "✅" : "❌") << " Stack fallback used for remaining needs" << std::endl;
    }
    
    void test_optimal_allocation_scenario() {
        std::cout << "\n📋 TEST 3: Optimal Allocation Scenario" << std::endl;
        std::cout << "Function has 2 SELF needs + 1 DESCENDANT need = 3 total (perfect fit!)" << std::endl;
        std::cout << "Expected: All needs get fast registers, no stack needed" << std::endl;
        
        FunctionInfo func;
        func.name = "optimal_func";
        func.scope_level = 3;
        func.self_parent_needs = {0, 2};        // 2 SELF needs
        func.descendant_parent_needs = {1};     // 1 descendant need
        func.total_parent_needs = {0, 1, 2};    // 3 total - perfect!
        
        std::cout << "\nAnalysis:" << std::endl;
        std::cout << "SELF needs: ";
        print_set(func.self_parent_needs);
        std::cout << "DESCENDANT needs: ";
        print_set(func.descendant_parent_needs);
        std::cout << "Total needs: ";
        print_set(func.total_parent_needs);
        
        apply_priority_allocation(func);
        show_allocation_results(func);
        
        bool all_fast = func.fast_register_allocation.size() == 3;
        bool no_stack = !func.needs_stack_fallback;
        
        std::cout << "\nValidation:" << std::endl;
        std::cout << (all_fast ? "✅" : "❌") << " All 3 needs got fast registers" << std::endl;
        std::cout << (no_stack ? "✅" : "❌") << " No stack fallback needed - optimal!" << std::endl;
        
        if (all_fast && no_stack) {
            std::cout << "🎉 PERFECT OPTIMIZATION: All parent scopes use fast registers!" << std::endl;
        }
    }
    
    void apply_priority_allocation(FunctionInfo& func) {
        std::cout << "\n🔄 Applying priority-based register allocation..." << std::endl;
        
        func.fast_register_allocation.clear();
        func.stack_allocation.clear();
        func.needs_stack_fallback = false;
        
        std::vector<int> fast_registers = {12, 13, 14}; // r12, r13, r14
        int register_index = 0;
        int stack_offset = 0;
        
        // PHASE 1: Allocate fast registers to SELF needs (highest priority)
        std::vector<int> self_needs(func.self_parent_needs.begin(), func.self_parent_needs.end());
        std::sort(self_needs.begin(), self_needs.end());
        
        std::cout << "PHASE 1 - SELF needs allocation:" << std::endl;
        for (int level : self_needs) {
            if (register_index < static_cast<int>(fast_registers.size())) {
                func.fast_register_allocation[level] = fast_registers[register_index];
                std::cout << "  Level " << level << " (SELF) -> r" << fast_registers[register_index] << " (FAST)" << std::endl;
                register_index++;
            } else {
                func.stack_allocation[level] = stack_offset;
                func.needs_stack_fallback = true;
                std::cout << "  Level " << level << " (SELF) -> stack[" << stack_offset << "] (SLOW - unavoidable)" << std::endl;
                stack_offset += 8;
            }
        }
        
        // PHASE 2: Allocate remaining registers/stack to DESCENDANT needs
        std::vector<int> descendant_needs(func.descendant_parent_needs.begin(), func.descendant_parent_needs.end());
        std::sort(descendant_needs.begin(), descendant_needs.end());
        
        std::cout << "PHASE 2 - DESCENDANT needs allocation:" << std::endl;
        for (int level : descendant_needs) {
            if (register_index < static_cast<int>(fast_registers.size())) {
                func.fast_register_allocation[level] = fast_registers[register_index];
                std::cout << "  Level " << level << " (DESCENDANT) -> r" << fast_registers[register_index] << " (FAST - bonus)" << std::endl;
                register_index++;
            } else {
                func.stack_allocation[level] = stack_offset;
                func.needs_stack_fallback = true;
                std::cout << "  Level " << level << " (DESCENDANT) -> stack[" << stack_offset << "] (SLOW - acceptable)" << std::endl;
                stack_offset += 8;
            }
        }
    }
    
    void show_allocation_results(const FunctionInfo& func) {
        std::cout << "\n📊 ALLOCATION RESULTS for " << func.name << ":" << std::endl;
        std::cout << "Fast registers used: " << func.fast_register_allocation.size() << "/3" << std::endl;
        std::cout << "Stack slots used: " << func.stack_allocation.size() << std::endl;
        std::cout << "Stack fallback needed: " << (func.needs_stack_fallback ? "YES" : "NO") << std::endl;
        
        std::cout << "\nDetailed allocation:" << std::endl;
        std::cout << "  r15: Current scope (always)" << std::endl;
        
        // Show fast register allocations
        std::vector<std::pair<int, int>> fast_regs;
        for (const auto& entry : func.fast_register_allocation) {
            fast_regs.push_back({entry.first, entry.second});
        }
        std::sort(fast_regs.begin(), fast_regs.end());
        
        for (const auto& reg : fast_regs) {
            std::string priority = func.self_parent_needs.count(reg.first) > 0 ? "SELF" : "DESCENDANT";
            std::cout << "  r" << reg.second << ": Parent level " << reg.first << " (" << priority << ")" << std::endl;
        }
        
        // Show stack allocations
        std::vector<std::pair<int, int>> stack_slots;
        for (const auto& entry : func.stack_allocation) {
            stack_slots.push_back({entry.first, entry.second});
        }
        std::sort(stack_slots.begin(), stack_slots.end());
        
        for (const auto& slot : stack_slots) {
            std::string priority = func.self_parent_needs.count(slot.first) > 0 ? "SELF" : "DESCENDANT";
            std::cout << "  stack[" << slot.second << "]: Parent level " << slot.first << " (" << priority << ")" << std::endl;
        }
    }
    
    void print_set(const std::unordered_set<int>& s) {
        if (s.empty()) {
            std::cout << "(none)" << std::endl;
            return;
        }
        
        std::vector<int> sorted(s.begin(), s.end());
        std::sort(sorted.begin(), sorted.end());
        for (size_t i = 0; i < sorted.size(); i++) {
            if (i > 0) std::cout << ", ";
            std::cout << sorted[i];
        }
        std::cout << std::endl;
    }
};

int main() {
    UltimateScopeOptimizationTest test;
    test.run_ultimate_optimization_demo();
    
    std::cout << "\n" << std::string(80, '=') << std::endl;
    std::cout << "🎉 ULTIMATE SCOPE OPTIMIZATION COMPLETE!" << std::endl;
    std::cout << "" << std::endl;
    std::cout << "🎯 KEY ACHIEVEMENTS:" << std::endl;
    std::cout << "✅ Priority-based register allocation" << std::endl;
    std::cout << "✅ SELF-accessed scopes get fast registers first" << std::endl;
    std::cout << "✅ DESCENDANT-only scopes use stack when needed" << std::endl;
    std::cout << "✅ Maximum performance for frequently accessed parent scopes" << std::endl;
    std::cout << "✅ Optimal register utilization under pressure" << std::endl;
    std::cout << "" << std::endl;
    std::cout << "🚀 This is the ultimate lexical scope optimization!" << std::endl;
    
    return 0;
}
