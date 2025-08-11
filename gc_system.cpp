#include "gc_system.h"
#include "lexical_scope.h"
#include <algorithm>
#include <chrono>
#include <iostream>
#include <cassert>

namespace ultraScript {

// ============================================================================
// ESCAPE ANALYZER IMPLEMENTATION
// ============================================================================

EscapeAnalyzer& EscapeAnalyzer::instance() {
    static EscapeAnalyzer analyzer;
    return analyzer;
}

void EscapeAnalyzer::enter_scope(size_t scope_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    scope_stack_.push_back(scope_id);
}

void EscapeAnalyzer::exit_scope(size_t scope_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!scope_stack_.empty() && scope_stack_.back() == scope_id) {
        scope_stack_.pop_back();
    }
}

void EscapeAnalyzer::register_variable(size_t variable_id, const std::string& name, size_t scope_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    variable_names_[variable_id] = name;
    variable_scopes_[variable_id] = scope_id;
}

void EscapeAnalyzer::register_escape(size_t variable_id, EscapeType type, size_t escape_site) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto name_it = variable_names_.find(variable_id);
    auto scope_it = variable_scopes_.find(variable_id);
    
    if (name_it != variable_names_.end() && scope_it != variable_scopes_.end()) {
        escape_info_.emplace_back(variable_id, name_it->second, scope_it->second, type, escape_site);
        
        // Update requires_heap_alloc flag
        auto& info = escape_info_.back();
        info.requires_heap_alloc = (type != EscapeType::NONE);
        
        std::cout << "[GC] Variable '" << name_it->second << "' escapes via " 
                  << static_cast<int>(type) << " at site " << escape_site << std::endl;
    }
}

bool EscapeAnalyzer::does_variable_escape(size_t variable_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return std::any_of(escape_info_.begin(), escape_info_.end(),
                      [variable_id](const EscapeInfo& info) {
                          return info.variable_id == variable_id && info.escape_type != EscapeType::NONE;
                      });
}

void EscapeAnalyzer::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    scope_stack_.clear();
    variable_names_.clear();
    variable_scopes_.clear();
    escape_info_.clear();
}

// ============================================================================
// VARIABLE TRACKER IMPLEMENTATION
// ============================================================================

VariableTracker& VariableTracker::instance() {
    static VariableTracker tracker;
    return tracker;
}

size_t VariableTracker::enter_scope(const std::string& scope_name, bool is_function, bool is_loop) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    size_t scope_id = next_scope_id_++;
    size_t parent_id = current_scope_id_;
    
    ScopeInfo scope_info;
    scope_info.scope_id = scope_id;
    scope_info.parent_scope_id = parent_id;
    scope_info.is_function_scope = is_function;
    scope_info.is_loop_scope = is_loop;
    scope_info.scope_name = scope_name.empty() ? ("scope_" + std::to_string(scope_id)) : scope_name;
    
    scopes_[scope_id] = scope_info;
    
    // Update parent's children
    if (parent_id != 0) {
        scopes_[parent_id].child_scopes.push_back(scope_id);
    }
    
    scope_stack_.push_back(current_scope_id_);
    current_scope_id_ = scope_id;
    current_scope_variables_.clear();
    
    // Notify escape analyzer
    EscapeAnalyzer::instance().enter_scope(scope_id);
    
    std::cout << "[GC] Entered scope " << scope_id << " (" << scope_info.scope_name << ")" << std::endl;
    
    return scope_id;
}

void VariableTracker::exit_scope() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!scope_stack_.empty()) {
        size_t exiting_scope = current_scope_id_;
        current_scope_id_ = scope_stack_.back();
        scope_stack_.pop_back();
        
        // Restore parent scope's variables for quick lookup
        current_scope_variables_.clear();
        if (current_scope_id_ != 0) {
            const auto& parent_scope = scopes_[current_scope_id_];
            for (const auto& [name, var_id] : parent_scope.variables) {
                current_scope_variables_[name] = var_id;
            }
        }
        
        // Notify escape analyzer
        EscapeAnalyzer::instance().exit_scope(exiting_scope);
        
        std::cout << "[GC] Exited scope " << exiting_scope << ", returned to scope " << current_scope_id_ << std::endl;
    }
}

