#include "function_instance_system.h"
#include "x86_codegen_v2.h"
#include "compiler.h"
#include <iostream>
#include <algorithm>
#include <cstring>

// Global function instance system
FunctionInstanceSystem g_function_system;

// Phase 1: Complete static analysis computation
void FunctionInstanceSystem::compute_complete_function_analysis(
    FunctionDecl* function, 
    const std::unordered_map<int, LexicalScopeNode*>& all_scopes) {
    
    std::cout << "[FUNCTION_SYSTEM] Computing complete analysis for function: " << function->name << std::endl;
    
    CompleteFunctionAnalysis analysis;
    
    if (!function->lexical_scope) {
        std::cout << "[FUNCTION_SYSTEM] No lexical scope for function " << function->name << std::endl;
        function_analysis_cache_[function->name] = analysis;
        return;
    }
    
    LexicalScopeNode* function_scope = function->lexical_scope.get();
    
    // Step 1: Compute scope dependencies based on variable accesses
    compute_scope_dependencies(function, function_scope, all_scopes, analysis);
    
    // Step 2: Compute register allocation for most frequent scopes
    compute_register_allocation(analysis);
    
    // Step 3: Compute function instance sizing
    analysis.function_instance_size = compute_function_instance_size(analysis.priority_sorted_parent_scopes);
    analysis.local_scope_size = function_scope->total_scope_frame_size;
    
    // Step 4: Compute hidden parameter specification for function calls
    compute_hidden_parameter_specification(analysis, all_scopes);
    
    // Step 5: Compute descendant dependencies (for Conservative Maximum Size)
    compute_descendant_dependencies(function, all_scopes, analysis);
    
    // Cache the results
    function_analysis_cache_[function->name] = analysis;
    
    // Update the function's static analysis data for compatibility
    function->static_analysis.needed_parent_scopes = analysis.needed_parent_scopes;
    function->static_analysis.parent_location_indexes = analysis.parent_location_indexes;
    function->static_analysis.num_registers_needed = analysis.num_registers_needed;
    function->static_analysis.needs_r12 = analysis.needs_r12;
    function->static_analysis.needs_r13 = analysis.needs_r13;
    function->static_analysis.needs_r14 = analysis.needs_r14;
    function->static_analysis.function_instance_size = analysis.function_instance_size;
    function->static_analysis.local_scope_size = analysis.local_scope_size;
    
    std::cout << "[FUNCTION_SYSTEM] Analysis complete for " << function->name 
              << ": needs " << analysis.needed_parent_scopes.size() << " parent scopes, "
              << analysis.num_registers_needed << " registers, "
              << analysis.function_instance_size << " bytes function instance" << std::endl;
}

void FunctionInstanceSystem::compute_scope_dependencies(
    FunctionDecl* function, 
    LexicalScopeNode* function_scope,
    const std::unordered_map<int, LexicalScopeNode*>& all_scopes,
    CompleteFunctionAnalysis& analysis) {
    
    std::cout << "[FUNCTION_SYSTEM] Computing scope dependencies for " << function->name << std::endl;
    
    // Use the existing self_dependencies from lexical scope analysis
    std::unordered_map<int, size_t> scope_access_counts;
    
    for (const auto& dep : function_scope->self_dependencies) {
        int scope_depth = dep.definition_depth;
        size_t access_count = dep.access_count;
        
        // Skip current function's own scope
        if (scope_depth == function_scope->scope_depth) {
            continue;
        }
        
        analysis.needed_parent_scopes.push_back(scope_depth);
        scope_access_counts[scope_depth] += access_count;
        
        std::cout << "[FUNCTION_SYSTEM]   Depends on scope depth " << scope_depth 
                  << " with " << access_count << " accesses" << std::endl;
    }
    
    // Remove duplicates and sort by access frequency
    std::sort(analysis.needed_parent_scopes.begin(), analysis.needed_parent_scopes.end());
    analysis.needed_parent_scopes.erase(
        std::unique(analysis.needed_parent_scopes.begin(), analysis.needed_parent_scopes.end()),
        analysis.needed_parent_scopes.end()
    );
    
    // Create priority sorted list based on access frequency
    analysis.priority_sorted_parent_scopes = analysis.needed_parent_scopes;
    std::sort(analysis.priority_sorted_parent_scopes.begin(), 
             analysis.priority_sorted_parent_scopes.end(),
             [&scope_access_counts](int a, int b) {
                 return scope_access_counts[a] > scope_access_counts[b];
             });
    
    std::cout << "[FUNCTION_SYSTEM]   Priority sorted scopes: ";
    for (int depth : analysis.priority_sorted_parent_scopes) {
        std::cout << depth << "(" << scope_access_counts[depth] << ") ";
    }
    std::cout << std::endl;
}

