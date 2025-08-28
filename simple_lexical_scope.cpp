#include "simple_lexical_scope.h"
#include "compiler.h"  // For LexicalScopeNode and DataType enum
#include <algorithm>
#include <iostream>
#include <set>
#include <unordered_map>

// Called when entering a new lexical scope (function, block, etc.)
void SimpleLexicalScopeAnalyzer::enter_scope() {
    std::cout << "[SimpleLexicalScope] ENTER_SCOPE CALL: current_depth_ before increment: " << current_depth_ << std::endl;
    std::cout << "[SimpleLexicalScope] ENTER_SCOPE CALL: scope_stack_.size() = " << scope_stack_.size() << std::endl;
    current_depth_++;
    
    // NEW: Create LexicalScopeNode immediately so pointers are always available
    auto lexical_scope_node = std::make_shared<LexicalScopeNode>(current_depth_);
    
    // Register the scope node for direct access right away
    LexicalScopeNode* scope_node_ptr = lexical_scope_node.get();
    depth_to_scope_node_[current_depth_] = scope_node_ptr;
    
    // Add to scope stack for processing during parsing
    scope_stack_.push_back(lexical_scope_node);
    
    std::cout << "[SimpleLexicalScope] Entered scope at depth " << current_depth_ 
              << " (pointer immediately available: " << scope_node_ptr << ")" << std::endl;
}

// Called when exiting a lexical scope - returns LexicalScopeNode with all scope info
std::unique_ptr<LexicalScopeNode> SimpleLexicalScopeAnalyzer::exit_scope() {
    if (scope_stack_.empty()) {
        std::cerr << "[SimpleLexicalScope] ERROR: Attempting to exit scope when none exists!" << std::endl;
        return nullptr;
    }
    
    // Get the current LexicalScopeNode (already created when entering scope)
    std::shared_ptr<LexicalScopeNode> current_scope_node = scope_stack_.back();
    scope_stack_.pop_back();
    
    std::cout << "[SimpleLexicalScope] Exiting scope at depth " << current_depth_ 
              << " with " << current_scope_node->declared_variables.size() << " declared variables" << std::endl;
    
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
    
    // NEW: Perform optimal variable packing for this scope
    std::unordered_map<std::string, size_t> variable_offsets;
    std::vector<std::string> packed_order;
    size_t total_frame_size = 0;
    
    pack_scope_variables(current_scope_node->declared_variables, variable_offsets, packed_order, total_frame_size);
    
    // Store packing results in the lexical scope node
    current_scope_node->variable_offsets = variable_offsets;
    current_scope_node->packed_variable_order = packed_order;
    current_scope_node->total_scope_frame_size = total_frame_size;
    
    std::cout << "[SimpleLexicalScope] Variable packing completed: " << total_frame_size << " bytes total" << std::endl;
    for (const auto& var : packed_order) {
        std::cout << "[SimpleLexicalScope]   " << var << " -> offset " << variable_offsets[var] << std::endl;
    }
    
    std::cout << "[SimpleLexicalScope] Priority-sorted parent scopes: ";
    for (int depth : current_scope_node->priority_sorted_parent_scopes) {
        std::cout << depth << " ";
    }
    std::cout << std::endl;
    
    // NOTE: Scope node was already registered in depth_to_scope_node_ when entering scope
    std::cout << "[SimpleLexicalScope] Scope node at depth " << current_scope_node->scope_depth 
              << " remains registered for direct access (pointer: " << current_scope_node.get() << ")" << std::endl;

    // Clean up variable declarations at this depth
    // IMPORTANT: Don't clean up global scope (depth 2) declarations as they're needed for code generation
    if (current_depth_ != 2) {
        cleanup_declarations_at_depth(current_depth_);
    } else {
        std::cout << "[SimpleLexicalScope] Preserving global scope declarations for code generation" << std::endl;
        // For global scope, keep the shared_ptr in completed_scopes_ to ensure it stays alive
        // The parser won't store the global scope anywhere, so we need to keep it alive ourselves
        completed_scopes_.push_back(current_scope_node);
        std::cout << "[SimpleLexicalScope] Stored global scope shared_ptr for code generation lifecycle" << std::endl;
    }

    // Decrement depth since we're exiting this scope
    current_depth_--;

    // Return a unique_ptr copy for AST storage
    auto result = std::make_unique<LexicalScopeNode>(*current_scope_node);
    return result;
}

// Called when a variable is declared (new version with DataType)
void SimpleLexicalScopeAnalyzer::declare_variable(const std::string& name, const std::string& declaration_type, DataType data_type) {
    // Add to the variable declarations map
    variable_declarations_[name].emplace_back(current_depth_, declaration_type, data_type);
    
    // Add to current scope's declared variables
    if (!scope_stack_.empty()) {
        scope_stack_.back()->declared_variables.insert(name);
    }
    
    std::cout << "[SimpleLexicalScope] Declared variable '" << name << "' as " << declaration_type 
              << " (DataType=" << static_cast<int>(data_type) << ") at depth " << current_depth_ << std::endl;
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
    
    std::cout << "[SimpleLexicalScope] Accessing variable '" << name 
              << "' defined at depth " << definition_depth 
              << " from depth " << current_depth_ << std::endl;
    
    // Update usage count for this declaration
    auto& declarations = variable_declarations_[name];
    for (auto& decl : declarations) {
        if (decl.depth == definition_depth) {
            decl.usage_count++;
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

// Get the absolute depth where a variable was last declared
int SimpleLexicalScopeAnalyzer::get_variable_definition_depth(const std::string& name) const {
    auto it = variable_declarations_.find(name);
    if (it == variable_declarations_.end() || it->second.empty()) {
        return -1; // Not found
    }
    
    // Return the depth of the most recent declaration (last in the vector)
    return it->second.back().depth;
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
            std::cout << "[depth=" << decl.depth << ", decl=" << decl.declaration_type 
                      << ", usage=" << decl.usage_count << "] ";
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
                [depth, &var_name](const VariableDeclarationInfo& decl) {
                    bool should_remove = (decl.depth == depth);
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
                                                      size_t& total_size) const {
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
            pack.data_type = decl.data_type;
            pack.size = get_datatype_size(decl.data_type);
            pack.alignment = get_datatype_alignment(decl.data_type);
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

// NEW: Get direct pointer to scope node where a variable was defined
LexicalScopeNode* SimpleLexicalScopeAnalyzer::get_definition_scope_for_variable(const std::string& name) const {
    int def_depth = get_variable_definition_depth(name);
    if (def_depth == -1) {
        return nullptr;  // Variable not found
    }
    return get_scope_node_for_depth(def_depth);
}

// NEW: Get direct pointer to the current scope node
LexicalScopeNode* SimpleLexicalScopeAnalyzer::get_current_scope_node() const {
    return get_scope_node_for_depth(current_depth_);
}
