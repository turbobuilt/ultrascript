#pragma once

#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <memory>
#include <atomic>
#include <mutex>
#include <string>
#include <queue>
#include <thread>
#include <condition_variable>
#include <functional>
#include "compiler.h"


// Forward declarations
class LexicalScope;
struct VariableBinding;

// Type system integration - define here to avoid circular dependencies
enum class PropertyType : uint8_t {
    DYNAMIC = 0,
    INT64 = 1,
    FLOAT64 = 2,
    STRING = 3,
    OBJECT_PTR = 4,
    BOOL = 5
};

// Minimal type system structures for GC integration
struct PropertyDescriptor {
    std::string name;
    uint32_t offset;
    PropertyType type;
    uint16_t index;
};

class ClassMetadata {
public:
    std::string class_name;
    std::vector<PropertyDescriptor> properties;
    uint32_t instance_size;
};

// ============================================================================
// ESCAPE ANALYSIS - Track variable lifetime and escape patterns
// ============================================================================

enum class EscapeType {
    NONE,           // Variable doesn't escape current scope
    FUNCTION_ARG,   // Passed as function argument
    CALLBACK,       // Used in callback/closure
    OBJECT_ASSIGN,  // Assigned to object property
    RETURN_VALUE,   // Returned from function
    GLOBAL_ASSIGN,  // Assigned to global variable
    GOROUTINE      // Captured by goroutine
};

struct EscapeInfo {
    size_t variable_id;
    std::string variable_name;
    size_t scope_id;
    EscapeType escape_type;
    size_t escape_site;     // Location where escape happens
    bool requires_heap_alloc = false;
    
    EscapeInfo(size_t var_id, const std::string& name, size_t scope, EscapeType type, size_t site)
        : variable_id(var_id), variable_name(name), scope_id(scope), escape_type(type), escape_site(site) {
        requires_heap_alloc = (type != EscapeType::NONE);
    }
};

// Tracks escape analysis during compilation
class EscapeAnalyzer {
public:
    static EscapeAnalyzer& instance();
    
    // Called during parsing/compilation
    void enter_scope(size_t scope_id);
    void exit_scope(size_t scope_id);
    void register_variable(size_t variable_id, const std::string& name, size_t scope_id);
    void register_escape(size_t variable_id, EscapeType type, size_t escape_site);
    
    // Query escape information
    bool does_variable_escape(size_t variable_id) const;
    const std::vector<EscapeInfo>& get_escape_info() const { return escape_info_; }
    
    // Clear analysis (between compilation units)
    void clear();
    
private:
    mutable std::mutex mutex_;
    std::vector<size_t> scope_stack_;
    std::unordered_map<size_t, std::string> variable_names_;
    std::unordered_map<size_t, size_t> variable_scopes_;
    std::vector<EscapeInfo> escape_info_;
    
    EscapeAnalyzer() = default;
};

// ============================================================================
// GC OBJECT HEADER - Minimal overhead tracking
// ============================================================================

struct GCObjectHeader {
    enum Flags : uint8_t {
        MARKED = 0x01,
        ESCAPED = 0x02,
        PINNED = 0x04,
        LARGE_OBJECT = 0x08
    };
    
    uint32_t size;          // Object size in bytes
    uint32_t type_id;       // Runtime type information
    uint8_t flags;          // GC flags
    uint8_t generation;     // Generation (0=young, 1=old)
    uint16_t padding;       // Padding for alignment
    
    void mark() { flags |= MARKED; }
    void unmark() { flags &= ~MARKED; }
    bool is_marked() const { return flags & MARKED; }
    
    void set_escaped() { flags |= ESCAPED; }
    bool has_escaped() const { return flags & ESCAPED; }
    
    void pin() { flags |= PINNED; }
    void unpin() { flags &= ~PINNED; }
    bool is_pinned() const { return flags & PINNED; }
};

// ============================================================================
// VARIABLE TRACKER - Tracks variables in each scope
// ============================================================================

struct ScopeInfo {
    size_t scope_id;
    size_t parent_scope_id;
    std::vector<size_t> child_scopes;
    std::unordered_map<std::string, size_t> variables;  // name -> variable_id
    bool is_function_scope = false;
    bool is_loop_scope = false;
    std::string scope_name;  // for debugging
};