void FunctionInstanceSystem::compute_register_allocation(CompleteFunctionAnalysis& analysis) {
    std::cout << "[FUNCTION_SYSTEM] Computing register allocation" << std::endl;
    
    // Allocate R12, R13, R14 to most frequently accessed scopes
    analysis.num_registers_needed = std::min((int)analysis.priority_sorted_parent_scopes.size(), 3);
    
    analysis.parent_location_indexes.resize(analysis.priority_sorted_parent_scopes.size(), -1);
    
    for (int i = 0; i < analysis.num_registers_needed; ++i) {
        analysis.parent_location_indexes[i] = i; // For now, simple 1:1 mapping
        
        switch (i) {
            case 0: analysis.needs_r12 = true; break;
            case 1: analysis.needs_r13 = true; break;
            case 2: analysis.needs_r14 = true; break;
        }
        
        std::cout << "[FUNCTION_SYSTEM]   R" << (12 + i) << " = scope depth " 
                  << analysis.priority_sorted_parent_scopes[i] << std::endl;
    }
    
    // Remaining scopes go on stack
    for (int i = 3; i < (int)analysis.priority_sorted_parent_scopes.size(); ++i) {
        std::cout << "[FUNCTION_SYSTEM]   Stack[" << (i-3) << "] = scope depth " 
                  << analysis.priority_sorted_parent_scopes[i] << std::endl;
    }
}

void FunctionInstanceSystem::compute_hidden_parameter_specification(
    const CompleteFunctionAnalysis& analysis,
    const std::unordered_map<int, LexicalScopeNode*>& all_scopes) {
    
    // This will be implemented when we handle function calls
    std::cout << "[FUNCTION_SYSTEM] Hidden parameter computation - TODO" << std::endl;
}

void FunctionInstanceSystem::compute_descendant_dependencies(
    FunctionDecl* function,
    const std::unordered_map<int, LexicalScopeNode*>& all_scopes,
    CompleteFunctionAnalysis& analysis) {
    
    // Add descendant dependencies from lexical scope
    if (function->lexical_scope) {
        for (const auto& dep : function->lexical_scope->descendant_dependencies) {
            analysis.all_descendant_scope_needs.insert(dep.definition_depth);
        }
    }
    
    std::cout << "[FUNCTION_SYSTEM] Added " << analysis.all_descendant_scope_needs.size() 
              << " descendant scope dependencies" << std::endl;
}

size_t FunctionInstanceSystem::compute_function_instance_size(const std::vector<int>& captured_scopes) const {
    // Function instance size = header (16 bytes) + (num_captured_scopes * 8 bytes)
    return 16 + (captured_scopes.size() * 8);
}

// Phase 2: Pure assembly function instance creation
void FunctionInstanceSystem::emit_function_instance_creation_pure_asm(
    CodeGenerator& gen, 
    FunctionDecl* function,
    const CompleteFunctionAnalysis& analysis) {
    
    std::cout << "[FUNCTION_SYSTEM] Emitting function instance creation for " << function->name << std::endl;
    
    X86CodeGenV2* x86_gen = dynamic_cast<X86CodeGenV2*>(&gen);
    if (!x86_gen) {
        throw std::runtime_error("Function instance creation requires X86CodeGenV2");
    }
    
    size_t total_size = analysis.function_instance_size;
    
    // STEP 1: Pure ASM heap allocation
    emit_heap_allocation_pure_asm(gen, total_size);
    x86_gen->emit_mov_reg_reg(10, 0); // R10 = allocated memory address
    
    // STEP 2: Initialize FunctionInstance header (pure ASM)
    // Store size field: [R10 + 0] = total_size
    x86_gen->emit_mov_reg_imm(11, total_size);
    x86_gen->emit_mov_reg_offset_reg(10, 0, 11);
    
    // Store placeholder function code address: [R10 + 8] = 0 (will be patched)
    x86_gen->emit_mov_reg_imm(11, 0);
    x86_gen->emit_mov_reg_offset_reg(10, 8, 11);
    
    // STEP 3: Capture lexical scope addresses (pure ASM)
    size_t scope_offset = 16; // Start after header
    
    for (size_t i = 0; i < analysis.priority_sorted_parent_scopes.size(); i++) {
        int scope_depth = analysis.priority_sorted_parent_scopes[i];
        
        // Emit pure ASM to get scope address for this depth
        emit_scope_address_capture(gen, scope_depth, scope_offset);
        scope_offset += 8;
    }
    
    // STEP 4: Return pointer to function instance (RAX = R10)
    x86_gen->emit_mov_reg_reg(0, 10);
    
    std::cout << "[FUNCTION_SYSTEM] Function instance creation complete for " << function->name 
              << " (" << total_size << " bytes)" << std::endl;
}

