#include "compiler.h"
#include "x86_codegen_v2.h"
#include "simple_lexical_scope.h"
#include "runtime.h"
#include "runtime_object.h"
#include "compilation_context.h"
#include "function_compilation_manager.h"
#include "console_log_overhaul.h"
#include "class_runtime_interface.h"
#include "dynamic_properties.h"
#include <iostream>
#include <unordered_map>
#include <unordered_set>
#include <cstring>
#include <cstdlib>
#include <stdexcept>
#include <string>

// Simple global constant storage for imported constants
static std::unordered_map<std::string, double> global_imported_constants;

// Forward declarations for function ID registry
void __register_function_id(int64_t function_id, const std::string& function_name);
void ensure_lookup_function_by_id_registered();

// Forward declaration for fast runtime function lookup
extern "C" void* __lookup_function_fast(uint16_t func_id);

// NEW SCOPE-AWARE CODE GENERATOR
// This extends X86CodeGenV2 with lexical scope management
class ScopeAwareCodeGen : public X86CodeGenV2 {
private:
    // Scope register management 
    // r15 = current scope (always)
    // r12, r13, r14 = parent scopes in order of access frequency
    struct ScopeRegisterState {
        int current_scope_depth = 0;
        std::unordered_map<int, int> scope_depth_to_register;  // scope_depth -> register_id (12,13,14)
        std::vector<int> available_scope_registers = {12, 13, 14};
        std::vector<int> stack_stored_scopes; // scopes that couldn't fit in registers
        
        // Register preservation tracking
        std::unordered_set<int> registers_in_use;  // which of r12,r13,r14 are currently used
        std::unordered_set<int> registers_saved_to_stack;  // which registers we've pushed to stack
        std::vector<int> register_save_order;  // order in which registers were saved (for proper restore)
    } scope_state;
    
    // Current context
    LexicalScopeNode* current_scope = nullptr;
    SimpleLexicalScopeAnalyzer* scope_analyzer = nullptr;
    
    // Type information from parse phase
    std::unordered_map<std::string, DataType> variable_types;
    std::unordered_map<std::string, DataType> variable_array_element_types;
    
public:
    ScopeAwareCodeGen(SimpleLexicalScopeAnalyzer* analyzer) : scope_analyzer(analyzer) {}
    
    // --- NEW FUNCTION INSTANCE CREATION ---
    // Emits code to create a function instance struct in memory
    void emit_function_instance_creation(FunctionDecl* child_func, size_t func_offset) {
        const FunctionStaticAnalysis& analysis = child_func->static_analysis;
        
        std::cout << "[NEW_CODEGEN] Creating function instance for '" << child_func->name 
                  << "' at offset " << func_offset << std::endl;
        std::cout << "[NEW_CODEGEN] Instance size: " << analysis.function_instance_size 
                  << ", captured scopes: " << analysis.parent_location_indexes.size() << std::endl;
        
        // Emit instance header: size field
        emit_mov_reg_imm(0, analysis.function_instance_size);
        emit_mov_reg_offset_reg(15, func_offset, 0); // [r15 + func_offset] = size

        // Emit function code address (will be patched by linker)
        emit_mov_reg_imm(0, 0x1234567890ABCDEF); // Placeholder for code address
        emit_mov_reg_offset_reg(15, func_offset + 8, 0); // [r15 + func_offset + 8] = code_addr

        // Emit number of captured scopes
        emit_mov_reg_imm(0, analysis.parent_location_indexes.size());
        emit_mov_reg_offset_reg(15, func_offset + 16, 0); // [r15 + func_offset + 16] = num_scopes

        // Copy captured scope addresses using parent_location_indexes mapping
        for (size_t child_idx = 0; child_idx < analysis.parent_location_indexes.size(); ++child_idx) {
            int parent_idx = analysis.parent_location_indexes[child_idx];
            size_t dest_offset = func_offset + 24 + (child_idx * 8);
            
            std::cout << "[NEW_CODEGEN] Mapping child_idx " << child_idx << " -> parent_idx " << parent_idx << std::endl;
            
            if (parent_idx == -1) {
                // Copy from parent's local scope (r15)
                emit_mov_reg_offset_reg(15, dest_offset, 15); // [r15 + dest_offset] = r15
                std::cout << "[NEW_CODEGEN] Copied parent local scope (r15) to child scopes[" << child_idx << "]" << std::endl;
            } else {
                // Copy from parent's register (r12 + parent_idx)
                int source_reg = 12 + parent_idx;
                emit_mov_reg_offset_reg(15, dest_offset, source_reg); // [r15 + dest_offset] = r12/r13/r14
                std::cout << "[NEW_CODEGEN] Copied parent r" << source_reg << " to child scopes[" << child_idx << "]" << std::endl;
            }
        }
    }