size_t VariableTracker::register_variable(const std::string& name, DataType type) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    size_t variable_id = next_variable_id_++;
    
    VariableInfo var_info(variable_id, name, current_scope_id_, type);
    variables_[variable_id] = var_info;
    
    // Add to current scope
    scopes_[current_scope_id_].variables[name] = variable_id;
    current_scope_variables_[name] = variable_id;
    
    // Notify escape analyzer
    EscapeAnalyzer::instance().register_variable(variable_id, name, current_scope_id_);
    
    std::cout << "[GC] Registered variable '" << name << "' (id=" << variable_id 
              << ") in scope " << current_scope_id_ << std::endl;
    
    return variable_id;
}

VariableInfo* VariableTracker::get_variable(size_t variable_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = variables_.find(variable_id);
    return (it != variables_.end()) ? &it->second : nullptr;
}

VariableInfo* VariableTracker::find_variable_in_scope(const std::string& name, size_t scope_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (scope_id == 0) {
        scope_id = current_scope_id_;
    }
    
    // Walk up the scope chain
    size_t current = scope_id;
    while (current != 0) {
        auto scope_it = scopes_.find(current);
        if (scope_it != scopes_.end()) {
            auto var_it = scope_it->second.variables.find(name);
            if (var_it != scope_it->second.variables.end()) {
                size_t var_id = var_it->second;
                auto var_info_it = variables_.find(var_id);
                if (var_info_it != variables_.end()) {
                    return &var_info_it->second;
                }
            }
            current = scope_it->second.parent_scope_id;
        } else {
            break;
        }
    }
    
    return nullptr;
}

void VariableTracker::mark_variable_escape(size_t variable_id, EscapeType escape_type) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = variables_.find(variable_id);
    if (it != variables_.end()) {
        it->second.escapes = true;
        it->second.escape_types.push_back(escape_type);
        
        // Notify escape analyzer
        EscapeAnalyzer::instance().register_escape(variable_id, escape_type, 0);
    }
}

void VariableTracker::mark_variable_escape(const std::string& name, EscapeType escape_type) {
    VariableInfo* var = find_variable_in_scope(name);
    if (var) {
        mark_variable_escape(var->variable_id, escape_type);
    }
}

const ScopeInfo* VariableTracker::get_scope(size_t scope_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = scopes_.find(scope_id);
    return (it != scopes_.end()) ? &it->second : nullptr;
}

std::vector<size_t> VariableTracker::get_variables_in_scope(size_t scope_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<size_t> result;
    auto it = scopes_.find(scope_id);
    if (it != scopes_.end()) {
        for (const auto& [name, var_id] : it->second.variables) {
            result.push_back(var_id);
        }
    }
    
    return result;
}

std::vector<size_t> VariableTracker::get_all_escaping_variables() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<size_t> result;
    for (const auto& [id, var] : variables_) {
        if (var.escapes) {
            result.push_back(id);
        }
    }
    
    return result;
}

void VariableTracker::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    next_scope_id_ = 1;
    next_variable_id_ = 1;
    current_scope_id_ = 0;
    scope_stack_.clear();
    scopes_.clear();
    variables_.clear();
    current_scope_variables_.clear();
    
    EscapeAnalyzer::instance().clear();
}

void VariableTracker::dump_scope_tree() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::cout << "\n=== SCOPE TREE ===" << std::endl;
    
    // Find root scopes (those with parent_scope_id == 0)
    for (const auto& [id, scope] : scopes_) {
        if (scope.parent_scope_id == 0) {
            dump_scope_recursive(id, 0);
        }
    }
}

