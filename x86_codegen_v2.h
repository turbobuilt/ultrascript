#pragma once

#include "x86_instruction_builder.h"
#include "codegen_forward.h"
#include <memory>
#include <unordered_set>
#include <unordered_map>
#include <vector>

// Forward declarations for scope management
class LexicalScopeNode;
class SimpleLexicalScopeAnalyzer;
class StaticAnalyzer;
struct FunctionDecl;
struct VariableDeclarationInfo;
struct ASTNode;
enum class DataType;



// New high-performance X86 code generator using instruction builder abstraction
class X86CodeGenV2 : public CodeGenerator {
private:
    std::vector<uint8_t> code_buffer;
    std::unique_ptr<X86InstructionBuilder> instruction_builder;
    std::unique_ptr<X86PatternBuilder> pattern_builder;
    
    // Register allocation state
    struct RegisterState {
        bool is_free[16];
        X86Reg last_allocated;
        
        RegisterState() : last_allocated(X86Reg::RAX) {
            std::fill(is_free, is_free + 16, true);
            // Mark stack pointer and base pointer as not free
            is_free[static_cast<int>(X86Reg::RSP)] = false;
            is_free[static_cast<int>(X86Reg::RBP)] = false;
        }
    } reg_state;
    
    // Stack frame management
    struct StackFrame {
        size_t local_stack_size = 0;
        std::vector<X86Reg> saved_registers;
        size_t current_offset = 0;
        bool frame_established = false;
    } stack_frame;
    
    // Label management
    std::unordered_map<std::string, int64_t> label_offsets;
    std::vector<std::pair<std::string, size_t>> unresolved_jumps;
    
    // Function instance patching system
    struct FunctionInstancePatchInfo {
        void* instance_ptr;           // Pointer to the function instance
        std::string function_name;    // Name of the function to resolve
        size_t code_addr_offset;      // Offset within instance where to store the address
        
        FunctionInstancePatchInfo(void* ptr, const std::string& name, size_t offset)
            : instance_ptr(ptr), function_name(name), code_addr_offset(offset) {}
    };
    std::vector<FunctionInstancePatchInfo> function_instances_to_patch;
    
    // Scope management - merged from ScopeAwareCodeGen
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
    class LexicalScopeNode* current_scope = nullptr;
    class SimpleLexicalScopeAnalyzer* scope_analyzer = nullptr;
    class StaticAnalyzer* static_analyzer_ = nullptr;
    
    // Type information from parse phase
    std::unordered_map<std::string, DataType> variable_types;
    std::unordered_map<std::string, DataType> variable_array_element_types;
    
    // Helper methods for register management
    X86Reg allocate_register();
    void free_register(X86Reg reg);
    X86Reg get_register_for_int(int reg_id);
    
    // Runtime function resolution helper
    void* get_runtime_function_address(const std::string& function_name);
    
    // Scope management private methods
    void setup_parent_scope_registers(LexicalScopeNode* scope_node);
    void restore_parent_scope_registers();
    
    // Optimization helpers
    void optimize_mov_sequences();
    void eliminate_dead_code();
    
public:
    X86CodeGenV2();
    X86CodeGenV2(SimpleLexicalScopeAnalyzer* analyzer);
    X86CodeGenV2(StaticAnalyzer* analyzer);
    ~X86CodeGenV2() override = default;
    
    // Performance optimization settings
    bool enable_peephole_optimization = true;
    bool enable_register_allocation = true;
    
    // --- SCOPE MANAGEMENT METHODS (merged from ScopeAwareCodeGen) ---
    
    // Function instance creation/call methods (stubs for now)
    void emit_function_instance_creation(struct FunctionDecl* child_func, size_t func_offset);
    void emit_function_instance_call(size_t func_offset, const std::vector<std::unique_ptr<ASTNode>>& arguments);
    
    // Function prologue/epilogue generation with scope management
    void emit_function_prologue(struct FunctionDecl* function);
    void emit_function_epilogue(struct FunctionDecl* function);
    