    // --- NEW FUNCTION CALL EMISSION ---
    // Emits code to call a function instance using the new stack-based calling convention
    void emit_function_instance_call(size_t func_offset, const std::vector<std::unique_ptr<ASTNode>>& arguments) {
        std::cout << "[NEW_CODEGEN] Emitting function instance call at offset " << func_offset << std::endl;
        
        // Load function instance address into rax
        emit_mov_reg_reg_offset(0, 15, func_offset); // rax = [r15 + func_offset] (instance pointer)
        
        // Get number of captured scopes
        emit_mov_reg_reg_offset(1, 0, 16); // rcx = [rax + 16] (num_captured_scopes)
        
        // Push captured scopes in reverse order (LIFO stack)
        // We need to push from (N-1) down to 0 so they're popped in order 0 to (N-1)
        
        // Generate loop to push scopes in reverse order
        // for (int i = num_scopes - 1; i >= 0; i--) push [rax + 24 + i*8]
        
        emit_mov_reg_reg(2, 1); // rdx = num_scopes
        emit_sub_reg_imm(2, 1);  // rdx = num_scopes - 1
        
        // Loop label (we'll implement with jumps)
        size_t loop_start = get_current_offset();
        
        // Check if rdx < 0 (signed comparison)
        emit_cmp_reg_imm(2, 0);
        size_t jump_end = reserve_jump_location(); // We'll patch this later
        
        // Calculate offset: 24 + rdx * 8
        emit_mov_reg_imm(3, 24); // r8 = 24
        emit_mov_reg_imm(4, 8);  // r9 = 8  
        emit_imul_reg_reg(4, 2); // r9 = rdx * 8
        emit_add_reg_reg(3, 4);  // r8 = 24 + (rdx * 8)
        
        // Push scope: push qword ptr [rax + r8]
        emit_push_reg_offset_reg(0, 3); // push [rax + r8]
        
        // Decrement counter and loop
        emit_sub_reg_imm(2, 1); // rdx--
        emit_jmp_to_offset(loop_start);
        
        // Patch the conditional jump to here
        patch_jump_to_current_location(jump_end);
        
        // Push parameters in reverse order
        for (int i = arguments.size() - 1; i >= 0; i--) {
            // Generate code for argument and push result
            arguments[i]->generate_code(*this);
            emit_push_reg(0); // push rax (result)
        }
        
        // Call the function: call [rax + 8] (function_code_addr)
        emit_call_reg_offset(0, 8);
        
        // Clean up stack: add rsp, (num_scopes + num_args) * 8
        size_t total_pushed = 8; // We'll calculate this properly
        // For now, assume we know the count and clean up
        // TODO: Compute exact cleanup size
        emit_add_reg_imm(4, total_pushed);
    }

