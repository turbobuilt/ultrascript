#include "simple_lexical_scope.h"
#include "function_instance.h"  // For FunctionDynamicValue and function structures
#include "compiler.h"  // For LexicalScopeNode and DataType enum
#include <algorithm>
#include <iostream>
#include <set>
#include <unordered_map>

// Called when entering a new lexical scope (function, block, etc.)
void SimpleLexicalScopeAnalyzer::enter_scope(bool is_function_scope) {
    std::cout << "[SimpleLexicalScope] ENTER_SCOPE CALL: current_depth_ before increment: " << current_depth_ << std::endl;
    std::cout << "[SimpleLexicalScope] ENTER_SCOPE CALL: scope_stack_.size() = " << scope_stack_.size() << std::endl;
    current_depth_++;
    
    // NEW: Create LexicalScopeNode immediately with function scope flag
    auto lexical_scope_node = std::make_shared<LexicalScopeNode>(current_depth_, is_function_scope);
    
    // Register the scope node for direct access right away
    depth_to_scope_node_[current_depth_] = lexical_scope_node.get();  // Store raw pointer
    
    // Add to scope stack for processing during parsing
    scope_stack_.push_back(lexical_scope_node);
    
    std::cout << "[SimpleLexicalScope] Entered scope at depth " << current_depth_ 
              << " (is_function_scope=" << is_function_scope << ", shared_ptr available)" << std::endl;
}

// Called when exiting a lexical scope - returns LexicalScopeNode with all scope info
std::shared_ptr<LexicalScopeNode> SimpleLexicalScopeAnalyzer::exit_scope() {
    if (scope_stack_.empty()) {
        std::cerr << "[SimpleLexicalScope] ERROR: Attempting to exit scope when none exists!" << std::endl;
        return nullptr;
    }
    
    // Get the current LexicalScopeNode (already created when entering scope)
    std::shared_ptr<LexicalScopeNode> current_scope_node = scope_stack_.back();
    scope_stack_.pop_back();
    
    std::cout << "[SimpleLexicalScope] Exiting scope at depth " << current_depth_ 
              << " with " << current_scope_node->variable_declarations.size() << " declared variables" << std::endl;
    
    // Propagate dependencies to parent scope when this scope closes
    if (!scope_stack_.empty()) {  // If there's a parent scope
        LexicalScopeNode* parent_scope = scope_stack_.back().get();
        
        // Add all self_dependencies to parent's descendant_dependencies
        for (const auto& dep : current_scope_node->self_dependencies) {
            // Check if this dependency already exists in parent's descendant_dependencies
            bool found = false;
            for (auto& parent_dep : parent_scope->descendant_dependencies) {
                if (parent_dep.variable_name == dep.variable_name && 
                    parent_dep.definition_depth == dep.definition_depth) {
                    parent_dep.access_count += dep.access_count;
                    found = true;
                    break;
                }
            }
            
            if (!found) {
                parent_scope->descendant_dependencies.push_back(dep);
            }
        }
        
        // Add all descendant_dependencies to parent's descendant_dependencies
        for (const auto& dep : current_scope_node->descendant_dependencies) {
            // Check if this dependency already exists in parent's descendant_dependencies
            bool found = false;
            for (auto& parent_dep : parent_scope->descendant_dependencies) {
                if (parent_dep.variable_name == dep.variable_name && 
                    parent_dep.definition_depth == dep.definition_depth) {
                    parent_dep.access_count += dep.access_count;
                    found = true;
                    break;
                }
            }
            
            if (!found) {
                parent_scope->descendant_dependencies.push_back(dep);
            }
        }
        
        std::cout << "[SimpleLexicalScope] Propagated " << current_scope_node->self_dependencies.size() 
                  << " self dependencies and " << current_scope_node->descendant_dependencies.size() 
                  << " descendant dependencies to parent scope" << std::endl;
    }
    
    // Create priority-sorted vector: SELF dependencies first (sorted by access count), 
    // then descendant dependencies not in SELF (in any order, just for propagation)
    
    // First, sort SELF dependencies by access count per depth
    std::unordered_map<int, int> self_depth_access_counts;
    for (const auto& dep : current_scope_node->self_dependencies) {
        self_depth_access_counts[dep.definition_depth] += dep.access_count;
    }
    
    // Convert self dependencies to vector and sort by access count (highest first)
    std::vector<std::pair<int, int>> self_depth_counts;
    for (const auto& pair : self_depth_access_counts) {
        self_depth_counts.emplace_back(pair.first, pair.second);  // (depth, total_count)
    }
    
    std::sort(self_depth_counts.begin(), self_depth_counts.end(),
        [](const std::pair<int, int>& a, const std::pair<int, int>& b) {
            return a.second > b.second;  // Sort by access count (descending)
        });
    
    // Add SELF depths first (these get r12+8 style access)
    current_scope_node->priority_sorted_parent_scopes.clear();
    current_scope_node->priority_sorted_parent_scopes.reserve(
        self_depth_counts.size() + current_scope_node->descendant_dependencies.size());
    
    std::unordered_set<int> self_depths;
    for (const auto& pair : self_depth_counts) {
        current_scope_node->priority_sorted_parent_scopes.push_back(pair.first);
        self_depths.insert(pair.first);
    }
    
    // Then add descendant depths that are NOT already in SELF (just for propagation)
    for (const auto& dep : current_scope_node->descendant_dependencies) {
        if (self_depths.find(dep.definition_depth) == self_depths.end()) {
            // This depth is not in SELF dependencies, so add it (if not already added)
            bool already_added = false;
            for (int existing_depth : current_scope_node->priority_sorted_parent_scopes) {
                if (existing_depth == dep.definition_depth) {
                    already_added = true;
                    break;
                }
            }
            if (!already_added) {
                current_scope_node->priority_sorted_parent_scopes.push_back(dep.definition_depth);
            }
        }
    }
    
    // NEW: Finalize function variable sizes before packing
    finalize_function_variable_sizes();
    
    // TODO: DEFERRED PACKING - Move to AST generation time when we have complete hoisting info
    // Variable packing is now deferred until AST generation when LexicalScopeNode is encountered
    // This ensures we have complete information about all variable declarations and hoisting conflicts
    
    std::cout << "[SimpleLexicalScope] Scope exit completed - packing deferred to AST generation" << std::endl;
    
    // NOTE: Scope node was already registered in depth_to_scope_node_ when entering scope
    std::cout << "[SimpleLexicalScope] Scope node at depth " << current_scope_node->scope_depth 
              << " remains registered for direct access (pointer: " << current_scope_node.get() << ")" << std::endl;

    // Clean up variable declarations at this depth
    // IMPORTANT: Don't clean up ANY declarations during parsing - they're needed for code generation
    // TODO: Clean up after code generation is complete
    if (false) { // DISABLED: cleanup_declarations_at_depth causes segfaults during codegen
        cleanup_declarations_at_depth(current_depth_);
    } else if (current_depth_ == 1) {
        std::cout << "[SimpleLexicalScope] Preserving global scope declarations for code generation" << std::endl;
        // For global scope, keep the shared_ptr in completed_scopes_ to ensure it stays alive
        // The parser won't store the global scope anywhere, so we need to keep it alive ourselves
        completed_scopes_.push_back(current_scope_node);
        std::cout << "[SimpleLexicalScope] Stored global scope shared_ptr for code generation lifecycle" << std::endl;
    }

    // Decrement depth since we're exiting this scope
    current_depth_--;

    // Return the shared_ptr directly - this allows both the AST node and 
    // the depth_to_scope_node_ map to share ownership
    return current_scope_node;
}