// FunctionExpression version of the same method
void FunctionInstanceSystem::emit_function_instance_creation_pure_asm(
    CodeGenerator& gen, 
    FunctionExpression* function,
    const CompleteFunctionAnalysis& analysis) {
    
    std::cout << "[FUNCTION_SYSTEM] Emitting function instance creation for FunctionExpression" << std::endl;
    
    X86CodeGenV2* x86_gen = dynamic_cast<X86CodeGenV2*>(&gen);
    if (!x86_gen) {
        throw std::runtime_error("Function instance creation requires X86CodeGenV2");
    }
    
    size_t total_size = analysis.function_instance_size;
    if (total_size == 0) {
        total_size = 16; // Minimum size for function instance header
    }
    
    std::cout << "[FUNCTION_SYSTEM] Creating function instance of size " << total_size << " bytes" << std::endl;
    
    // STEP 1: Pure ASM heap allocation
    emit_heap_allocation_pure_asm(gen, total_size);
    x86_gen->emit_mov_reg_reg(10, 0); // R10 = allocated memory address
    
    // STEP 2: Initialize FunctionInstance header (pure ASM)
    // Store size field: [R10 + 0] = total_size
    x86_gen->emit_mov_reg_imm(11, total_size);
    x86_gen->emit_mov_reg_offset_reg(10, 0, 11);
    
    // Store placeholder function code address: [R10 + 8] = 0 (will be patched)
    x86_gen->emit_mov_reg_imm(11, 0);
    x86_gen->emit_mov_reg_offset_reg(10, 8, 11);
    
    // STEP 3: Capture lexical scope addresses (simplified for now)
    size_t scope_offset = 16; // Start after header
    
    for (size_t i = 0; i < analysis.priority_sorted_parent_scopes.size(); i++) {
        int scope_depth = analysis.priority_sorted_parent_scopes[i];
        
        // Simplified scope address capture for testing
        emit_scope_address_capture(gen, scope_depth, scope_offset);
        scope_offset += 8;
    }
    
    // STEP 4: Return pointer to function instance (RAX = R10)
    x86_gen->emit_mov_reg_reg(0, 10);
    
    std::cout << "[FUNCTION_SYSTEM] Function instance creation complete for FunctionExpression" 
              << " (" << total_size << " bytes)" << std::endl;
}

void FunctionInstanceSystem::emit_scope_address_capture(CodeGenerator& gen,
                                                       int scope_depth,
                                                       int target_offset) {
    X86CodeGenV2* x86_gen = dynamic_cast<X86CodeGenV2*>(&gen);
    
    // Pure ASM to capture scope address based on current register state
    // This assumes we're in a context where the parent scope registers are loaded
    
    if (scope_depth == 1) {
        // Global scope - use a known global scope address (to be set up at startup)
        x86_gen->emit_mov_reg_imm(11, 0); // Will be patched with global scope address
        std::cout << "[FUNCTION_SYSTEM]   Capturing global scope at offset " << target_offset << std::endl;
    } else {
        // For now, use a runtime call - will be replaced with pure ASM once scope tracking is complete
        x86_gen->emit_mov_reg_imm(7, scope_depth); // RDI = scope depth
        x86_gen->emit_call("__get_scope_address_for_depth"); // Returns address in RAX
        x86_gen->emit_mov_reg_reg(11, 0); // R11 = scope address
        std::cout << "[FUNCTION_SYSTEM]   Capturing scope depth " << scope_depth 
                  << " at offset " << target_offset << std::endl;
    }
    
    // Store captured scope address in function instance: [R10 + target_offset] = R11
    x86_gen->emit_mov_reg_offset_reg(10, target_offset, 11);
}