    // Set the current scope context
    void set_current_scope(LexicalScopeNode* scope);
    
    // Scope data access methods
    LexicalScopeNode* get_scope_node_for_depth(int depth);
    LexicalScopeNode* get_definition_scope_for_variable(const std::string& name);
    void perform_deferred_packing_for_scope(LexicalScopeNode* scope_node);
    
    // Scope management methods
    void enter_lexical_scope(LexicalScopeNode* scope_node);
    void exit_lexical_scope(LexicalScopeNode* scope_node);
    
    // Variable access methods
    void emit_variable_load(const std::string& var_name);
    void emit_variable_store(const std::string& var_name);
    VariableDeclarationInfo* get_variable_declaration_info(const std::string& name);
    
    // Type context methods
    void set_variable_type(const std::string& name, DataType type);
    DataType get_variable_type(const std::string& name);
    
    // Register usage tracking methods
    void mark_register_in_use(int reg_id);
    void mark_register_free(int reg_id);
    bool is_register_in_use(int reg_id);
    
    // --- END SCOPE MANAGEMENT ---
    
    // CodeGenerator interface implementation
    void emit_prologue() override;
    void emit_epilogue() override;
    void emit_mov_reg_imm(int reg, int64_t value) override;
    
    // ROBUST PATCHING API - Enhanced MOV with exact patch information
    struct MovPatchInfo {
        size_t immediate_offset;    // Exact byte offset where immediate field is located
        size_t instruction_length;  // Total length of the instruction
        size_t immediate_size;      // Size of immediate field (4 or 8 bytes)
    };
    MovPatchInfo emit_mov_reg_imm_with_patch_info(int reg, int64_t value);
    
    // HIGH-LEVEL ROBUST PATCHING API FOR FUNCTION CALLS
    void emit_patchable_function_call(const std::string& function_name, void* function_ast_node);
    void emit_mov_reg_reg(int dst, int src) override;
    void emit_mov_mem_reg(int64_t offset, int reg) override;
    void emit_mov_reg_mem(int reg, int64_t offset) override;
    
    // Register-relative memory operations for direct object property access
    void emit_mov_reg_reg_offset(int dst_reg, int src_reg, int64_t offset) override;  // dst = [src+offset]
    void emit_mov_reg_offset_reg(int dst_reg, int64_t offset, int src_reg) override;  // [dst+offset] = src
    
    // RSP-relative memory operations for stack manipulation
    void emit_mov_mem_rsp_reg(int64_t offset, int reg) override;
    void emit_mov_reg_mem_rsp(int reg, int64_t offset) override;
    void emit_add_reg_imm(int reg, int64_t value) override;
    void emit_add_reg_reg(int dst, int src) override;
    void emit_sub_reg_imm(int reg, int64_t value) override;
    void emit_sub_reg_reg(int dst, int src) override;
    void emit_mul_reg_reg(int dst, int src) override;
    void emit_div_reg_reg(int dst, int src) override;
    void emit_mod_reg_reg(int dst, int src) override;
    void emit_call(const std::string& label) override;
    void emit_ret() override;
    void emit_function_return() override;
    void emit_jump(const std::string& label) override;
    void emit_jump_if_zero(const std::string& label) override;
    void emit_jump_if_not_zero(const std::string& label) override;
    void emit_jump_if_greater_equal(const std::string& label);
    void emit_compare(int reg1, int reg2) override;
    void emit_setl(int reg) override;
    void emit_setg(int reg) override;
    void emit_sete(int reg) override;
    void emit_setne(int reg) override;
    void emit_setle(int reg) override;
    void emit_setge(int reg) override;
    void emit_and_reg_imm(int reg, int64_t value) override;
    void emit_xor_reg_reg(int dst, int src) override;
    void emit_call_reg(int reg) override;
    void emit_label(const std::string& label) override;
    void emit_goroutine_spawn(const std::string& function_name) override;
    void emit_goroutine_spawn_with_args(const std::string& function_name, int arg_count) override;
    void emit_goroutine_spawn_with_func_ptr() override;
    void emit_goroutine_spawn_with_func_id() override;
    void emit_goroutine_spawn_with_address(void* function_address) override;
    void emit_promise_resolve(int value_reg) override;
    void emit_promise_await(int promise_reg) override;
    