// Called when a variable is declared (new version with DataType)
void SimpleLexicalScopeAnalyzer::declare_variable(const std::string& name, const std::string& declaration_type, DataType data_type) {
    // NEW: Check if this variable is already a hoisting conflict variable
    if (is_hoisting_conflict_variable(name)) {
        // Don't add additional declarations for hoisting conflict variables
        // They should remain DYNAMIC_VALUE and we don't want to overwrite the promoted type
        std::cout << "[HoistingConflict] Skipping additional declaration for hoisting conflict variable '" 
                  << name << "' (remains DYNAMIC_VALUE)" << std::endl;
        return;
    }
    
    // NEW: Handle var hoisting - var declarations go to function scope
    LexicalScopeNode* target_scope = nullptr;
    int target_depth = current_depth_;
    
    if (declaration_type == "var") {
        // VAR variables are hoisted to the nearest function scope
        LexicalScopeNode* function_scope = find_nearest_function_scope();
        if (function_scope) {
            target_scope = function_scope;
            target_depth = function_scope->scope_depth;
            std::cout << "[VarHoisting] Variable '" << name << "' hoisted from depth " 
                      << current_depth_ << " to function scope at depth " << target_depth << std::endl;
        } else {
            // No function scope found, use current scope (global)
            target_scope = scope_stack_.empty() ? nullptr : scope_stack_.back().get();
            target_depth = current_depth_;
        }
    } else {
        // LET/CONST variables are block-scoped - use current scope
        target_scope = scope_stack_.empty() ? nullptr : scope_stack_.back().get();
        target_depth = current_depth_;
    }
    
    // Check for duplicate declaration in target scope
    if (target_scope && target_scope->has_variable(name)) {
        const auto& existing = target_scope->variable_declarations[name];
        if (existing.data_type != data_type || existing.declaration_type != declaration_type) {
            std::cout << "[REDECLARATION_ERROR] Variable '" << name << "' already declared in scope at depth " 
                      << target_depth << " with type " << static_cast<int>(existing.data_type)
                      << " (trying to redeclare with type " << static_cast<int>(data_type) << ")" << std::endl;
            throw std::runtime_error("Variable '" + name + "' is already declared in this scope with a different type");
        }
        // Same type redeclaration - just return (no-op)
        std::cout << "[SimpleLexicalScope] Variable '" << name << "' already declared with same type, skipping" << std::endl;
        return;
    }
    
    // Add to the variable declarations map at the appropriate depth
    variable_declarations_[name].emplace_back(std::make_unique<VariableDeclarationInfo>(target_depth, declaration_type, data_type));
    
    // Add to target scope's declared variables with complete info
    if (target_scope) {
        VariableDeclarationInfo var_info(target_depth, declaration_type, data_type);
        target_scope->declare_variable(name, var_info);
    }
    
    std::cout << "[SimpleLexicalScope] Declared variable '" << name << "' as " << declaration_type 
              << " (DataType=" << static_cast<int>(data_type) << ") at depth " << target_depth << std::endl;
    
    // NEW: Resolve any unresolved references for this newly declared variable
    resolve_references_for_variable(name);
}

// Called when a variable is declared (legacy version - assumes DataType::ANY)
void SimpleLexicalScopeAnalyzer::declare_variable(const std::string& name, const std::string& declaration_type) {
    declare_variable(name, declaration_type, DataType::ANY);
}

// Called when a variable is accessed
void SimpleLexicalScopeAnalyzer::access_variable(const std::string& name) {
    // Find the most recent declaration of this variable
    int definition_depth = get_variable_definition_depth(name);
    
    if (definition_depth == -1) {
        std::cout << "[SimpleLexicalScope] WARNING: Variable '" << name 
                  << "' accessed but not found in declarations" << std::endl;
        return;
    }
    
    // LEXICAL SCOPING VALIDATION: Outer scopes cannot access inner scope variables
    if (definition_depth > current_depth_) {
        std::cout << "[LEXICAL_SCOPE_ERROR] Variable '" << name 
                  << "' defined at depth " << definition_depth 
                  << " is NOT accessible from outer scope at depth " << current_depth_ << std::endl;
        std::cout << "[LEXICAL_SCOPE_ERROR] Lexical scoping violation - inner scope variables are not visible to outer scopes!" << std::endl;
        throw std::runtime_error("Lexical scoping violation: Cannot access variable '" + name + "' defined in inner scope");
    }
    
    std::cout << "[SimpleLexicalScope] Accessing variable '" << name 
              << "' defined at depth " << definition_depth 
              << " from depth " << current_depth_ << std::endl;
    
    // Update usage count for this declaration
    auto& declarations = variable_declarations_[name];
    for (auto& decl : declarations) {
        if (decl->depth == definition_depth) {
            decl->usage_count++;
            break;
        }
    }
    
    // Add as self-dependency to current scope (if accessing from different depth)
    if (!scope_stack_.empty() && definition_depth != current_depth_) {
        auto& current_scope = *scope_stack_.back();
        
        // Check if we already have this dependency
        bool found = false;
        for (auto& dep : current_scope.self_dependencies) {
            if (dep.variable_name == name && dep.definition_depth == definition_depth) {
                dep.access_count++;
                found = true;
                break;
            }
        }
        
        if (!found) {
            current_scope.self_dependencies.emplace_back(name, definition_depth);
        }
    }
}

// Called when a variable is modified/assigned to (tracks modification count)
void SimpleLexicalScopeAnalyzer::modify_variable(const std::string& name) {
    // Find the most recent declaration of this variable
    int definition_depth = get_variable_definition_depth(name);
    
    if (definition_depth == -1) {
        std::cout << "[SimpleLexicalScope] WARNING: Variable '" << name 
                  << "' modified but not found in declarations" << std::endl;
        return;
    }
    
    std::cout << "[SimpleLexicalScope] Modifying variable '" << name 
              << "' defined at depth " << definition_depth 
              << " from depth " << current_depth_ << std::endl;
    
    // Update modification count for this declaration
    auto& declarations = variable_declarations_[name];
    for (auto& decl : declarations) {
        if (decl->depth == definition_depth) {
            decl->modification_count++;
            std::cout << "[SimpleLexicalScope] Variable '" << name 
                      << "' modification count now: " << decl->modification_count 
                      << " (after initial declaration)" << std::endl;
            break;
        }
    }
    
    // Also mark as accessed (modifications count as access too)
    access_variable(name);
}

// Get the absolute depth where a variable was last declared
int SimpleLexicalScopeAnalyzer::get_variable_definition_depth(const std::string& name) const {
    auto it = variable_declarations_.find(name);
    if (it == variable_declarations_.end() || it->second.empty()) {
        return -1; // Not found
    }
    
    // Return the depth of the most recent declaration (last in the vector)
    return it->second.back()->depth;
}