// Phase 3: Pure assembly function calling with hidden parameters
void FunctionInstanceSystem::emit_function_call_with_hidden_parameters(
    CodeGenerator& gen,
    const std::string& function_name,
    const std::vector<std::unique_ptr<ASTNode>>& arguments) {
    
    std::cout << "[FUNCTION_SYSTEM] Emitting function call with hidden parameters for " << function_name << std::endl;
    
    X86CodeGenV2* x86_gen = dynamic_cast<X86CodeGenV2*>(&gen);
    if (!x86_gen) {
        throw std::runtime_error("Function calling requires X86CodeGenV2");
    }
    
    // Look up analysis for called function
    auto analysis_it = function_analysis_cache_.find(function_name);
    if (analysis_it == function_analysis_cache_.end()) {
        std::cout << "[FUNCTION_SYSTEM] No analysis found for " << function_name 
                  << ", using simple call" << std::endl;
        x86_gen->emit_call(function_name);
        return;
    }
    
    const CompleteFunctionAnalysis& analysis = analysis_it->second;
    
    // Generate regular function arguments first
    for (const auto& arg : arguments) {
        arg->generate_code(gen);
        // TODO: Handle proper argument passing based on calling convention
    }
    
    // Generate hidden scope parameters
    std::cout << "[FUNCTION_SYSTEM] Passing " << analysis.needed_parent_scopes.size() 
              << " hidden scope parameters" << std::endl;
    
    // For each scope the called function needs, pass the appropriate address
    for (size_t i = 0; i < analysis.needed_parent_scopes.size(); ++i) {
        int needed_scope_depth = analysis.needed_parent_scopes[i];
        
        // Determine where to get this scope address from current context
        if (needed_scope_depth == 1) {
            // Global scope - pass global scope address
            x86_gen->emit_mov_reg_imm(7, 0); // Will be patched with global scope address
        } else {
            // For now, pass current R15 (local scope) - this needs proper scope mapping
            x86_gen->emit_mov_reg_reg(7, 15); // RDI = R15 (current local scope)
        }
        
        // Push scope address as hidden parameter (will be handled by callee's prologue)
        x86_gen->emit_push_reg(7);
    }
    
    // Make the actual function call
    x86_gen->emit_call(function_name);
    
    // Clean up stack (pop hidden parameters)
    if (!analysis.needed_parent_scopes.empty()) {
        size_t stack_cleanup = analysis.needed_parent_scopes.size() * 8;
        x86_gen->emit_add_reg_imm(4, stack_cleanup); // RSP += stack_cleanup
    }
}

// Phase 4: Pure assembly function prologue with scope register loading
void FunctionInstanceSystem::emit_function_prologue_with_scope_loading(
    CodeGenerator& gen,
    FunctionDecl* function,
    const CompleteFunctionAnalysis& analysis) {
    
    std::cout << "[FUNCTION_SYSTEM] Emitting function prologue with scope loading for " << function->name << std::endl;
    
    X86CodeGenV2* x86_gen = dynamic_cast<X86CodeGenV2*>(&gen);
    if (!x86_gen) {
        throw std::runtime_error("Function prologue generation requires X86CodeGenV2");
    }
    
    // Standard function prologue
    x86_gen->emit_push_reg(5);      // push rbp
    x86_gen->emit_mov_reg_reg(5, 4); // mov rbp, rsp
    
    // Save caller's scope registers that we'll overwrite
    if (analysis.needs_r12) x86_gen->emit_push_reg(12); // push r12
    if (analysis.needs_r13) x86_gen->emit_push_reg(13); // push r13  
    if (analysis.needs_r14) x86_gen->emit_push_reg(14); // push r14
    
    // Allocate local scope on heap
    if (analysis.local_scope_size > 0) {
        emit_heap_allocation_pure_asm(gen, analysis.local_scope_size);
        x86_gen->emit_mov_reg_reg(15, 0); // R15 = local scope address
        std::cout << "[FUNCTION_SYSTEM]   Allocated " << analysis.local_scope_size 
                  << " bytes for local scope in R15" << std::endl;
    }
    
    // Load parent scopes from hidden parameters into registers
    int param_index = 0;
    for (int i = 0; i < analysis.num_registers_needed; ++i) {
        int target_register = 12 + i;
        // Load from stack parameter: mov rN, [rbp + 16 + param_index*8]
        int64_t stack_offset = 16 + param_index * 8;
        x86_gen->emit_mov_reg_reg_offset(target_register, 5, stack_offset);
        param_index++;
        
        std::cout << "[FUNCTION_SYSTEM]   Loaded R" << target_register 
                  << " from stack parameter " << param_index-1 << std::endl;
    }
    
    std::cout << "[FUNCTION_SYSTEM] Prologue complete for " << function->name << std::endl;
}