struct VariableInfo {
    size_t variable_id = 0;
    std::string name;
    size_t scope_id = 0;
    DataType type = DataType::ANY;
    void* memory_location = nullptr;
    GCObjectHeader* gc_header = nullptr;
    bool is_local = true;
    bool escapes = false;
    std::vector<EscapeType> escape_types;
    
    VariableInfo() = default;  // Default constructor
    VariableInfo(size_t id, const std::string& var_name, size_t scope, DataType var_type)
        : variable_id(id), name(var_name), scope_id(scope), type(var_type) {}
};

// Tracks all variables and their scope relationships during parsing
class VariableTracker {
public:
    static VariableTracker& instance();
    
    // Scope management during parsing
    size_t enter_scope(const std::string& scope_name = "", bool is_function = false, bool is_loop = false);
    void exit_scope();
    size_t current_scope() const { return current_scope_id_; }
    
    // Variable registration
    size_t register_variable(const std::string& name, DataType type);
    VariableInfo* get_variable(size_t variable_id);
    VariableInfo* find_variable_in_scope(const std::string& name, size_t scope_id = 0);
    
    // Escape tracking
    void mark_variable_escape(size_t variable_id, EscapeType escape_type);
    void mark_variable_escape(const std::string& name, EscapeType escape_type);
    
    // Scope queries
    const ScopeInfo* get_scope(size_t scope_id) const;
    std::vector<size_t> get_variables_in_scope(size_t scope_id) const;
    std::vector<size_t> get_all_escaping_variables() const;
    
    // Clear between compilation units
    void clear();
    
    // Debug
    void dump_scope_tree() const;
    void dump_variables() const;
    
private:
    mutable std::mutex mutex_;
    size_t next_scope_id_ = 1;
    size_t next_variable_id_ = 1;
    size_t current_scope_id_ = 0;
    std::vector<size_t> scope_stack_;
    
    std::unordered_map<size_t, ScopeInfo> scopes_;
    std::unordered_map<size_t, VariableInfo> variables_;
    std::unordered_map<std::string, size_t> current_scope_variables_;  // Fast lookup in current scope
    
    VariableTracker() = default;
    void dump_scope_recursive(size_t scope_id, int indent = 0) const;
};

// ============================================================================
// GARBAGE COLLECTOR - Mark-sweep-defrag implementation
// ============================================================================

class GarbageCollector {
public:
    struct Stats {
        size_t total_allocated = 0;
        size_t total_freed = 0;
        size_t live_objects = 0;
        size_t collections = 0;
        size_t young_collections = 0;
        size_t old_collections = 0;
        double avg_collection_time_ms = 0.0;
        size_t defrag_operations = 0;
        size_t bytes_moved = 0;
    };
    
    static GarbageCollector& instance();
    
    // Memory allocation with GC tracking
    void* gc_alloc(size_t size, uint32_t type_id = 0);
    void* gc_alloc_array(size_t element_size, size_t count, uint32_t type_id = 0);
    void gc_free(void* ptr);
    
    // Root set management
    void add_root(void** root_ptr);
    void remove_root(void** root_ptr);
    void add_scope_roots(std::shared_ptr<LexicalScope> scope);
    void remove_scope_roots(std::shared_ptr<LexicalScope> scope);
    
    // GC operations
    void collect();  // Full collection
    void collect_young();  // Young generation only
    void request_collection();  // Async collection request
    bool should_collect() const;
    
    // Configuration
    void set_heap_limit(size_t bytes);
    void set_collection_threshold(double threshold);  // 0.0-1.0
    void enable_generational_gc(bool enable);
    void enable_concurrent_gc(bool enable);
    
    // Statistics
    const Stats& get_stats() const { return stats_; }
    size_t get_heap_size() const { return heap_size_; }
    size_t get_heap_used() const { return heap_used_; }
    
    // Shutdown
    void shutdown();
    
private:
    mutable std::mutex mutex_;
    std::atomic<bool> running_{true};
    
    // Memory management
    std::vector<void*> heap_blocks_;
    size_t heap_size_ = 0;
    size_t heap_used_ = 0;
    size_t heap_limit_ = 256 * 1024 * 1024;  // 256MB default
    double collection_threshold_ = 0.8;  // Collect at 80% full
    
    // Root set
    std::unordered_set<void**> roots_;
    std::vector<std::shared_ptr<LexicalScope>> root_scopes_;
    