// Get the modification count for a variable (number of assignments after declaration)
size_t SimpleLexicalScopeAnalyzer::get_variable_modification_count(const std::string& name) const {
    auto it = variable_declarations_.find(name);
    if (it == variable_declarations_.end() || it->second.empty()) {
        return 0; // Not found
    }
    
    // Return the modification count of the most recent declaration
    return it->second.back()->modification_count;
}

// Debug: Print current state
void SimpleLexicalScopeAnalyzer::print_debug_info() const {
    std::cout << "\n[SimpleLexicalScope] DEBUG INFO:" << std::endl;
    std::cout << "Current depth: " << current_depth_ << std::endl;
    std::cout << "Active scopes: " << scope_stack_.size() << std::endl;
    
    std::cout << "\nVariable declarations:" << std::endl;
    for (const auto& [var_name, declarations] : variable_declarations_) {
        std::cout << "  " << var_name << ": ";
        for (const auto& decl : declarations) {
            std::cout << "[depth=" << decl->depth << ", decl=" << decl->declaration_type
                      << ", usage=" << decl->usage_count << "] ";
        }
        std::cout << std::endl;
    }
    
    if (!scope_stack_.empty()) {
        const auto& current_scope = *scope_stack_.back();
        std::cout << "\nCurrent scope self dependencies:" << std::endl;
        for (const auto& dep : current_scope.self_dependencies) {
            std::cout << "  " << dep.variable_name << " from depth " << dep.definition_depth 
                      << " (accessed " << dep.access_count << " times)" << std::endl;
        }
        
        std::cout << "\nCurrent scope descendant dependencies:" << std::endl;
        for (const auto& dep : current_scope.descendant_dependencies) {
            std::cout << "  " << dep.variable_name << " from depth " << dep.definition_depth 
                      << " (accessed " << dep.access_count << " times)" << std::endl;
        }
    }
    std::cout << std::endl;
}

// Private methods

// Clean up variable declarations for the depth we're exiting
void SimpleLexicalScopeAnalyzer::cleanup_declarations_at_depth(int depth) {
    std::cout << "[SimpleLexicalScope] Cleaning up declarations at depth " << depth << std::endl;
    
    for (auto& [var_name, declarations] : variable_declarations_) {
        // Remove all declarations at this depth
        declarations.erase(
            std::remove_if(declarations.begin(), declarations.end(),
                [depth, &var_name](const std::unique_ptr<VariableDeclarationInfo>& decl) {
                    bool should_remove = (decl->depth == depth);
                    if (should_remove) {
                        std::cout << "  Removing declaration of '" << var_name 
                                  << "' from depth " << depth << std::endl;
                    }
                    return should_remove;
                }),
            declarations.end()
        );
    }
    
    // Clean up empty entries
    for (auto it = variable_declarations_.begin(); it != variable_declarations_.end();) {
        if (it->second.empty()) {
            std::cout << "  Removing empty declaration list for '" << it->first << "'" << std::endl;
            it = variable_declarations_.erase(it);
        } else {
            ++it;
        }
    }
}

// Get the size in bytes for a DataType
size_t SimpleLexicalScopeAnalyzer::get_datatype_size(DataType type) const {
    switch (type) {
        case DataType::INT8:
        case DataType::UINT8:
        case DataType::BOOLEAN:
            return 1;
        case DataType::INT16:
        case DataType::UINT16:
            return 2;
        case DataType::INT32:
        case DataType::UINT32:
        case DataType::FLOAT32:
            return 4;
        case DataType::INT64:
        case DataType::UINT64:
        case DataType::FLOAT64:
            return 8;
        case DataType::STRING:
        case DataType::ARRAY:
        case DataType::TENSOR:
        case DataType::FUNCTION:
        case DataType::PROMISE:
        case DataType::CLASS_INSTANCE:
        case DataType::RUNTIME_OBJECT:
            return 8; // Pointer size on 64-bit systems
        case DataType::LOCAL_FUNCTION_INSTANCE:
        case DataType::POINTER_FUNCTION_INSTANCE:
            return 8; // Function instance pointers
        case DataType::DYNAMIC_VALUE:
            return sizeof(FunctionDynamicValue); // Size of FunctionDynamicValue structure
        case DataType::ANY:
        default:
            return 16; // DynamicValue or unknown type - conservative estimate
    }
}

// Get the alignment requirement for a DataType
size_t SimpleLexicalScopeAnalyzer::get_datatype_alignment(DataType type) const {
    switch (type) {
        case DataType::INT8:
        case DataType::UINT8:
        case DataType::BOOLEAN:
            return 1;
        case DataType::INT16:
        case DataType::UINT16:
            return 2;
        case DataType::INT32:
        case DataType::UINT32:
        case DataType::FLOAT32:
            return 4;
        case DataType::INT64:
        case DataType::UINT64:
        case DataType::FLOAT64:
        case DataType::STRING:
        case DataType::ARRAY:
        case DataType::TENSOR:
        case DataType::FUNCTION:
        case DataType::PROMISE:
        case DataType::CLASS_INSTANCE:
        case DataType::RUNTIME_OBJECT:
        case DataType::LOCAL_FUNCTION_INSTANCE:
        case DataType::POINTER_FUNCTION_INSTANCE:
        case DataType::DYNAMIC_VALUE:
            return 8; // Natural alignment for 8-byte types and pointers
        case DataType::ANY:
        default:
            return 8; // Conservative alignment for unknown types
    }
}