    // --- NEW FUNCTION PROLOGUE GENERATION ---
    // Generates optimized function prologue using static analysis
    void emit_function_prologue(FunctionDecl* function) {
        const FunctionStaticAnalysis& analysis = function->static_analysis;
        
        std::cout << "[NEW_CODEGEN] Generating prologue for function '" << function->name << "'" << std::endl;
        std::cout << "[NEW_CODEGEN] Needs " << analysis.parent_location_indexes.size() << " parent scopes" << std::endl;
        
        // Standard prologue
        emit_push_reg(5); // push rbp
        emit_mov_reg_reg(5, 4); // mov rbp, rsp
        
        // Save only the registers we need (based on static analysis)
        if (analysis.needs_r12) {
            emit_push_reg(12); // push r12
            std::cout << "[NEW_CODEGEN] Saving r12 for parent scope access" << std::endl;
        }
        if (analysis.needs_r13) {
            emit_push_reg(13); // push r13
            std::cout << "[NEW_CODEGEN] Saving r13 for parent scope access" << std::endl;
        }
        if (analysis.needs_r14) {
            emit_push_reg(14); // push r14
            std::cout << "[NEW_CODEGEN] Saving r14 for parent scope access" << std::endl;
        }
        
        // Load parent scopes from stack parameters into registers
        size_t stack_offset = 16; // Skip return address (8) + rbp (8)
        
        for (size_t i = 0; i < analysis.parent_location_indexes.size() && i < 3; ++i) {
            int target_reg = 12 + i; // r12, r13, r14
            emit_mov_reg_reg_offset(target_reg, 5, stack_offset); // r12/13/14 = [rbp + offset]
            std::cout << "[NEW_CODEGEN] Loaded parent scope " << i << " into r" << target_reg 
                      << " from [rbp+" << stack_offset << "]" << std::endl;
            stack_offset += 8;
        }
        
        // Allocate local scope using direct syscall (mmap)
        if (analysis.local_scope_size > 0) {
            std::cout << "[NEW_CODEGEN] Allocating " << analysis.local_scope_size << " bytes for local scope" << std::endl;
            
            // Use mmap syscall for scope allocation
            emit_mov_reg_imm(0, 9);     // sys_mmap
            emit_mov_reg_imm(7, 0);     // addr = NULL  
            emit_mov_reg_imm(6, analysis.local_scope_size); // length
            emit_mov_reg_imm(2, 3);     // prot = PROT_READ | PROT_WRITE
            emit_mov_reg_imm(10, 34);   // flags = MAP_PRIVATE | MAP_ANONYMOUS
            emit_mov_reg_imm(8, -1);    // fd = -1
            emit_mov_reg_imm(9, 0);     // offset = 0
            emit_syscall();
            
            emit_mov_reg_reg(15, 0);    // r15 = mmap result (local scope address)
            std::cout << "[NEW_CODEGEN] Local scope allocated at address in r15" << std::endl;
        }
    }

    // --- NEW FUNCTION EPILOGUE GENERATION ---
    void emit_function_epilogue(FunctionDecl* function) {
        const FunctionStaticAnalysis& analysis = function->static_analysis;
        
        std::cout << "[NEW_CODEGEN] Generating epilogue for function '" << function->name << "'" << std::endl;
        
        // Free local scope using munmap syscall
        if (analysis.local_scope_size > 0) {
            emit_mov_reg_imm(0, 11);    // sys_munmap
            emit_mov_reg_reg(7, 15);    // addr = r15 (local scope)
            emit_mov_reg_imm(6, analysis.local_scope_size); // length
            emit_syscall();
            std::cout << "[NEW_CODEGEN] Freed local scope" << std::endl;
        }
        
        // Restore saved registers in reverse order
        if (analysis.needs_r14) {
            emit_pop_reg(14); // pop r14
        }
        if (analysis.needs_r13) {
            emit_pop_reg(13); // pop r13
        }
        if (analysis.needs_r12) {
            emit_pop_reg(12); // pop r12
        }
        
        // Standard epilogue
        emit_mov_reg_reg(4, 5); // mov rsp, rbp
        emit_pop_reg(5);        // pop rbp
        emit_ret();             // ret
    }
    