void VariableTracker::dump_scope_recursive(size_t scope_id, int indent) const {
    auto it = scopes_.find(scope_id);
    if (it == scopes_.end()) return;
    
    const auto& scope = it->second;
    std::string indent_str(indent * 2, ' ');
    
    std::cout << indent_str << "Scope " << scope_id << " (" << scope.scope_name << ")" << std::endl;
    std::cout << indent_str << "  Variables: ";
    for (const auto& [name, var_id] : scope.variables) {
        auto var_it = variables_.find(var_id);
        if (var_it != variables_.end()) {
            std::cout << name;
            if (var_it->second.escapes) std::cout << "*";
            std::cout << " ";
        }
    }
    std::cout << std::endl;
    
    for (size_t child_id : scope.child_scopes) {
        dump_scope_recursive(child_id, indent + 1);
    }
}

void VariableTracker::dump_variables() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::cout << "\n=== VARIABLES ===" << std::endl;
    for (const auto& [id, var] : variables_) {
        std::cout << "Variable " << id << ": " << var.name 
                  << " (scope=" << var.scope_id 
                  << ", type=" << static_cast<int>(var.type)
                  << ", escapes=" << (var.escapes ? "YES" : "NO") << ")" << std::endl;
        
        if (var.escapes && !var.escape_types.empty()) {
            std::cout << "  Escape types: ";
            for (EscapeType type : var.escape_types) {
                std::cout << static_cast<int>(type) << " ";
            }
            std::cout << std::endl;
        }
    }
}

// ============================================================================
// GARBAGE COLLECTOR IMPLEMENTATION
// ============================================================================

GarbageCollector& GarbageCollector::instance() {
    static GarbageCollector collector;
    return collector;
}

GarbageCollector::GarbageCollector() {
    std::cout << "[GC] Initializing Garbage Collector" << std::endl;
    
    // Start background collector thread
    collector_thread_ = std::thread(&GarbageCollector::collector_thread_func, this);
}

GarbageCollector::~GarbageCollector() {
    shutdown();
}

void* GarbageCollector::gc_alloc(size_t size, uint32_t type_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    void* ptr = allocate_with_header(size, type_id);
    if (ptr) {
        heap_used_ += size + sizeof(GCObjectHeader);
        stats_.total_allocated += size;
        stats_.live_objects++;
        
        // Check if we should trigger collection
        if (should_collect()) {
            request_collection();
        }
    }
    
    return ptr;
}

void* GarbageCollector::gc_alloc_array(size_t element_size, size_t count, uint32_t type_id) {
    size_t total_size = element_size * count;
    void* ptr = gc_alloc(total_size, type_id);
    
    if (ptr) {
        GCObjectHeader* header = get_header(ptr);
        if (header) {
            header->flags |= GCObjectHeader::LARGE_OBJECT;
        }
    }
    
    return ptr;
}

void GarbageCollector::gc_free(void* ptr) {
    if (!ptr) return;
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = object_headers_.find(ptr);
    if (it != object_headers_.end()) {
        GCObjectHeader* header = it->second;
        size_t size = header->size;
        
        object_headers_.erase(it);
        heap_used_ -= size + sizeof(GCObjectHeader);
        stats_.total_freed += size;
        stats_.live_objects--;
        
        std::free(reinterpret_cast<char*>(ptr) - sizeof(GCObjectHeader));
    }
}

void GarbageCollector::add_root(void** root_ptr) {
    std::lock_guard<std::mutex> lock(mutex_);
    roots_.insert(root_ptr);
}

void GarbageCollector::remove_root(void** root_ptr) {
    std::lock_guard<std::mutex> lock(mutex_);
    roots_.erase(root_ptr);
}

void GarbageCollector::add_scope_roots(std::shared_ptr<LexicalScope> scope) {
    std::lock_guard<std::mutex> lock(mutex_);
    root_scopes_.push_back(scope);
}

void GarbageCollector::remove_scope_roots(std::shared_ptr<LexicalScope> scope) {
    std::lock_guard<std::mutex> lock(mutex_);
    root_scopes_.erase(
        std::remove(root_scopes_.begin(), root_scopes_.end(), scope),
        root_scopes_.end()
    );
}

