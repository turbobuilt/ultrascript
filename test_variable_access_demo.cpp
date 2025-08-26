#include <iostream>
#include <vector>
#include <string>

// Simulate the variable access pattern for test_scope.gts
int main() {
    std::cout << "=== VARIABLE ACCESS PATTERN DEMONSTRATION ===" << std::endl;
    std::cout << "\nFor test_scope.gts:" << std::endl;
    std::cout << "var x = 5;                    // Global scope level 0" << std::endl;
    std::cout << "let result = go function() {  // Goroutine function scope level 1" << std::endl;
    std::cout << "    var y = 0;                // Local to goroutine function" << std::endl;
    std::cout << "    console.log(y);           // Uses local variable y" << std::endl;
    std::cout << "    console.log('X is', x);   // Uses parent scope variable x" << std::endl;
    std::cout << "}" << std::endl;
    
    std::cout << "\n=== COMPILED ACCESS PATTERNS ===" << std::endl;
    
    std::cout << "\n--- Global scope (level 0) execution ---" << std::endl;
    std::cout << "x = 5                         // Direct assignment in current scope" << std::endl;
    std::cout << "result = <goroutine_ptr>      // Direct assignment in current scope" << std::endl;
    
    std::cout << "\n--- Goroutine function (level 1) execution ---" << std::endl;
    std::cout << "// Setup: r15 points to current scope, r12 points to parent scope" << std::endl;
    std::cout << "mov r12, [parent_scope_ptr]   // Load parent scope address into r12" << std::endl;
    std::cout << "mov r15, [current_scope_ptr]  // Load current scope address into r15" << std::endl;
    std::cout << "" << std::endl;
    
    std::cout << "// Variable assignments and access:" << std::endl;
    std::cout << "mov [r15+0], 0                // y = 0 (local variable, r15+offset)" << std::endl;
    std::cout << "mov rax, [r15+0]              // console.log(y) - load y from current scope" << std::endl;
    std::cout << "mov rbx, [r12+0]              // console.log(x) - load x from parent scope" << std::endl;
    
    std::cout << "\n=== REGISTER CONVENTION SUMMARY ===" << std::endl;
    std::cout << "✓ r15: ALWAYS holds current scope address" << std::endl;
    std::cout << "✓ r12: Holds parent scope level 0 address (when needed)" << std::endl;
    std::cout << "✓ r13: Holds parent scope level 1 address (when needed)" << std::endl;
    std::cout << "✓ r14: Holds parent scope level 2 address (when needed)" << std::endl;
    std::cout << "" << std::endl;
    std::cout << "Variable Access Patterns:" << std::endl;
    std::cout << "• Local variables: [r15 + offset]" << std::endl;
    std::cout << "• Parent level 0:  [r12 + offset]" << std::endl;
    std::cout << "• Parent level 1:  [r13 + offset]" << std::endl;
    std::cout << "• Parent level 2:  [r14 + offset]" << std::endl;
    
    std::cout << "\n=== EXAMPLE FOR test_scope.gts ===" << std::endl;
    std::cout << "In goroutine function:" << std::endl;
    std::cout << "• Variable 'y': [r15+0] (current scope)" << std::endl;
    std::cout << "• Variable 'x': [r12+0] (parent scope level 0)" << std::endl;
    
    std::cout << "\n🎯 OPTIMIZATION: Only allocate parent scope registers when actually needed!" << std::endl;
    return 0;
}