    // High-Performance Function Calls
    void emit_call_fast(uint16_t func_id) override;
    void emit_goroutine_spawn_fast(uint16_t func_id) override;
    void emit_goroutine_spawn_direct(void* function_address) override;
    void emit_goroutine_spawn_and_wait_direct(void* function_address);
    void emit_goroutine_spawn_and_wait_fast(uint16_t func_id);
    
    // Lock operations
    void emit_lock_acquire(int lock_reg) override;
    void emit_lock_release(int lock_reg) override;
    void emit_lock_try_acquire(int lock_reg, int result_reg) override;
    void emit_lock_try_acquire_timeout(int lock_reg, int timeout_reg, int result_reg) override;
    
    // Atomic operations
    void emit_atomic_compare_exchange(int ptr_reg, int expected_reg, int desired_reg, int result_reg) override;
    void emit_atomic_fetch_add(int ptr_reg, int value_reg, int result_reg) override;
    void emit_atomic_store(int ptr_reg, int value_reg, int memory_order) override;
    void emit_atomic_load(int ptr_reg, int result_reg, int memory_order) override;
    void emit_memory_fence(int fence_type) override;
    
    // NEW METHODS FOR FUNCTION CALLING OVERHAUL
    void emit_push_reg_offset_reg(int base_reg, int offset_reg); // push [base + offset]
    void emit_call_reg_offset(int reg, int64_t offset);          // call [reg + offset]
    void emit_cmp_reg_imm(int reg, int64_t value);               // cmp reg, imm
    void emit_imul_reg_reg(int dst, int src);                    // imul dst, src
    void emit_jmp_to_offset(size_t target_offset);               // jmp to absolute offset
    size_t reserve_jump_location();                              // Reserve space for conditional jump
    void patch_jump_to_current_location(size_t jump_location);   // Patch reserved jump
    void emit_syscall();                                         // syscall instruction
    void emit_push_reg(int reg);                                 // push reg  
    void emit_pop_reg(int reg);                                  // pop reg
    
    // High-performance reference counting operations
    void emit_ref_count_increment(int object_reg) override;
    void emit_ref_count_decrement(int object_reg, int result_reg) override;
    
    // Additional ultra-fast reference counting operations for specific use cases
    void emit_ref_count_increment_simple(int object_reg);
    void emit_ref_count_decrement_simple(int object_reg);
    void emit_ref_count_check_zero_and_free(int object_reg, const std::string& free_label);
    
    // CodeGenerator interface getters
    std::vector<uint8_t> get_code() const override { return code_buffer; }
    void clear() override;
    
    // Validation for robust code generation
    bool validate_code_generation() const;  // Validate all labels resolved and code is ready
    
    // High-Performance Floating-Point Operations
    // These provide direct XMM register access for maximum performance
    void emit_movq_xmm_gpr(int xmm_reg, int gpr_reg);  // Move 64-bit from GPR to XMM
    void emit_movq_gpr_xmm(int gpr_reg, int xmm_reg);  // Move 64-bit from XMM to GPR  
    void emit_movsd_xmm_xmm(int dst_xmm, int src_xmm);  // Move scalar double between XMM
    void emit_cvtsi2sd(int xmm_reg, int gpr_reg);  // Convert signed integer to double
    void emit_cvtsd2si(int gpr_reg, int xmm_reg);  // Convert double to signed integer
    