void GarbageCollector::collect() {
    auto start_time = std::chrono::high_resolution_clock::now();
    
    std::cout << "[GC] Starting full collection..." << std::endl;
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    size_t objects_before = stats_.live_objects;
    size_t heap_used_before = heap_used_;
    
    // Mark phase
    mark_phase();
    
    // Sweep phase
    sweep_phase();
    
    // Defrag phase (optional, expensive)
    if (heap_used_ > heap_limit_ * 0.9) {
        defrag_phase();
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
    
    stats_.collections++;
    update_stats();
    
    size_t objects_freed = objects_before - stats_.live_objects;
    size_t bytes_freed = heap_used_before - heap_used_;
    
    std::cout << "[GC] Collection complete: freed " << objects_freed 
              << " objects (" << bytes_freed << " bytes) in " 
              << duration.count() / 1000.0 << "ms" << std::endl;
}

void GarbageCollector::collect_young() {
    // Simplified young generation collection
    // For now, just do a full collection
    collect();
    stats_.young_collections++;
}

void GarbageCollector::request_collection() {
    collection_requested_.store(true);
    collection_cv_.notify_one();
}

bool GarbageCollector::should_collect() const {
    return (static_cast<double>(heap_used_) / heap_limit_) > collection_threshold_;
}

void GarbageCollector::mark_phase() {
    std::cout << "[GC] Mark phase..." << std::endl;
    
    // Clear all mark bits
    for (auto& [ptr, header] : object_headers_) {
        header->unmark();
    }
    
    // Mark from roots
    mark_roots();
    
    // Process mark queue
    while (!mark_queue_.empty()) {
        void* obj = mark_queue_.front();
        mark_queue_.pop();
        mark_object(obj);
    }
}

void GarbageCollector::sweep_phase() {
    std::cout << "[GC] Sweep phase..." << std::endl;
    
    auto it = object_headers_.begin();
    while (it != object_headers_.end()) {
        void* ptr = it->first;
        GCObjectHeader* header = it->second;
        
        if (!header->is_marked()) {
            // Object is unreachable, free it
            size_t size = header->size;
            heap_used_ -= size + sizeof(GCObjectHeader);
            stats_.total_freed += size;
            stats_.live_objects--;
            
            std::free(reinterpret_cast<char*>(ptr) - sizeof(GCObjectHeader));
            it = object_headers_.erase(it);
        } else {
            // Object is still alive, unmark for next collection
            header->unmark();
            ++it;
        }
    }
}

void GarbageCollector::defrag_phase() {
    std::cout << "[GC] Defrag phase..." << std::endl;
    
    // Simple defragmentation: compact all live objects
    // This is a simplified version - real implementation would be more sophisticated
    
    std::vector<std::pair<void*, GCObjectHeader*>> live_objects;
    for (const auto& [ptr, header] : object_headers_) {
        live_objects.push_back({ptr, header});
    }
    
    // Sort by address to maintain relative order
    std::sort(live_objects.begin(), live_objects.end());
    
    // TODO: Implement actual memory compaction
    // This would involve:
    // 1. Allocating new contiguous memory block
    // 2. Copying all live objects to new locations
    // 3. Updating all pointers to point to new locations
    // 4. Freeing old fragmented memory
    
    stats_.defrag_operations++;
}

void GarbageCollector::mark_object(void* obj) {
    if (!obj) return;
    
    GCObjectHeader* header = get_header(obj);
    if (!header || header->is_marked()) {
        return;  // Already marked
    }
    
    header->mark();
    
    // TODO: Traverse object references and add to mark queue
    // This requires type information to know which fields are pointers
    // For now, we assume objects don't contain references to other GC objects
}

void GarbageCollector::mark_roots() {
    // Mark from explicit roots
    for (void** root : roots_) {
        if (*root) {
            mark_queue_.push(*root);
        }
    }
    
    // Mark from scope variables
    for (auto scope : root_scopes_) {
        mark_scope_variables(scope);
    }
}

void GarbageCollector::mark_scope_variables(std::shared_ptr<LexicalScope> scope) {
    if (!scope) return;
    
    // TODO: Traverse scope variables and mark any that contain GC objects
    // This requires integration with the lexical scope system
    
    // For now, just mark all variables that have escaped
    auto& tracker = VariableTracker::instance();
    auto escaping_vars = tracker.get_all_escaping_variables();
    
    for (size_t var_id : escaping_vars) {
        VariableInfo* var = tracker.get_variable(var_id);
        if (var && var->memory_location) {
            mark_queue_.push(var->memory_location);
        }
    }
}

GCObjectHeader* GarbageCollector::get_header(void* obj) {
    auto it = object_headers_.find(obj);
    return (it != object_headers_.end()) ? it->second : nullptr;
}

void* GarbageCollector::allocate_with_header(size_t size, uint32_t type_id) {
    size_t total_size = sizeof(GCObjectHeader) + size;
    void* raw_memory = std::malloc(total_size);
    
    if (!raw_memory) {
        return nullptr;
    }
    
    // Initialize header
    GCObjectHeader* header = static_cast<GCObjectHeader*>(raw_memory);
    header->size = static_cast<uint32_t>(size);
    header->type_id = type_id;
    header->flags = 0;
    header->generation = 0;  // Start in young generation
    header->ref_count = 0;
    
    // Object data starts after header
    void* obj_ptr = reinterpret_cast<char*>(raw_memory) + sizeof(GCObjectHeader);
    
    // Register object
    object_headers_[obj_ptr] = header;
    heap_blocks_.push_back(raw_memory);
    
    return obj_ptr;
}

void GarbageCollector::update_stats() {
    // Update average collection time and other stats
    // Implementation would track timing history
}

void GarbageCollector::collector_thread_func() {
    while (running_.load()) {
        std::unique_lock<std::mutex> lock(mutex_);
        
        collection_cv_.wait(lock, [this] {
            return collection_requested_.load() || !running_.load();
        });
        
        if (!running_.load()) {
            break;
        }
        
        if (collection_requested_.exchange(false)) {
            lock.unlock();
            
            if (generational_gc_enabled_ && stats_.young_collections < 5) {
                collect_young();
            } else {
                collect();
            }
        }
    }
}

void GarbageCollector::shutdown() {
    running_.store(false);
    collection_cv_.notify_all();
    
    if (collector_thread_.joinable()) {
        collector_thread_.join();
    }
    
    // Free all remaining objects
    for (void* block : heap_blocks_) {
        std::free(block);
    }
    
    heap_blocks_.clear();
    object_headers_.clear();
    roots_.clear();
    root_scopes_.clear();
    
    std::cout << "[GC] Shutdown complete" << std::endl;
}

// ============================================================================
// PARSER INTEGRATION IMPLEMENTATION
// ============================================================================

void GCParserIntegration::on_enter_scope(const std::string& scope_name, bool is_function) {
    VariableTracker::instance().enter_scope(scope_name, is_function, false);
}

void GCParserIntegration::on_exit_scope() {
    VariableTracker::instance().exit_scope();
}

void GCParserIntegration::on_variable_declaration(const std::string& name, DataType type) {
    VariableTracker::instance().register_variable(name, type);
}

void GCParserIntegration::on_function_call(const std::string& function_name, const std::vector<std::string>& args) {
    auto& tracker = VariableTracker::instance();
    
    // Mark all arguments as escaping via function call
    for (const std::string& arg : args) {
        tracker.mark_variable_escape(arg, EscapeType::FUNCTION_ARG);
    }
}

void GCParserIntegration::on_callback_creation(const std::vector<std::string>& captured_vars) {
    auto& tracker = VariableTracker::instance();
    
    // Mark all captured variables as escaping via callback
    for (const std::string& var : captured_vars) {
        tracker.mark_variable_escape(var, EscapeType::CALLBACK);
    }
}

void GCParserIntegration::on_object_assignment(const std::string& object_name, const std::string& property, const std::string& value_var) {
    auto& tracker = VariableTracker::instance();
    
    // Mark the value variable as escaping via object assignment
    tracker.mark_variable_escape(value_var, EscapeType::OBJECT_ASSIGN);
}

void GCParserIntegration::on_return_statement(const std::string& returned_var) {
    auto& tracker = VariableTracker::instance();
    
    // Mark returned variable as escaping via return
    tracker.mark_variable_escape(returned_var, EscapeType::RETURN_VALUE);
}

void GCParserIntegration::on_goroutine_creation(const std::vector<std::string>& captured_vars) {
    auto& tracker = VariableTracker::instance();
    
    // Mark all captured variables as escaping via goroutine
    for (const std::string& var : captured_vars) {
        tracker.mark_variable_escape(var, EscapeType::GOROUTINE);
    }
}

void GCParserIntegration::finalize_escape_analysis() {
    std::cout << "[GC] Finalizing escape analysis..." << std::endl;
    
    auto& tracker = VariableTracker::instance();
    auto& analyzer = EscapeAnalyzer::instance();
    
    // Dump analysis results
    dump_analysis_results();
}

void GCParserIntegration::dump_analysis_results() {
    std::cout << "\n===============================================" << std::endl;
    std::cout << "GARBAGE COLLECTION ANALYSIS RESULTS" << std::endl;
    std::cout << "===============================================" << std::endl;
    
    VariableTracker::instance().dump_scope_tree();
    VariableTracker::instance().dump_variables();
    
    const auto& escape_info = EscapeAnalyzer::instance().get_escape_info();
    
    std::cout << "\n=== ESCAPE ANALYSIS ===" << std::endl;
    std::cout << "Total escaping variables: " << escape_info.size() << std::endl;
    
    for (const auto& info : escape_info) {
        std::cout << "Variable '" << info.variable_name << "' escapes via ";
        switch (info.escape_type) {
            case EscapeType::FUNCTION_ARG: std::cout << "FUNCTION_ARG"; break;
            case EscapeType::CALLBACK: std::cout << "CALLBACK"; break;
            case EscapeType::OBJECT_ASSIGN: std::cout << "OBJECT_ASSIGN"; break;
            case EscapeType::RETURN_VALUE: std::cout << "RETURN_VALUE"; break;
            case EscapeType::GLOBAL_ASSIGN: std::cout << "GLOBAL_ASSIGN"; break;
            case EscapeType::GOROUTINE: std::cout << "GOROUTINE"; break;
            default: std::cout << "UNKNOWN"; break;
        }
        std::cout << " (requires heap: " << (info.requires_heap_alloc ? "YES" : "NO") << ")" << std::endl;
    }
}

} // namespace ultraScript