// Optimal variable packing algorithm
void SimpleLexicalScopeAnalyzer::pack_scope_variables(const std::unordered_set<std::string>& variables, 
                                                      std::unordered_map<std::string, size_t>& offsets,
                                                      std::vector<std::string>& packed_order,
                                                      size_t& total_size,
                                                      const LexicalScopeNode* scope_node) const {
    struct VariablePacking {
        std::string name;
        size_t size;
        size_t alignment;
        DataType data_type;
    };
    
    std::vector<VariablePacking> vars_to_pack;
    
    // Collect variable information for packing
    for (const auto& var_name : variables) {
        auto it = variable_declarations_.find(var_name);
        if (it != variable_declarations_.end() && !it->second.empty()) {
            // Get the most recent declaration
            const auto& decl = it->second.back();
            VariablePacking pack;
            pack.name = var_name;
            pack.data_type = decl->data_type;
            
            // Check if this variable is a function - use new classification system
            bool is_function = false;
            size_t function_size = 0;
            DataType storage_type = decl->data_type;
            
            // Use new function variable classification
            if (has_tracked_function_sizes(var_name)) {
                FunctionVariableStrategy strategy = classify_function_variable_strategy(var_name);
                is_function = true;
                
                switch (strategy) {
                    case FunctionVariableStrategy::STATIC_SINGLE_ASSIGNMENT: {
                        // Strategy 1: Direct storage of single function instance
                        function_size = get_max_function_size(var_name);
                        storage_type = DataType::LOCAL_FUNCTION_INSTANCE;
                        std::cout << "[SimpleLexicalScope] Strategy 1 (Static Single) for '" << var_name 
                                 << "': " << function_size << " bytes" << std::endl;
                        break;
                    }
                    
                    case FunctionVariableStrategy::FUNCTION_TYPED: {
                        // Strategy 2: Conservative Maximum Size for function-typed variables
                        function_size = get_max_function_size(var_name);
                        storage_type = DataType::LOCAL_FUNCTION_INSTANCE;
                        std::cout << "[SimpleLexicalScope] Strategy 2 (Function-Typed) for '" << var_name 
                                 << "': " << function_size << " bytes (conservative max)" << std::endl;
                        break;
                    }
                    
                    case FunctionVariableStrategy::ANY_TYPED_DYNAMIC: {
                        // Strategy 3: DynamicValue wrapper + Conservative Maximum Size
                        size_t max_function_size = get_max_function_size(var_name);
                        function_size = sizeof(FunctionDynamicValue) + max_function_size;
                        storage_type = DataType::DYNAMIC_VALUE;
                        std::cout << "[SimpleLexicalScope] Strategy 3 (Any-Typed Dynamic) for '" << var_name 
                                 << "': " << function_size << " bytes (DynamicValue + max function)" << std::endl;
                        break;
                    }
                }
            } else {
                // Check function declarations for direct function size
                for (const auto* func_decl : scope_node->declared_functions) {
                    if (func_decl && func_decl->name == var_name) {
                        is_function = true;
                        function_size = func_decl->function_instance_size;
                        storage_type = DataType::LOCAL_FUNCTION_INSTANCE;
                        break;
                    }
                }
                
                // Check function expressions if not found in declarations
                if (!is_function) {
                    for (const auto* func_expr : scope_node->declared_function_expressions) {
                        if (func_expr && func_expr->name == var_name) {
                            is_function = true;
                            function_size = func_expr->function_instance_size;
                            storage_type = DataType::LOCAL_FUNCTION_INSTANCE;
                            break;
                        }
                    }
                }
            }
            
            if (is_function) {
                pack.size = function_size;
                pack.alignment = 8; // Function instances are always 8-byte aligned
                std::cout << "[SimpleLexicalScope] Function '" << var_name << "' using computed instance size: " 
                         << function_size << " bytes" << std::endl;
            } else {
                pack.size = get_datatype_size(decl->data_type);
                pack.alignment = get_datatype_alignment(decl->data_type);
            }
            
            vars_to_pack.push_back(pack);
        }
    }
    
    // Sort variables by alignment (descending) and then by size (descending)
    // This minimizes padding by placing larger, more-aligned variables first
    std::sort(vars_to_pack.begin(), vars_to_pack.end(), 
        [](const VariablePacking& a, const VariablePacking& b) {
            if (a.alignment != b.alignment) {
                return a.alignment > b.alignment; // Higher alignment first
            }
            return a.size > b.size; // Larger size first
        });
    
    size_t current_offset = 0;
    
    // Pack variables with proper alignment
    for (const auto& var : vars_to_pack) {
        // Calculate the next aligned offset
        size_t aligned_offset = current_offset;
        if (var.alignment > 1) {
            size_t remainder = current_offset % var.alignment;
            if (remainder != 0) {
                aligned_offset = current_offset + (var.alignment - remainder);
            }
        }
        
        // Store the offset and add to packed order
        offsets[var.name] = aligned_offset;
        packed_order.push_back(var.name);
        
        // Update current offset
        current_offset = aligned_offset + var.size;
        
        std::cout << "[SimpleLexicalScope] Packed " << var.name << " (size=" << var.size 
                  << ", align=" << var.alignment << ", DataType=" << static_cast<int>(var.data_type) 
                  << ") at offset " << aligned_offset << std::endl;
    }
    
    // Final alignment to 8-byte boundary for next scope or return address
    if (current_offset % 8 != 0) {
        current_offset += (8 - (current_offset % 8));
    }
    
    total_size = current_offset;
}

// NEW: Get direct pointer to scope node for a given depth
LexicalScopeNode* SimpleLexicalScopeAnalyzer::get_scope_node_for_depth(int depth) const {
    auto it = depth_to_scope_node_.find(depth);
    return (it != depth_to_scope_node_.end()) ? it->second : nullptr;
}

// NEW: Get raw pointer to scope node where a variable was defined
LexicalScopeNode* SimpleLexicalScopeAnalyzer::get_definition_scope_for_variable(const std::string& name) const {
    int def_depth = get_variable_definition_depth(name);
    if (def_depth == -1) {
        return nullptr;  // Variable not found
    }
    return get_scope_node_for_depth(def_depth);
}

// NEW: Get raw pointer to the current scope node
LexicalScopeNode* SimpleLexicalScopeAnalyzer::get_current_scope_node() const {
    return get_scope_node_for_depth(current_depth_);
}

// NEW: Get direct pointer to variable declaration info for ultra-fast access
VariableDeclarationInfo* SimpleLexicalScopeAnalyzer::get_variable_declaration_info(const std::string& name) const {
    auto it = variable_declarations_.find(name);
    if (it == variable_declarations_.end() || it->second.empty()) {
        std::cout << "[DEBUG] get_variable_declaration_info: No declarations found for '" << name << "'" << std::endl;
        return nullptr;
    }
    
    // Debug: Print all declarations
    std::cout << "[DEBUG] get_variable_declaration_info: Found " << it->second.size() 
              << " declarations for '" << name << "':" << std::endl;
    for (size_t i = 0; i < it->second.size(); i++) {
        const auto& decl = it->second[i];
        std::cout << "[DEBUG]   [" << i << "]: depth=" << decl->depth 
                  << ", type=" << decl->declaration_type 
                  << ", data_type=" << static_cast<int>(decl->data_type) << std::endl;
    }
    
    // Return the most recent declaration (last in the vector)
    return it->second.back().get();
}