    // Set the current scope context
    void set_current_scope(LexicalScopeNode* scope) {
        current_scope = scope;
        if (scope) {
            scope_state.current_scope_depth = scope->scope_depth;
        }
    }
    
    // Scope management methods
    void enter_lexical_scope(LexicalScopeNode* scope_node) {
        std::cout << "[NEW_CODEGEN] Entering lexical scope at depth " << scope_node->scope_depth << std::endl;
        
        // Allocate stack space for this scope's variables
        size_t scope_size = scope_node->total_scope_frame_size;
        if (scope_size > 0) {
            emit_sub_reg_imm(4, scope_size); // sub rsp, scope_size
            emit_mov_reg_reg(15, 4); // mov r15, rsp (r15 points to current scope)
            std::cout << "[NEW_CODEGEN] Allocated " << scope_size << " bytes for scope, r15 = rsp" << std::endl;
        }
        
        // r15 is always used for current scope
        mark_register_in_use(15);
        
        // Set up parent scope registers based on priority
        setup_parent_scope_registers(scope_node);
        
        // Update current context
        set_current_scope(scope_node);
    }
    
    void exit_lexical_scope(LexicalScopeNode* scope_node) {
        std::cout << "[NEW_CODEGEN] Exiting lexical scope at depth " << scope_node->scope_depth << std::endl;
        
        // Restore parent scope registers first (before deallocating stack)
        restore_parent_scope_registers();
        
        // Deallocate stack space
        size_t scope_size = scope_node->total_scope_frame_size;
        if (scope_size > 0) {
            emit_add_reg_imm(4, scope_size); // add rsp, scope_size
            std::cout << "[NEW_CODEGEN] Deallocated " << scope_size << " bytes from scope" << std::endl;
        }
        
        // r15 is no longer used for this scope (will be set by parent scope)
        mark_register_free(15);
    }
    
    // Variable access methods
    void emit_variable_load(const std::string& var_name) {
        if (!current_scope || !scope_analyzer) {
            throw std::runtime_error("No scope context for variable access: " + var_name);
        }
        
        // Find the scope where this variable is defined
        auto def_scope_weak = scope_analyzer->get_definition_scope_for_variable(var_name);
        auto def_scope = def_scope_weak.lock();
        if (!def_scope) {
            throw std::runtime_error("Variable not found in any scope: " + var_name);
        }
        
        // Get variable offset within its scope
        auto offset_it = def_scope->variable_offsets.find(var_name);
        if (offset_it == def_scope->variable_offsets.end()) {
            throw std::runtime_error("Variable offset not found: " + var_name);
        }
        size_t var_offset = offset_it->second;
        
        if (def_scope == current_scope) {
            // Variable is in current scope - use r15 + offset
            emit_mov_reg_reg_offset(0, 15, var_offset); // rax = [r15 + offset]
            std::cout << "[NEW_CODEGEN] Loaded local variable '" << var_name << "' from r15+" << var_offset << std::endl;
        } else {
            // Variable is in parent scope - find which register holds it
            int scope_depth = def_scope->scope_depth;
            auto reg_it = scope_state.scope_depth_to_register.find(scope_depth);
            
            if (reg_it != scope_state.scope_depth_to_register.end()) {
                // Parent scope is in a register
                int scope_reg = reg_it->second;
                emit_mov_reg_reg_offset(0, scope_reg, var_offset); // rax = [r12/13/14 + offset]
                std::cout << "[NEW_CODEGEN] Loaded parent variable '" << var_name 
                          << "' from r" << scope_reg << "+" << var_offset << std::endl;
            } else {
                // Parent scope is on stack (deep nesting)
                // TODO: Implement stack-based parent scope access
                throw std::runtime_error("Deep nested scope access not yet implemented for: " + var_name);
            }
        }
    }
    