    // Object tracking
    std::unordered_map<void*, GCObjectHeader*> object_headers_;
    std::queue<void*> mark_queue_;
    
    // Configuration
    bool generational_gc_enabled_ = true;
    bool concurrent_gc_enabled_ = false;
    
    // Background collection thread
    std::thread collector_thread_;
    std::condition_variable collection_cv_;
    std::atomic<bool> collection_requested_{false};
    
    // Statistics
    Stats stats_;
    
    GarbageCollector();
    ~GarbageCollector();
    
    // GC implementation
    void mark_phase();
    void sweep_phase();
    void defrag_phase();
    void mark_object(void* obj);
    void mark_roots();
    void mark_scope_variables(std::shared_ptr<LexicalScope> scope);
    void collector_thread_func();
    
    // Memory management
    GCObjectHeader* get_header(void* obj);
    void* allocate_with_header(size_t size, uint32_t type_id);
    
    // Type system integration
    void traverse_object_references(void* obj, uint32_t type_id);
    void handle_builtin_type_traversal(void* obj, uint32_t type_id);
    void handle_class_instance_traversal(void* obj, uint32_t type_id);
    void traverse_class_properties(void* obj, ClassMetadata* class_meta);
    void traverse_dynamic_array(void* array_obj);
    void traverse_generic_object(void* obj);
    void traverse_dynamic_property(void* prop_ptr);
    void conservative_scan_memory(void* ptr, size_t size);
    bool is_gc_managed(void* ptr);
    ClassMetadata* find_class_metadata_by_type_id(uint32_t type_id);
    
    // Scope and variable scanning
    bool contains_gc_references(DataType type);
    bool is_direct_gc_object(DataType type);
    bool is_reference_containing_type(DataType type);
    void scan_for_gc_references(void* ptr, DataType type);
    
    // Type mapping utilities
    uint32_t datatype_to_type_id(DataType type);
    DataType type_id_to_datatype(uint32_t type_id);
    void update_stats();
};

// ============================================================================
// INTEGRATION WITH PARSING - Called by parser
// ============================================================================

class GCParserIntegration {
public:
    // Called by parser during AST construction
    static void on_enter_scope(const std::string& scope_name = "", bool is_function = false);
    static void on_exit_scope();
    static void on_variable_declaration(const std::string& name, DataType type);
    static void on_function_call(const std::string& function_name, const std::vector<std::string>& args);
    static void on_callback_creation(const std::vector<std::string>& captured_vars);
    static void on_object_assignment(const std::string& object_name, const std::string& property, const std::string& value_var);
    static void on_return_statement(const std::string& returned_var);
    static void on_goroutine_creation(const std::vector<std::string>& captured_vars);
    
    // Called after parsing to finalize analysis
    static void finalize_escape_analysis();
    static void dump_analysis_results();
};

// ============================================================================
// UTILITY MACROS FOR JIT INTEGRATION
// ============================================================================

#define GC_ALLOC(size, type_id) GarbageCollector::instance().gc_alloc(size, type_id)
#define GC_ALLOC_ARRAY(elem_size, count, type_id) GarbageCollector::instance().gc_alloc_array(elem_size, count, type_id)
#define GC_FREE(ptr) GarbageCollector::instance().gc_free(ptr)
#define GC_ADD_ROOT(ptr) GarbageCollector::instance().add_root(reinterpret_cast<void**>(ptr))
#define GC_REMOVE_ROOT(ptr) GarbageCollector::instance().remove_root(reinterpret_cast<void**>(ptr))
#define GC_COLLECT() GarbageCollector::instance().collect()

// ============================================================================
// C API FOR JIT CODE GENERATION
// ============================================================================

extern "C" {
    void* __gc_alloc(size_t size, uint32_t type_id);
    void* __gc_alloc_array(size_t element_size, size_t count, uint32_t type_id);
    void __gc_free(void* ptr);
    void __gc_add_root(void** root_ptr);
    void __gc_remove_root(void** root_ptr);
    void __gc_collect();
    void __gc_collect_young();
    int __gc_should_collect();
    
    // Parser integration
    void __gc_enter_scope(const char* scope_name, int is_function);
    void __gc_exit_scope();
    void __gc_register_var(const char* name, int type);
    void __gc_mark_escape(const char* name, int escape_type);
    void __gc_finalize_analysis();
}