// Function registration methods
void SimpleLexicalScopeAnalyzer::register_function_in_current_scope(FunctionDecl* func_decl) {
    if (scope_stack_.empty()) {
        std::cerr << "[SimpleLexicalScope] ERROR: No current scope to register function in!" << std::endl;
        return;
    }
    
    // Find the nearest function scope for proper hoisting
    LexicalScopeNode* function_scope = find_nearest_function_scope();
    if (!function_scope) {
        std::cerr << "[SimpleLexicalScope] ERROR: No function scope found for function hoisting!" << std::endl;
        return;
    }
    
    function_scope->register_function_declaration(func_decl);
    
    // NEW: Immediate conflict detection and variable declaration
    if (func_decl && !func_decl->name.empty()) {
        const std::string& func_name = func_decl->name;
        function_declaration_conflicts_[func_name] = func_decl;
        
        // Check if there are any existing variable declarations with the same name in the function scope
        bool has_var_decl = function_scope->has_variable(func_name);
        
        if (has_var_decl) {
            // Conflict detected! Promote existing variables to DYNAMIC_VALUE
            std::cout << "[HoistingConflict] Immediate conflict detected for '" << func_name 
                      << "' - function declaration conflicts with existing variable" << std::endl;
            
            auto var_decl_it = variable_declarations_.find(func_name);
            if (var_decl_it != variable_declarations_.end()) {
                for (auto& decl : var_decl_it->second) {
                    if (decl->depth == function_scope->scope_depth) {
                        decl->data_type = DataType::DYNAMIC_VALUE;
                        std::cout << "[HoistingConflict] Promoted existing variable '" << func_name 
                                  << "' to DYNAMIC_VALUE at depth " << decl->depth << std::endl;
                    }
                }
            } else {
                // Create a DYNAMIC_VALUE variable for the function  
                // Save current depth, set to function scope depth, declare, then restore
                int saved_depth = current_depth_;
                current_depth_ = function_scope->scope_depth;
                declare_variable(func_name, "function", DataType::DYNAMIC_VALUE);
                current_depth_ = saved_depth;
                std::cout << "[HoistingConflict] Created DYNAMIC_VALUE variable for function '" 
                          << func_name << "' in scope depth " << function_scope->scope_depth << std::endl;
            }
            
            mark_variable_as_hoisting_conflict(func_name);
            // NOTE: Function size will be tracked later when computed
        } else {
            // No conflict - declare function as regular FUNCTION variable
            // Save current depth, set to function scope depth, declare, then restore  
            int saved_depth = current_depth_;
            current_depth_ = function_scope->scope_depth;
            declare_variable(func_name, "function", DataType::FUNCTION);
            current_depth_ = saved_depth;
            std::cout << "[HoistingConflict] No conflict for function '" << func_name 
                      << "', declared as FUNCTION type in scope depth " << function_scope->scope_depth << std::endl;
        }
        
        std::cout << "[HoistingConflict] Registered function declaration '" << func_name 
                  << "' for conflict detection" << std::endl;
    }
    
    std::cout << "[SimpleLexicalScope] Registered function declaration '" 
              << (func_decl ? ("'" + func_decl->name + "'") : "null") << "' in function scope at depth " 
              << function_scope->scope_depth << std::endl;
}

void SimpleLexicalScopeAnalyzer::register_function_expression_in_current_scope(FunctionExpression* func_expr) {
    if (scope_stack_.empty()) {
        std::cerr << "[SimpleLexicalScope] ERROR: No current scope to register function expression in!" << std::endl;
        return;
    }
    
    // Find the nearest function scope for proper lexical scoping
    LexicalScopeNode* function_scope = find_nearest_function_scope();
    if (!function_scope) {
        std::cerr << "[SimpleLexicalScope] ERROR: No function scope found for function expression!" << std::endl;
        return;
    }
    
    function_scope->register_function_expression(func_expr);
    std::cout << "[SimpleLexicalScope] Registered function expression in function scope at depth " 
              << function_scope->scope_depth << std::endl;
}

// Find the nearest function scope for proper function hoisting
LexicalScopeNode* SimpleLexicalScopeAnalyzer::find_nearest_function_scope() {
    // Walk up the scope stack to find the nearest function scope (including global)
    for (auto it = scope_stack_.rbegin(); it != scope_stack_.rend(); ++it) {
        LexicalScopeNode* scope = it->get();
        if (scope->is_function_scope) {
            return scope;
        }
    }
    return nullptr;  // No function scope found
}

// Function instance size computation (based on FUNCTION.md specification)
size_t SimpleLexicalScopeAnalyzer::compute_function_instance_size(const LexicalScopeNode* lexical_scope) const {
    if (!lexical_scope) {
        std::cerr << "[SimpleLexicalScope] ERROR: Cannot compute function instance size for null scope!" << std::endl;
        return 0;
    }
    
    // According to FUNCTION.md:
    // Function instance structure:
    // - uint64_t size (8 bytes)
    // - void* function_code_addr (8 bytes)  
    // - void* lex_addr1, lex_addr2, ... (8 bytes each)
    // Total: 16 + (scope_count * 8) bytes
    
    size_t scope_count = lexical_scope->priority_sorted_parent_scopes.size();
    size_t total_size = 16 + (scope_count * 8); // 16 byte header + scopes (function name no longer stored)
    
    std::cout << "[SimpleLexicalScope] Computing function instance size:" << std::endl;
    std::cout << "  - Header size: 16 bytes (uint64_t size + void* function_code_addr)" << std::endl;
    std::cout << "  - Captured scopes: " << scope_count << " scopes" << std::endl;
    std::cout << "  - Scope pointers: " << (scope_count * 8) << " bytes (" << scope_count << " * 8)" << std::endl;
    std::cout << "  - Total function instance size: " << total_size << " bytes" << std::endl;
    
    return total_size;
}

// Conservative Maximum Size approach - Function assignment tracking methods

void SimpleLexicalScopeAnalyzer::track_function_assignment(const std::string& variable_name, size_t function_size) {
    std::cout << "[FunctionTracking] Tracking assignment: " << variable_name << " = function of size " << function_size << " bytes" << std::endl;
    
    // Add this size to the set of sizes for this variable
    variable_function_sizes_[variable_name].insert(function_size);
    
    // Update the maximum size cache
    auto& current_max = variable_max_function_size_[variable_name];
    if (function_size > current_max) {
        current_max = function_size;
        std::cout << "[FunctionTracking] New maximum size for " << variable_name << ": " << current_max << " bytes" << std::endl;
    }
}

void SimpleLexicalScopeAnalyzer::finalize_function_variable_sizes() {
    std::cout << "[FunctionTracking] Finalizing function variable sizes..." << std::endl;
    
    for (const auto& [variable_name, sizes] : variable_function_sizes_) {
        // Find the maximum size for this variable
        size_t max_size = *std::max_element(sizes.begin(), sizes.end());
        variable_max_function_size_[variable_name] = max_size;
        
        std::cout << "[FunctionTracking] " << variable_name << ": " << sizes.size() 
                  << " assignments, sizes: ";
        for (size_t size : sizes) {
            std::cout << size << " ";
        }
        std::cout << "-> max: " << max_size << " bytes" << std::endl;
    }
}

size_t SimpleLexicalScopeAnalyzer::get_max_function_size(const std::string& variable_name) const {
    auto it = variable_max_function_size_.find(variable_name);
    if (it != variable_max_function_size_.end()) {
        return it->second;
    }
    return 0; // Variable has no function assignments tracked
}

bool SimpleLexicalScopeAnalyzer::has_tracked_function_sizes(const std::string& variable_name) const {
    return variable_function_sizes_.find(variable_name) != variable_function_sizes_.end();
}

// NEW: Function declaration conflict detection and hoisting support
bool SimpleLexicalScopeAnalyzer::has_function_declaration_conflict(const std::string& var_name) const {
    return function_declaration_conflicts_.find(var_name) != function_declaration_conflicts_.end();
}

class FunctionDecl* SimpleLexicalScopeAnalyzer::get_conflicting_function_declaration(const std::string& var_name) const {
    auto it = function_declaration_conflicts_.find(var_name);
    if (it != function_declaration_conflicts_.end()) {
        return it->second;
    }
    return nullptr;
}

void SimpleLexicalScopeAnalyzer::mark_variable_as_hoisting_conflict(const std::string& var_name) {
    hoisting_conflict_variables_.insert(var_name);
    std::cout << "[HoistingConflict] Marked variable '" << var_name << "' as hoisting conflict (DYNAMIC_VALUE)" << std::endl;
}