    void emit_variable_store(const std::string& var_name) {
        if (!current_scope || !scope_analyzer) {
            throw std::runtime_error("No scope context for variable store: " + var_name);
        }
        
        // For assignments, the variable should be in current scope
        auto offset_it = current_scope->variable_offsets.find(var_name);
        if (offset_it != current_scope->variable_offsets.end()) {
            // Variable is in current scope
            size_t var_offset = offset_it->second;
            emit_mov_reg_offset_reg(15, var_offset, 0); // [r15 + offset] = rax
            std::cout << "[NEW_CODEGEN] Stored to local variable '" << var_name << "' at r15+" << var_offset << std::endl;
        } else {
            // This might be a reassignment to a parent scope variable
            auto def_scope_weak = scope_analyzer->get_definition_scope_for_variable(var_name);
            auto def_scope = def_scope_weak.lock();
            if (!def_scope) {
                // New variable - add to current scope
                // This would need integration with the scope analyzer
                throw std::runtime_error("Variable assignment to undefined variable: " + var_name);
            } else {
                // Reassignment to parent scope variable
                auto parent_offset_it = def_scope->variable_offsets.find(var_name);
                if (parent_offset_it == def_scope->variable_offsets.end()) {
                    throw std::runtime_error("Parent variable offset not found: " + var_name);
                }
                
                size_t var_offset = parent_offset_it->second;
                int scope_depth = def_scope->scope_depth;
                auto reg_it = scope_state.scope_depth_to_register.find(scope_depth);
                
                if (reg_it != scope_state.scope_depth_to_register.end()) {
                    int scope_reg = reg_it->second;
                    emit_mov_reg_offset_reg(scope_reg, var_offset, 0); // [r12/13/14 + offset] = rax
                    std::cout << "[NEW_CODEGEN] Stored to parent variable '" << var_name 
                              << "' at r" << scope_reg << "+" << var_offset << std::endl;
                } else {
                    throw std::runtime_error("Deep nested scope store not yet implemented for: " + var_name);
                }
            }
        }
    }
    
    // Type context methods (similar to old TypeInference)
    void set_variable_type(const std::string& name, DataType type) {
        variable_types[name] = type;
    }
    
    DataType get_variable_type(const std::string& name) {
        auto it = variable_types.find(name);
        return (it != variable_types.end()) ? it->second : DataType::ANY;
    }
    
    // Register usage tracking methods
    void mark_register_in_use(int reg_id) {
        scope_state.registers_in_use.insert(reg_id);
        std::cout << "[NEW_CODEGEN] Marked r" << reg_id << " as in use" << std::endl;
    }
    
    void mark_register_free(int reg_id) {
        scope_state.registers_in_use.erase(reg_id);
        std::cout << "[NEW_CODEGEN] Marked r" << reg_id << " as free" << std::endl;
    }
    