void FunctionInstanceSystem::emit_function_epilogue_with_scope_restoration(
    CodeGenerator& gen,
    FunctionDecl* function,
    const CompleteFunctionAnalysis& analysis) {
    
    std::cout << "[FUNCTION_SYSTEM] Emitting function epilogue for " << function->name << std::endl;
    
    X86CodeGenV2* x86_gen = dynamic_cast<X86CodeGenV2*>(&gen);
    
    // Restore caller's scope registers
    if (analysis.needs_r14) x86_gen->emit_pop_reg(14); // pop r14
    if (analysis.needs_r13) x86_gen->emit_pop_reg(13); // pop r13
    if (analysis.needs_r12) x86_gen->emit_pop_reg(12); // pop r12
    
    // Standard function epilogue
    x86_gen->emit_mov_reg_reg(4, 5); // mov rsp, rbp
    x86_gen->emit_pop_reg(5);        // pop rbp
    x86_gen->emit_return();          // ret
}

// Analysis result accessors
const CompleteFunctionAnalysis& FunctionInstanceSystem::get_function_analysis(const std::string& function_name) const {
    static CompleteFunctionAnalysis empty_analysis;
    auto it = function_analysis_cache_.find(function_name);
    return (it != function_analysis_cache_.end()) ? it->second : empty_analysis;
}

// Utility functions for pure assembly generation
void emit_heap_allocation_pure_asm(CodeGenerator& gen, size_t size) {
    X86CodeGenV2* x86_gen = dynamic_cast<X86CodeGenV2*>(&gen);
    if (!x86_gen) return;
    
    // For now, use malloc - will be replaced with custom heap allocator
    x86_gen->emit_mov_reg_imm(7, size); // RDI = size
    x86_gen->emit_call("malloc");       // RAX = allocated memory
}

void FunctionInstanceSystem::compute_function_variable_strategies(const std::unordered_map<int, LexicalScopeNode*>& all_scopes) {
    std::cout << "[FUNCTION_SYSTEM] Computing function variable strategies" << std::endl;
    
    for (const auto& scope_pair : all_scopes) {
        LexicalScopeNode* scope = scope_pair.second;
        
        for (const auto& var_decl : scope->variable_declarations) {
            const std::string& var_name = var_decl.first;
            const VariableDeclarationInfo& var_info = var_decl.second;
            
            // Determine strategy based on variable usage patterns
            FunctionVariableStrategy strategy = FunctionVariableStrategy::ANY_TYPED_DYNAMIC;
            
            if (var_info.data_type == DataType::LOCAL_FUNCTION_INSTANCE && 
                var_info.modification_count == 0) {
                // Never reassigned function variable
                strategy = FunctionVariableStrategy::STATIC_SINGLE_ASSIGNMENT;
            } else if (var_info.data_type == DataType::LOCAL_FUNCTION_INSTANCE || 
                      var_info.data_type == DataType::POINTER_FUNCTION_INSTANCE) {
                // Function-typed variable
                strategy = FunctionVariableStrategy::FUNCTION_TYPED;
            }
            
            variable_strategies_[var_name] = strategy;
            
            std::cout << "[FUNCTION_SYSTEM]   Variable " << var_name << " -> ";
            switch (strategy) {
                case FunctionVariableStrategy::STATIC_SINGLE_ASSIGNMENT:
                    std::cout << "STATIC_SINGLE_ASSIGNMENT"; break;
                case FunctionVariableStrategy::FUNCTION_TYPED:
                    std::cout << "FUNCTION_TYPED"; break;
                case FunctionVariableStrategy::ANY_TYPED_DYNAMIC:
                    std::cout << "ANY_TYPED_DYNAMIC"; break;
            }
            std::cout << std::endl;
        }
    }
}

FunctionVariableStrategy FunctionInstanceSystem::get_variable_strategy(const std::string& variable_name) const {
    auto it = variable_strategies_.find(variable_name);
    return (it != variable_strategies_.end()) ? it->second : FunctionVariableStrategy::ANY_TYPED_DYNAMIC;
}