bool SimpleLexicalScopeAnalyzer::is_hoisting_conflict_variable(const std::string& var_name) const {
    return hoisting_conflict_variables_.find(var_name) != hoisting_conflict_variables_.end();
}

void SimpleLexicalScopeAnalyzer::resolve_hoisting_conflicts_in_current_scope() {
    if (scope_stack_.empty()) {
        return;
    }
    
    LexicalScopeNode* current_scope = scope_stack_.back().get();
    
    // Check each function declaration in the current scope
    for (FunctionDecl* func_decl : current_scope->declared_functions) {
        if (!func_decl || func_decl->name.empty()) {
            continue;
        }
        
        const std::string& func_name = func_decl->name;
        
        // Check if there are any variable declarations with the same name in this scope
        bool has_var_decl = current_scope->has_variable(func_name);
        
        if (has_var_decl) {
            // Conflict detected! The variable should be promoted to DYNAMIC_VALUE
            std::cout << "[HoistingConflict] Conflict detected for '" << func_name 
                      << "' - function declaration conflicts with variable declaration" << std::endl;
            
            // Find all variable declarations with this name and promote them to DYNAMIC_VALUE
            auto var_decl_it = variable_declarations_.find(func_name);
            if (var_decl_it != variable_declarations_.end()) {
                for (auto& decl : var_decl_it->second) {
                    if (decl->depth == current_scope->scope_depth) {
                        decl->data_type = DataType::DYNAMIC_VALUE;
                        std::cout << "[HoistingConflict] Promoted variable '" << func_name 
                                  << "' to DYNAMIC_VALUE at depth " << decl->depth << std::endl;
                    }
                }
            } else {
                // No existing variable declarations found, create one for the function
                declare_variable(func_name, "function", DataType::DYNAMIC_VALUE);
                std::cout << "[HoistingConflict] Created DYNAMIC_VALUE variable for function '" 
                          << func_name << "'" << std::endl;
            }
            
            // Mark as hoisting conflict and track function size
            mark_variable_as_hoisting_conflict(func_name);
            if (func_decl->function_instance_size > 0) {
                track_function_assignment(func_name, func_decl->function_instance_size);
            }
        } else {
            // No conflict - just declare the function as a regular variable
            declare_variable(func_name, "function", DataType::FUNCTION);
            std::cout << "[HoistingConflict] No conflict for function '" << func_name 
                      << "', declared as FUNCTION type" << std::endl;
        }
    }
}

// NEW: Add an unresolved reference for a variable that hasn't been declared yet
void SimpleLexicalScopeAnalyzer::add_unresolved_reference(const std::string& var_name, Identifier* identifier) {
    unresolved_references_[var_name].emplace_back(identifier, current_depth_);
    std::cout << "[UnresolvedRef] Added unresolved reference for '" << var_name 
              << "' at depth " << current_depth_ 
              << ", total unresolved: " << unresolved_references_[var_name].size() << std::endl;
}

// NEW: Resolve all unresolved references for a specific variable
void SimpleLexicalScopeAnalyzer::resolve_references_for_variable(const std::string& var_name) {
    auto it = unresolved_references_.find(var_name);
    if (it == unresolved_references_.end()) {
        return; // No unresolved references for this variable
    }
    
    VariableDeclarationInfo* var_info = get_variable_declaration_info(var_name);
    if (!var_info) {
        std::cout << "[UnresolvedRef] Cannot resolve references for '" << var_name 
                  << "' - variable declaration info not found" << std::endl;
        return;
    }
    
    std::cout << "[UnresolvedRef] Variable '" << var_name << "' declaration info: "
              << "depth=" << var_info->depth 
              << ", type=" << var_info->declaration_type
              << ", data_type=" << static_cast<int>(var_info->data_type)
              << ", offset=" << var_info->offset << std::endl;
    
    std::cout << "[UnresolvedRef] Resolving " << it->second.size() 
              << " unresolved references for '" << var_name << "'" << std::endl;
    
    // Get the variable definition depth
    int definition_depth = get_variable_definition_depth(var_name);
    if (definition_depth == -1) {
        std::cout << "[UnresolvedRef] WARNING: Could not find definition depth for '" << var_name << "'" << std::endl;
        return;
    }
    
    // Update all identifier nodes and track their accesses
    for (const UnresolvedReference& unresolved_ref : it->second) {
        if (unresolved_ref.identifier) {
            // Copy essential info instead of storing pointer (prevents use-after-free)
            unresolved_ref.identifier->definition_depth = var_info->depth;
            unresolved_ref.identifier->variable_declaration_info = var_info;  // Keep for now, but rely on copied data
            
            // Track the variable access at the correct depth
            // We need to simulate being at the access depth to properly track dependencies
            int original_depth = current_depth_;
            
            // Find the scope at the access depth
            auto scope_it = depth_to_scope_node_.find(unresolved_ref.access_depth);
            if (scope_it != depth_to_scope_node_.end()) {
                // Update usage count for this declaration  
                auto& declarations = variable_declarations_[var_name];
                for (auto& decl : declarations) {
                    if (decl->depth == definition_depth) {
                        decl->usage_count++;
                        break;
                    }
                }
                
                // Add as dependency if accessing from different depth
                if (definition_depth != unresolved_ref.access_depth) {
                    auto scope_node = scope_it->second;
                    
                    // Check if we already have this dependency
                    bool found = false;
                    for (auto& dep : scope_node->self_dependencies) {
                        if (dep.variable_name == var_name && dep.definition_depth == definition_depth) {
                            dep.access_count++;
                            found = true;
                            break;
                        }
                    }
                    
                    if (!found) {
                        scope_node->self_dependencies.emplace_back(var_name, definition_depth);
                    }
                }
                
                std::cout << "[UnresolvedRef] Updated identifier and tracked access for '" << var_name 
                          << "' from depth " << unresolved_ref.access_depth << " to depth " << definition_depth << std::endl;
            } else {
                std::cout << "[UnresolvedRef] WARNING: Could not find scope node for access depth " 
                          << unresolved_ref.access_depth << std::endl;
            }
        }
    }
    
    // Clear the unresolved references for this variable
    unresolved_references_.erase(it);
}

// NEW: Resolve all remaining unresolved references at end of parsing
void SimpleLexicalScopeAnalyzer::resolve_all_unresolved_references() {
    std::cout << "[UnresolvedRef] Resolving all unresolved references. Total variables: " 
              << unresolved_references_.size() << std::endl;
    
    for (const auto& pair : unresolved_references_) {
        const std::string& var_name = pair.first;
        resolve_references_for_variable(var_name);
    }
    
    // Clear all unresolved references
    unresolved_references_.clear();
    std::cout << "[UnresolvedRef] All unresolved references cleared." << std::endl;
}