    bool is_register_in_use(int reg_id) {
        return scope_state.registers_in_use.count(reg_id) > 0;
    }
    
private:
    void setup_parent_scope_registers(LexicalScopeNode* scope_node) {
        std::cout << "[NEW_CODEGEN] Setting up parent scope registers for scope depth " << scope_node->scope_depth << std::endl;
        
        // Determine which registers we need for parent scopes
        std::vector<int> needed_registers;
        size_t num_parent_scopes = scope_node->priority_sorted_parent_scopes.size();
        size_t max_registers = std::min(num_parent_scopes, size_t(3)); // r12, r13, r14
        
        for (size_t i = 0; i < max_registers; ++i) {
            needed_registers.push_back(scope_state.available_scope_registers[i]);
        }
        
        std::cout << "[NEW_CODEGEN] Need " << needed_registers.size() << " registers for parent scopes" << std::endl;
        
        // Save any currently used registers that we need to overwrite
        scope_state.register_save_order.clear();
        scope_state.registers_saved_to_stack.clear();
        
        for (int reg : needed_registers) {
            if (scope_state.registers_in_use.count(reg)) {
                // This register is currently in use, save it to stack
                emit_push_reg(reg);  // push r12/r13/r14
                scope_state.registers_saved_to_stack.insert(reg);
                scope_state.register_save_order.push_back(reg);
                std::cout << "[NEW_CODEGEN] Saved r" << reg << " to stack (was in use)" << std::endl;
            }
        }
        
        // Now set up parent scope registers
        scope_state.scope_depth_to_register.clear();
        
        for (size_t i = 0; i < max_registers; ++i) {
            int parent_depth = scope_node->priority_sorted_parent_scopes[i];
            int scope_reg = needed_registers[i];
            
            // Load parent scope pointer into the register
            // TODO: Implement actual parent scope pointer loading
            // For now, assume parent scopes are stored on stack
            int64_t parent_scope_offset = -32 - (parent_depth * 8);  // Placeholder calculation
            emit_mov_reg_mem(scope_reg, parent_scope_offset);  // r12/13/14 = [rbp + parent_offset]
            
            scope_state.scope_depth_to_register[parent_depth] = scope_reg;
            scope_state.registers_in_use.insert(scope_reg); // Mark as in use
            
            std::cout << "[NEW_CODEGEN] Assigned parent scope depth " << parent_depth 
                      << " to register r" << scope_reg << std::endl;
        }
        
        // Store remaining deep scopes on stack
        scope_state.stack_stored_scopes.clear();
        for (size_t i = max_registers; i < num_parent_scopes; ++i) {
            int deep_scope_depth = scope_node->priority_sorted_parent_scopes[i];
            scope_state.stack_stored_scopes.push_back(deep_scope_depth);
            std::cout << "[NEW_CODEGEN] Deep parent scope depth " << deep_scope_depth 
                      << " will use stack access" << std::endl;
        }
    }
    
    void restore_parent_scope_registers() {
        std::cout << "[NEW_CODEGEN] Restoring parent scope registers" << std::endl;
        
        // Clear current register usage for parent scopes
        for (auto& pair : scope_state.scope_depth_to_register) {
            int reg = pair.second;
            scope_state.registers_in_use.erase(reg);
            std::cout << "[NEW_CODEGEN] Freed register r" << reg << " from parent scope use" << std::endl;
        }
        
        // Restore previously saved registers in reverse order (stack is LIFO)
        for (auto it = scope_state.register_save_order.rbegin(); 
             it != scope_state.register_save_order.rend(); ++it) {
            int reg = *it;
            emit_pop_reg(reg);  // pop r12/r13/r14
            scope_state.registers_in_use.insert(reg); // Mark as in use again
            std::cout << "[NEW_CODEGEN] Restored r" << reg << " from stack" << std::endl;
        }
        
        // Clear tracking state
        scope_state.register_save_order.clear();
        scope_state.registers_saved_to_stack.clear();
        scope_state.scope_depth_to_register.clear();
        scope_state.stack_stored_scopes.clear();
    }
};

// Global codegen instance - this will replace the TypeInference usage
thread_local ScopeAwareCodeGen* g_scope_codegen = nullptr;

// Helper function to get current scope-aware codegen
ScopeAwareCodeGen* get_current_scope_codegen() {
    return g_scope_codegen;
}

// Helper function to set current scope-aware codegen
void set_current_scope_codegen(ScopeAwareCodeGen* codegen) {
    g_scope_codegen = codegen;
}

// Factory function to create new scope-aware code generator
std::unique_ptr<CodeGenerator> create_scope_aware_codegen(SimpleLexicalScopeAnalyzer* analyzer) {
    return std::make_unique<ScopeAwareCodeGen>(analyzer);
}

//=============================================================================
// NEW AST NODE IMPLEMENTATIONS WITHOUT TYPEINFERENCE
//=============================================================================