    // High-performance floating-point function calls with proper calling convention
    void emit_call_with_double_arg(const std::string& function_name, int value_gpr_reg);
    void emit_call_with_xmm_arg(const std::string& function_name, int xmm_reg);
    size_t get_current_offset() const override { return code_buffer.size(); }
    // size_t get_last_instruction_length() const { return instruction_builder->get_last_instruction_length(); }
    const std::unordered_map<std::string, int64_t>& get_label_offsets() const override;
    
    // New high-level APIs for better code generation
    void emit_function_call(const std::string& function_name, const std::vector<int>& args);
    void emit_typed_array_access(int array_reg, int index_reg, int result_reg, OpSize element_size);
    void emit_string_operation(const std::string& operation, int str1_reg, int str2_reg, int result_reg);
    void emit_bounds_check(int index_reg, int limit_reg);
    void emit_null_check(int pointer_reg);
    
    // Performance monitoring and debugging
    void enable_optimization(bool enable) { enable_peephole_optimization = enable; }
    void enable_register_optimization(bool enable) { enable_register_allocation = enable; }
    size_t get_instruction_count() const;
    void print_assembly_debug() const;
    
    // Advanced code generation patterns
    void emit_loop_optimized(int counter_reg, const std::string& body_label);
    void emit_conditional_move(int condition_reg, int true_val_reg, int false_val_reg, int dest_reg);
    void emit_switch_table(int selector_reg, const std::vector<std::string>& case_labels);
    
    // Memory management helpers
    void set_stack_frame_size(size_t size) { stack_frame.local_stack_size = size; }
    
    // INLINE HEAP ALLOCATION FOR LEXICAL SCOPES (ultra-fast malloc alternative)
    void emit_inline_heap_alloc(size_t size, int result_reg);  // Allocate heap memory inline, result in result_reg
    
    // HIGH-PERFORMANCE LEXICAL SCOPE REGISTER MANAGEMENT
    void emit_scope_register_setup(int scope_level);
    void emit_scope_register_save(int reg_id);
    void emit_scope_register_restore(int reg_id);
    void emit_scope_pointer_load(int reg_id, int scope_level);
    void emit_variable_load_from_scope_register(int dst_reg, int scope_reg, int64_t offset);
    
    // Stack management for function frames (required by base interface)
    void set_function_stack_size(int64_t size) override { stack_frame.local_stack_size = size; }
    int64_t get_function_stack_size() const override { return stack_frame.local_stack_size; }
    
    // Stack frame isolation for multiple function compilation
    void reset_stack_frame_for_new_function();
    
    // Runtime function call resolution (required by base interface)
    void resolve_runtime_function_calls() override;
    void add_saved_register(X86Reg reg) { stack_frame.saved_registers.push_back(reg); }
    
    // Direct access to builders for advanced usage
    X86InstructionBuilder& get_instruction_builder() { return *instruction_builder; }
    X86PatternBuilder& get_pattern_builder() { return *pattern_builder; }
    
    // Function instance patching system for high-performance function calls
    void register_function_instance_for_patching(void* instance_ptr, const std::string& function_name, size_t code_addr_offset);
    void patch_all_function_instances(void* executable_memory_base);
};

// Factory function for creating optimized code generators
std::unique_ptr<CodeGenerator> create_optimized_x86_codegen();

// Factory functions to replace create_scope_aware_codegen
std::unique_ptr<CodeGenerator> create_scope_aware_codegen(SimpleLexicalScopeAnalyzer* analyzer);
std::unique_ptr<CodeGenerator> create_scope_aware_codegen_with_static_analyzer(StaticAnalyzer* analyzer);

// New factory function names for clarity
std::unique_ptr<X86CodeGenV2> create_x86_codegen_with_scope_analyzer(SimpleLexicalScopeAnalyzer* analyzer);
std::unique_ptr<X86CodeGenV2> create_x86_codegen_with_static_analyzer(StaticAnalyzer* analyzer);

// Performance testing and validation
class X86CodeGenTester {
public:
    static bool validate_instruction_encoding(const std::vector<uint8_t>& code);
    static void benchmark_code_generation_speed();
};