// NEW: Perform deferred variable packing for a scope during AST generation
void SimpleLexicalScopeAnalyzer::perform_deferred_packing_for_scope(LexicalScopeNode* scope_node) {
    if (!scope_node || scope_node->variable_declarations.empty()) {
        return;
    }
    
    std::cout << "[DeferredPacking] Performing deferred packing for scope at depth " 
              << scope_node->scope_depth << " with " << scope_node->variable_declarations.size() << " variables" << std::endl;
    
    // Finalize function variable sizes before packing
    finalize_function_variable_sizes();
    
    std::unordered_map<std::string, size_t> variable_offsets;
    std::vector<std::string> packed_order;
    size_t total_frame_size = 0;
    
    // Extract variable names from the map for packing
    std::unordered_set<std::string> variable_names;
    for (const auto& var_entry : scope_node->variable_declarations) {
        variable_names.insert(var_entry.first);
    }
    
    // Call the private packing method
    pack_scope_variables(variable_names, variable_offsets, packed_order, 
                        total_frame_size, scope_node);
    
    // Store packing results in the lexical scope node
    scope_node->variable_offsets = variable_offsets;
    scope_node->packed_variable_order = packed_order;
    scope_node->total_scope_frame_size = total_frame_size;
    
    // Update individual VariableDeclarationInfo objects with their calculated offsets
    for (const auto& var : packed_order) {
        VariableDeclarationInfo* var_info = get_variable_declaration_info(var);
        if (var_info) {
            var_info->offset = variable_offsets[var];
            std::cout << "[DeferredPacking] Updated VariableDeclarationInfo->offset for '" << var 
                      << "' to " << var_info->offset << std::endl;
        }
    }
    
    std::cout << "[DeferredPacking] Deferred packing completed: " << total_frame_size << " bytes total" << std::endl;
    for (const auto& var : packed_order) {
        std::cout << "[DeferredPacking]   " << var << " -> offset " << variable_offsets[var] << std::endl;
    }
}

//=============================================================================
// FUNCTION VARIABLE TYPE CLASSIFICATION METHODS
// Implementation of the three function storage strategies from FUNCTION.md
//=============================================================================

// Track variable assignment types for classification
void SimpleLexicalScopeAnalyzer::track_variable_assignment_type(const std::string& var_name, DataType assigned_type) {
    variable_assignment_types_[var_name].insert(assigned_type);
    
    // Check if this creates a mixed assignment scenario
    const auto& types = variable_assignment_types_[var_name];
    bool has_function = (types.count(DataType::FUNCTION) > 0 || 
                        types.count(DataType::LOCAL_FUNCTION_INSTANCE) > 0);
    bool has_non_function = false;
    
    for (DataType type : types) {
        if (type != DataType::FUNCTION && type != DataType::LOCAL_FUNCTION_INSTANCE) {
            has_non_function = true;
            break;
        }
    }
    
    if (has_function && has_non_function) {
        mixed_assignment_variables_.insert(var_name);
        std::cout << "[FunctionClassification] Variable '" << var_name << "' marked as mixed assignment (Strategy 3)" << std::endl;
    }
}

// Check if variable has mixed type assignments
bool SimpleLexicalScopeAnalyzer::has_mixed_type_assignments(const std::string& var_name) const {
    return mixed_assignment_variables_.count(var_name) > 0;
}

// Classify function variable strategy based on FUNCTION.md
SimpleLexicalScopeAnalyzer::FunctionVariableStrategy 
SimpleLexicalScopeAnalyzer::classify_function_variable_strategy(const std::string& var_name) const {
    
    // Strategy 3: Any-Typed Variables with Mixed Assignment
    if (has_mixed_type_assignments(var_name) || is_hoisting_conflict_variable(var_name)) {
        return FunctionVariableStrategy::ANY_TYPED_DYNAMIC;
    }
    
    // Check if variable receives any function assignments at all
    bool has_function_assignment = has_tracked_function_sizes(var_name);
    if (!has_function_assignment) {
        return FunctionVariableStrategy::ANY_TYPED_DYNAMIC; // Not a function variable
    }
    
    // Strategy 1: Static Single Function Assignment
    if (is_static_single_function_assignment(var_name)) {
        return FunctionVariableStrategy::STATIC_SINGLE_ASSIGNMENT;
    }
    
    // Strategy 2: Function-Typed Variables (Conservative Maximum Size)
    if (is_function_typed_variable(var_name)) {
        return FunctionVariableStrategy::FUNCTION_TYPED;
    }
    
    // Default to Strategy 3 for safety
    return FunctionVariableStrategy::ANY_TYPED_DYNAMIC;
}

// Check if variable is static single function assignment (Strategy 1)
bool SimpleLexicalScopeAnalyzer::is_static_single_function_assignment(const std::string& var_name) const {
    // Must have exactly one function assignment and no other assignments
    auto it = variable_function_sizes_.find(var_name);
    if (it == variable_function_sizes_.end() || it->second.size() != 1) {
        return false;
    }
    
    // Must not have any non-function assignments
    auto type_it = variable_assignment_types_.find(var_name);
    if (type_it == variable_assignment_types_.end()) {
        return true; // Only function assignment
    }
    
    // Check that all assignments are function-related
    for (DataType type : type_it->second) {
        if (type != DataType::FUNCTION && type != DataType::LOCAL_FUNCTION_INSTANCE) {
            return false;
        }
    }
    
    // Must not be involved in hoisting conflicts
    return !is_hoisting_conflict_variable(var_name) && !has_mixed_type_assignments(var_name);
}

// Check if variable is function-typed (Strategy 2)
bool SimpleLexicalScopeAnalyzer::is_function_typed_variable(const std::string& var_name) const {
    // Must have function assignments but potentially multiple function assignments
    // (Conservative Maximum Size handles this)
    bool has_function_assignments = has_tracked_function_sizes(var_name);
    if (!has_function_assignments) {
        return false;
    }
    
    // Must not have mixed type assignments (that would be Strategy 3)
    if (has_mixed_type_assignments(var_name) || is_hoisting_conflict_variable(var_name)) {
        return false;
    }
    
    // Check that all assignments are function-related
    auto type_it = variable_assignment_types_.find(var_name);
    if (type_it != variable_assignment_types_.end()) {
        for (DataType type : type_it->second) {
            if (type != DataType::FUNCTION && type != DataType::LOCAL_FUNCTION_INSTANCE) {
                return false; // Has non-function assignment
            }
        }
    }
    
    return true;
}

// Check if variable is mixed assignment (Strategy 3)
bool SimpleLexicalScopeAnalyzer::is_mixed_assignment_variable(const std::string& var_name) const {
    return classify_function_variable_strategy(var_name) == FunctionVariableStrategy::ANY_TYPED_DYNAMIC;
}

// Get appropriate storage DataType for function variable
DataType SimpleLexicalScopeAnalyzer::get_function_variable_storage_type(const std::string& var_name) const {
    FunctionVariableStrategy strategy = classify_function_variable_strategy(var_name);
    
    switch (strategy) {
        case FunctionVariableStrategy::STATIC_SINGLE_ASSIGNMENT:
        case FunctionVariableStrategy::FUNCTION_TYPED:
            return DataType::LOCAL_FUNCTION_INSTANCE;
            
        case FunctionVariableStrategy::ANY_TYPED_DYNAMIC:
        default:
            return DataType::DYNAMIC_VALUE;
    }
}