// NumberLiteral - simplified without TypeInference complexity
void NumberLiteral::generate_code_new(CodeGenerator& gen) {
    std::cout << "[NEW_CODEGEN] NumberLiteral::generate_code - value=" << value << std::endl;
    
    // Get type context from scope-aware codegen if available
    ScopeAwareCodeGen* scope_gen = get_current_scope_codegen();
    if (scope_gen) {
        // TODO: Get assignment context from scope generator
        // For now, just use FLOAT64 for JavaScript compatibility
        gen.emit_mov_reg_imm(0, *reinterpret_cast<const int64_t*>(&value));
        result_type = DataType::FLOAT64;
    } else {
        // Fallback: direct code generation
        union { double d; int64_t i; } converter;
        converter.d = value;
        gen.emit_mov_reg_imm(0, converter.i);
        result_type = DataType::FLOAT64;
    }
}

// Identifier - the most important change
void Identifier::generate_code_new(CodeGenerator& gen) {
    std::cout << "[NEW_CODEGEN] Identifier::generate_code - variable: " << name << std::endl;
    
    // Handle special cases first
    if (name == "true") {
        gen.emit_mov_reg_imm(0, 1);
        result_type = DataType::BOOLEAN;
        return;
    }
    if (name == "false") {
        gen.emit_mov_reg_imm(0, 0);
        result_type = DataType::BOOLEAN;
        return;
    }
    if (name == "runtime") {
        result_type = DataType::RUNTIME_OBJECT;
        return;
    }
    
    // Check global constants
    auto it = global_imported_constants.find(name);
    if (it != global_imported_constants.end()) {
        union { double f; int64_t i; } converter;
        converter.f = it->second;
        gen.emit_mov_reg_imm(0, converter.i);
        result_type = DataType::FLOAT64;
        return;
    }
    
    // Use scope-aware codegen for variable access
    ScopeAwareCodeGen* scope_gen = get_current_scope_codegen();
    if (scope_gen) {
        try {
            scope_gen->emit_variable_load(name);
            result_type = scope_gen->get_variable_type(name);
            std::cout << "[NEW_CODEGEN] Variable '" << name << "' loaded successfully" << std::endl;
            return;
        } catch (const std::exception& e) {
            std::cout << "[NEW_CODEGEN] Variable load failed: " << e.what() << std::endl;
        }
    }
    
    // If we reach here, try implicit 'this.property' access
    // TODO: Implement class property access
    
    throw std::runtime_error("Undefined variable: " + name);
}

// Assignment - completely rewritten
void Assignment::generate_code_new(CodeGenerator& gen) {
    std::cout << "[NEW_CODEGEN] Assignment::generate_code - variable: " << variable_name 
              << ", declared_type=" << static_cast<int>(declared_type) << std::endl;
    
    ScopeAwareCodeGen* scope_gen = get_current_scope_codegen();
    if (!scope_gen) {
        throw std::runtime_error("No scope context for assignment: " + variable_name);
    }
    
    // Generate value first
    if (value) {
        // TODO: Once all nodes have generate_code_new, use that instead
        // For now, create a dummy TypeInference to satisfy the interface
        // This is a temporary hack until we convert all nodes
        struct DummyTypeInference {
            // Minimal implementation to satisfy assignment needs
            void set_variable_type(const std::string& name, DataType type) {}
            DataType get_variable_type(const std::string& name) { return DataType::ANY; }
            void mark_variable_used(const std::string& name) {}
            bool variable_exists(const std::string& name) { return false; }
        } dummy_types;
        
        // This is a temporary solution - we'll need to convert all AST nodes
        throw std::runtime_error("Mixed old/new interface not yet supported");
        
        // Determine variable type
        DataType variable_type;
        if (declared_type != DataType::ANY) {
            variable_type = declared_type;
        } else {
            variable_type = value->result_type;
        }
        
        // Store type information
        scope_gen->set_variable_type(variable_name, variable_type);
        
        // Store the value using scope-aware codegen
        scope_gen->emit_variable_store(variable_name);
        
        std::cout << "[NEW_CODEGEN] Assignment to '" << variable_name << "' completed" << std::endl;
    }
}

// TODO: Add more AST node implementations...