// ============================================================================
// C API IMPLEMENTATION
// ============================================================================

extern "C" {

void* __gc_alloc(size_t size, uint32_t type_id) {
    return ultraScript::GarbageCollector::instance().gc_alloc(size, type_id);
}

void* __gc_alloc_array(size_t element_size, size_t count, uint32_t type_id) {
    return ultraScript::GarbageCollector::instance().gc_alloc_array(element_size, count, type_id);
}

void __gc_free(void* ptr) {
    ultraScript::GarbageCollector::instance().gc_free(ptr);
}

void __gc_add_root(void** root_ptr) {
    ultraScript::GarbageCollector::instance().add_root(root_ptr);
}

void __gc_remove_root(void** root_ptr) {
    ultraScript::GarbageCollector::instance().remove_root(root_ptr);
}

void __gc_collect() {
    ultraScript::GarbageCollector::instance().collect();
}

void __gc_collect_young() {
    ultraScript::GarbageCollector::instance().collect_young();
}

int __gc_should_collect() {
    return ultraScript::GarbageCollector::instance().should_collect() ? 1 : 0;
}

void __gc_enter_scope(const char* scope_name, int is_function) {
    std::string name = scope_name ? scope_name : "";
    ultraScript::GCParserIntegration::on_enter_scope(name, is_function != 0);
}

void __gc_exit_scope() {
    ultraScript::GCParserIntegration::on_exit_scope();
}

void __gc_register_var(const char* name, int type) {
    ultraScript::GCParserIntegration::on_variable_declaration(
        name, static_cast<ultraScript::DataType>(type)
    );
}

void __gc_mark_escape(const char* name, int escape_type) {
    ultraScript::VariableTracker::instance().mark_variable_escape(
        name, static_cast<ultraScript::EscapeType>(escape_type)
    );
}

void __gc_finalize_analysis() {
    ultraScript::GCParserIntegration::finalize_escape_analysis();
}

}