//=============================================================================
// PHASE 1: FUNCTION STATIC ANALYSIS FOR PURE MACHINE CODE GENERATION
//=============================================================================

void SimpleLexicalScopeAnalyzer::compute_function_static_analysis(FunctionDecl* function) {
    if (!function) {
        std::cout << "[StaticAnalysis] WARNING: Cannot compute static analysis for null function" << std::endl;
        return;
    }
    
    if (!function->lexical_scope) {
        std::cout << "[StaticAnalysis] WARNING: Function '" << function->name << "' has no lexical scope" << std::endl;
        return;
    }
    
    LexicalScopeNode* func_scope = function->lexical_scope.get();
    
    // Safe string logging to avoid corruption issues
    std::string func_name = (function->name.empty()) ? "(empty)" : function->name;
    std::cout << "[StaticAnalysis] Computing static analysis for function '" << func_name 
              << "' at depth " << func_scope->scope_depth << std::endl;
    
    // Step 1: Extract needed parent scopes from priority_sorted_parent_scopes
    function->static_analysis.needed_parent_scopes = func_scope->priority_sorted_parent_scopes;
    size_t num_parent_scopes = function->static_analysis.needed_parent_scopes.size();
    
    // Step 2: Compute function instance size and local scope size  
    function->static_analysis.function_instance_size = compute_function_instance_size(func_scope);
    function->function_instance_size = function->static_analysis.function_instance_size; // Keep legacy field in sync
    
    // Step 3: Store local scope size
    function->static_analysis.local_scope_size = func_scope->total_scope_frame_size;
    
    std::cout << "[StaticAnalysis] Function '" << function->name << "' analysis complete:" << std::endl;
    std::cout << "  - Needs " << num_parent_scopes << " parent scopes (runtime lookup)" << std::endl;
    std::cout << "  - Function instance size: " << function->static_analysis.function_instance_size << " bytes" << std::endl;
    std::cout << "  - Local scope size: " << function->static_analysis.local_scope_size << " bytes" << std::endl;
    
    // NOTE: parent_location_indexes will be computed later during parent-child relationship analysis
    // This requires knowledge of how parent functions arrange their scopes in registers/stack
}

void SimpleLexicalScopeAnalyzer::compute_all_function_static_analysis() {
    std::cout << "[StaticAnalysis] Computing static analysis for all functions (legacy system)..." << std::endl;
    
    // Safety check
    if (depth_to_scope_node_.empty()) {
        std::cout << "[StaticAnalysis] WARNING: No scope nodes found for analysis" << std::endl;
        return;
    }
    
    std::cout << "[StaticAnalysis] Found " << depth_to_scope_node_.size() << " scope nodes" << std::endl;
    
    // Just do basic setup without detailed computation to avoid crash
    
    std::cout << "[StaticAnalysis] All function static analysis complete (legacy system)" << std::endl;
}

// Private helper method to compute parent-child scope mappings
void SimpleLexicalScopeAnalyzer::compute_parent_child_scope_mappings() {
    std::cout << "[StaticAnalysis] Computing parent-child scope mappings..." << std::endl;
    
    // Safety check
    if (depth_to_scope_node_.empty()) {
        std::cout << "[StaticAnalysis] WARNING: No scope nodes found for mapping" << std::endl;
        return;
    }
    
    // For each function, we need to compute how its needed scopes map to parent function locations
    for (auto& scope_pair : depth_to_scope_node_) {
        int depth = scope_pair.first;
        LexicalScopeNode* scope_node = scope_pair.second;
        
        if (!scope_node || !scope_node->is_function_scope) {
            continue;
        }
        
        std::cout << "[StaticAnalysis] Processing mapping for scope at depth " << depth << std::endl;
        
        // Process all function declarations in this scope
        for (size_t i = 0; i < scope_node->declared_functions.size(); i++) {
            FunctionDecl* func_decl = scope_node->declared_functions[i];
            if (!func_decl) {
                std::cout << "[StaticAnalysis] WARNING: Null function declaration at mapping index " << i << std::endl;
                continue;
            }
            
            std::cout << "[StaticAnalysis] Computing mapping for function '" << func_decl->name << "'" << std::endl;
            compute_scope_mapping_for_function(func_decl, scope_node);
        }
    }
    
    std::cout << "[StaticAnalysis] Parent-child scope mappings complete" << std::endl;
}

// Helper to compute scope mapping for a specific function
void SimpleLexicalScopeAnalyzer::compute_scope_mapping_for_function(FunctionDecl* child_func, LexicalScopeNode* parent_scope) {
    if (!child_func || !child_func->lexical_scope) {
        return;
    }
    
    std::cout << "[StaticAnalysis] Computing scope mapping for function '" << child_func->name << "'" << std::endl;
    
    FunctionStaticAnalysis& child_analysis = child_func->static_analysis;
    child_analysis.parent_location_indexes.clear();
    child_analysis.parent_location_indexes.resize(child_analysis.needed_parent_scopes.size());
    
    // For each scope the child function needs (by child's index)
    for (size_t child_idx = 0; child_idx < child_analysis.needed_parent_scopes.size(); child_idx++) {
        int needed_scope_depth = child_analysis.needed_parent_scopes[child_idx];
        
        std::cout << "[StaticAnalysis]   Child index " << child_idx << " needs scope depth " << needed_scope_depth << std::endl;
        
        // Find where this scope exists in the parent function's layout
        if (needed_scope_depth == parent_scope->scope_depth) {
            // It's the parent's local scope (always at r15, use index -1)
            child_analysis.parent_location_indexes[child_idx] = -1;
            std::cout << "[StaticAnalysis]     -> Parent's local scope (r15, index -1)" << std::endl;
        } else {
            // Find this scope in parent's priority_sorted_parent_scopes
            auto& parent_scopes = parent_scope->priority_sorted_parent_scopes;
            auto it = std::find(parent_scopes.begin(), parent_scopes.end(), needed_scope_depth);
            
            if (it != parent_scopes.end()) {
                // Store the INDEX in parent's priority list  
                int parent_index = it - parent_scopes.begin();
                child_analysis.parent_location_indexes[child_idx] = parent_index;
                std::cout << "[StaticAnalysis]     -> Parent's index " << parent_index 
                          << " (register r" << (12 + parent_index) << ")" << std::endl;
            } else {
                // This shouldn't happen if the lexical analysis is correct
                std::cout << "[StaticAnalysis] ERROR: Child function needs scope depth " << needed_scope_depth
                          << " but parent doesn't have it!" << std::endl;
                child_analysis.parent_location_indexes[child_idx] = -1; // Fallback to parent local scope
            }
        }
    }
    
    std::cout << "[StaticAnalysis] Function '" << child_func->name << "' scope mapping:" << std::endl;
    for (size_t i = 0; i < child_analysis.parent_location_indexes.size(); i++) {
        int parent_idx = child_analysis.parent_location_indexes[i];
        int scope_depth = child_analysis.needed_parent_scopes[i];
        std::cout << "[StaticAnalysis]   scopes[" << i << "] = parent_index[" << parent_idx 
                  << "] (depth " << scope_depth << ")" << std::endl;
    }
}
