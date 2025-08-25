#pragma once

#include <thread>
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <memory>
#include <functional>
#include <signal.h>
#include <chrono>
#include <unordered_map>
#include <unordered_set>
#include <sys/epoll.h>
#include <sys/timerfd.h>
#include <cstdint>
#include <shared_mutex>
#include <string>
// Removed lexical_scope.h include - using pure static analysis now

// Forward declarations
class Goroutine;
class ThreadWorker;
class FFIThread;
class EventDrivenScheduler;
class AsyncManager;
class StackManager;
class EventSystem;
// Removed LexicalScope forward declaration - using pure static analysis now
class EventLifetimeManager;

// Goroutine states
enum class GoroutineState {
    CREATED,
    RUNNING,
    SUSPENDED,
    WAITING_FOR_ASYNC,
    COMPLETED
};

// Async operation types
enum class AsyncOpType {
    TIMER,
    HTTP_REQUEST,
    FILE_IO,
    PROMISE_ALL,
    CUSTOM
};

// CPU context structure for x86_64
struct GoroutineContext {
    // CPU Register State (x86_64)
    uint64_t rax, rbx, rcx, rdx, rsi, rdi, rbp;
    uint64_t r8, r9, r10, r11, r12, r13, r14, r15;
    uint64_t rsp;        // Stack pointer
    uint64_t rip;        // Instruction pointer
    uint64_t rflags;     // CPU flags
    
    // XMM/AVX registers for floating point
    alignas(16) uint8_t xmm_state[512]; // FXSAVE area
    
    // Stack management
    void* stack_base;
    void* stack_top;
    size_t stack_size;
    void* guard_page;
};

// Context switching functions (assembly)
extern "C" {
    void save_context(GoroutineContext* ctx);
    void restore_context(GoroutineContext* ctx);
    void switch_context(GoroutineContext* from, GoroutineContext* to);
    void signal_safe_context_switch(GoroutineContext* from, GoroutineContext* to);
}

// Async operation tracking
struct AsyncOperation {
    int64_t id;
    AsyncOpType type;
    std::atomic<bool> completed{false};
    void* result_data{nullptr};
    void* handle_data{nullptr};  // Additional handle data
    
    // For Promise.all coordination
    std::atomic<int> remaining_count{0};
    std::vector<void*> results;
    std::mutex results_mutex;
    
    // Associated goroutine
    std::weak_ptr<Goroutine> waiting_goroutine;
    
    // Completion callback
    std::function<void(void*)> completion_callback;
};

// Removed all lexical scope manager classes - using pure static analysis now

// Promise.all coordination
class PromiseAllOperation {
private:
    std::atomic<int> remaining_operations_;
    std::vector<void*> results_;
    std::vector<bool> completed_;
    std::mutex results_mutex_;
    std::shared_ptr<Goroutine> waiting_goroutine_;
    
public:
    PromiseAllOperation(int count, std::shared_ptr<Goroutine> goroutine);
    void complete_operation(int index, void* result);
    const std::vector<void*>& get_results() const { return results_; }
};

// Stack management
class StackManager {
private:
    // Stack pools for different sizes to avoid fragmentation
    std::vector<void*> stack_pool_8k_;
    std::vector<void*> stack_pool_64k_;
    std::vector<void*> stack_pool_512k_;
    std::mutex pool_mutex_;
    
public:
    static StackManager& instance();
    
    void* allocate_stack(size_t size);
    void deallocate_stack(void* stack, size_t size);
    void setup_guard_page(void* stack_base, size_t size);
    
    // Stack growth with pointer updating
    void* grow_stack(void* old_stack, size_t old_size, size_t new_size);
    void update_stack_pointers(void* old_base, void* new_base, size_t size);
};

// FFI Thread for zero-overhead FFI calls
class FFIThread {
private:
    std::thread native_thread_;
    std::atomic<bool> is_bound_{false};
    std::shared_ptr<Goroutine> bound_goroutine_{nullptr};
    std::condition_variable work_signal_;
    std::mutex work_mutex_;
    bool should_exit_{false};
    
public:
    FFIThread();
    ~FFIThread();
    
    void bind_goroutine(std::shared_ptr<Goroutine> goroutine);
    void execute_with_native_stack();
    bool is_available() const { return !is_bound_; }
    void release_binding();
    void* get_native_stack() const;
    void continue_execution(std::shared_ptr<Goroutine> goroutine);
    
private:
    void main_loop();
};

// FFI Thread Pool
class FFIThreadPool {
private:
    std::vector<std::unique_ptr<FFIThread>> ffi_threads_;
    std::atomic<int> available_count_;
    std::mutex allocation_mutex_;
    
public:
    static FFIThreadPool& instance();
    
    FFIThread* acquire_thread_for_binding();
    void release_thread(FFIThread* thread);
    void initialize_pool(int thread_count = 1000);
    void shutdown();
};

// Enhanced Goroutine class
class Goroutine : public std::enable_shared_from_this<Goroutine> {
private:
    int64_t id_;
    std::atomic<GoroutineState> state_{GoroutineState::CREATED};
    std::atomic<bool> is_running_{false};
    std::atomic<int> preferred_thread_id_{-1}; // -1 = no preference
    
    // FFI Thread Binding
    std::atomic<bool> is_ffi_bound_{false};
    FFIThread* bound_ffi_thread_{nullptr};
    std::atomic<int> ffi_call_count_{0};
    
    // Execution context
    GoroutineContext context_;
    
    // Stack management
    void* stack_memory_;
    size_t current_stack_size_;
    static constexpr size_t INITIAL_STACK_SIZE = 8192;    // 8KB initial
    static constexpr size_t MAX_STACK_SIZE = 1024*1024*1024; // 1GB max
    
    // Async operations
    std::unordered_map<int64_t, std::shared_ptr<AsyncOperation>> pending_async_ops_;
    std::vector<int> active_timer_fds_;
    
    // Promise coordination
    std::unordered_map<int64_t, std::shared_ptr<PromiseAllOperation>> promise_all_ops_;
    
    // Parent-child relationships
    std::weak_ptr<Goroutine> parent_;
    std::vector<std::shared_ptr<Goroutine>> children_;
    std::atomic<int> child_count_{0};
    
    // Function to execute
    std::function<void()> main_function_;
    
    // Execution result
    std::atomic<void*> execution_result_{nullptr};
    
    // Removed all lexical scope member variables - using pure static analysis now
    
public:
    Goroutine(int64_t id, std::function<void()> function);
    ~Goroutine();
    
    // Core lifecycle
    void start();
    void suspend();
    void resume();
    void yield();
    
    // Thread affinity for cache locality
    void set_preferred_thread(int thread_id) { preferred_thread_id_.store(thread_id); }
    int get_preferred_thread() const { return preferred_thread_id_.load(); }
    void clear_preferred_thread() { preferred_thread_id_.store(-1); }
    
    // FFI Thread Binding
    bool is_ffi_bound() const { return is_ffi_bound_.load(); }
    void set_ffi_bound(bool bound) { is_ffi_bound_.store(bound); }
    void set_bound_ffi_thread(FFIThread* thread) { bound_ffi_thread_ = thread; }
    FFIThread* get_bound_ffi_thread() const { return bound_ffi_thread_; }
    
    // Combined thread identification
    bool is_thread_bound() const { 
        return is_ffi_bound() || get_preferred_thread() != -1; 
    }
    
    // Thread migration safety
    bool can_migrate_to_ffi() const {
        return !is_ffi_bound() && state_.load() != GoroutineState::RUNNING;
    }
    
    // Stack management
    void grow_stack(size_t new_size);
    bool check_stack_overflow();
    void setup_guard_page();
    void* get_stack_base() const { return context_.stack_base; }
    size_t get_stack_size() const { return current_stack_size_; }
    void* get_stack() const { return stack_memory_; }
    
    // Context access
    GoroutineContext& get_context() { return context_; }
    const GoroutineContext& get_context() const { return context_; }
    
    // Execution
    void execute_main_function() { if (main_function_) main_function_(); }
    
    // Async coordination
    int64_t add_async_operation(std::shared_ptr<AsyncOperation> op);
    void complete_async_operation(int64_t op_id, void* result);
    
    // Timer management
    int64_t add_timer(int64_t delay_ms, void* callback, bool is_interval);
    void cancel_timer(int64_t timer_id);
    
    // Promise.all results
    void set_promise_all_results(const std::vector<void*>& results);
    
    // Execution result
    void* get_result() const { return execution_result_.load(); }
    void set_result(void* result) { execution_result_.store(result); }
    
    // State management
    GoroutineState get_state() const { return state_.load(); }
    void set_state(GoroutineState state) { state_.store(state); }
    int64_t get_id() const { return id_; }
    bool is_running() const { return is_running_.load(); }
    void set_running(bool running) { is_running_.store(running); }
    
    // Removed all lexical scope methods - using pure static analysis now
    
    // Debug information
    std::string get_debug_info() const;
};

// Continuation for trampoline pattern
enum class ContinuationAction {
    DONE,           // Thread should go idle
    RUN_GOROUTINE,  // Execute this goroutine  
    CHECK_QUEUE     // Check queues for more work
};

struct Continuation {
    ContinuationAction action;
    std::shared_ptr<Goroutine> goroutine;
    
    Continuation() : action(ContinuationAction::DONE), goroutine(nullptr) {}
    Continuation(ContinuationAction a, std::shared_ptr<Goroutine> g = nullptr) 
        : action(a), goroutine(g) {}
};

// Thread worker for event-driven execution
class ThreadWorker {
private:
    int thread_id_;
    std::atomic<bool> is_idle_{true};
    std::atomic<bool> should_exit_{false};
    
    // Work assignment and wake-up mechanism
    std::shared_ptr<Goroutine> assigned_work_{nullptr};
    std::condition_variable work_signal_;
    std::mutex work_mutex_;
    
    // Stack depth tracking for trampoline
    int stack_depth_{0};
    static constexpr int MAX_STACK_DEPTH = 100;
    
    friend class EventDrivenScheduler;  // Allow access to private members
    
public:
    ThreadWorker(int thread_id);
    ~ThreadWorker();
    
    void main_loop();
    bool try_assign_work(std::shared_ptr<Goroutine> goroutine);
    bool try_assign_queued_work();
    void wait_for_work();
    void wake_for_work();
    
private:
    Continuation execute_goroutine(std::shared_ptr<Goroutine> goroutine);
    Continuation check_and_get_next_work();
    bool run_goroutine_until_yield_or_complete(std::shared_ptr<Goroutine> goroutine);
};

// Event system with epoll integration
class EventSystem {
private:
    struct ThreadEventLoop {
        int epoll_fd;
        std::unordered_map<int, std::shared_ptr<AsyncOperation>> fd_to_op;
        std::vector<int> timer_fds;
    };
    
    std::vector<ThreadEventLoop> thread_loops_;
    int num_threads_;
    
public:
    static EventSystem& instance();
    
    void initialize(int num_threads);
    void shutdown();
    
    // Timer management using timerfd
    int64_t create_timer(int64_t delay_ms, bool is_interval, std::shared_ptr<Goroutine> goroutine);
    void cancel_timer(int64_t timer_id);
    
    // I/O event management
    void add_io_operation(int fd, uint32_t events, std::shared_ptr<AsyncOperation> op);
    void remove_io_operation(int fd);
    
    // Event processing
    void process_events(int thread_id, int timeout_ms);
    void process_timer_event(int timer_fd);
    void process_io_event(int fd, uint32_t events);
    
private:
    std::shared_ptr<Goroutine> find_goroutine_for_timer(int timer_fd);
    std::shared_ptr<AsyncOperation> find_async_op_for_fd(int fd);
    void* create_io_result(int fd, uint32_t events);
};

// Async operation manager
class AsyncManager {
private:
    std::unordered_map<int64_t, std::shared_ptr<AsyncOperation>> active_ops_;
    std::mutex ops_mutex_;
    std::atomic<int64_t> next_op_id_{1};
    
public:
    static AsyncManager& instance();
    
    int64_t create_async_operation(AsyncOpType type, std::shared_ptr<Goroutine> goroutine);
    void complete_async_operation(int64_t op_id, void* result);
    void handle_promise_all_completion(int64_t op_id, int result_index, void* result);
    std::shared_ptr<AsyncOperation> get_operation(int64_t op_id);
};

// Event-driven scheduler with race condition prevention
class EventDrivenScheduler {
private:
    // Global queues - no per-thread queues
    std::queue<std::shared_ptr<Goroutine>> priority_queue_;    // Timer callbacks, high priority
    std::queue<std::shared_ptr<Goroutine>> regular_queue_;     // Regular async work
    std::mutex queue_mutex_;
    
    // Thread pool management
    std::vector<std::unique_ptr<ThreadWorker>> thread_workers_;
    int num_threads_;
    std::atomic<bool> should_shutdown_{false};
    
    // FFI Thread Pool Integration
    std::unique_ptr<FFIThreadPool> ffi_thread_pool_;
    std::atomic<int> total_ffi_bound_goroutines_{0};
    
    // Global scheduling lock to prevent race conditions
    std::mutex scheduling_mutex_;
    
public:
    static EventDrivenScheduler& instance();
    
    void initialize(int num_threads = 0); // 0 = use hardware_concurrency
    void shutdown();
    void wait_for_completion(); // Wait for all goroutines to complete
    
    // Main scheduling functions
    void schedule_priority(std::shared_ptr<Goroutine> goroutine);
    void schedule_regular(std::shared_ptr<Goroutine> goroutine);
    
    // FFI Thread Binding
    FFIThread* acquire_ffi_thread() { return ffi_thread_pool_->acquire_thread_for_binding(); }
    void release_ffi_thread(FFIThread* thread) { ffi_thread_pool_->release_thread(thread); }
    bool bind_goroutine_to_ffi_thread(std::shared_ptr<Goroutine> goroutine);
    
    // Queue management for running threads
    std::shared_ptr<Goroutine> try_get_queued_work(int preferred_thread_id);
    
    // Event callbacks
    void on_async_event_complete(std::shared_ptr<Goroutine> goroutine, bool is_timer);
    
    // Affinity management
    void notify_thread_available(int thread_id);
    void clear_affinity_conflicts_for_ffi_binding(int old_thread_id);
    
private:
    bool try_wake_idle_thread(std::shared_ptr<Goroutine> goroutine);
    bool try_wake_idle_thread_for_queued_work();
    void wake_threads_for_queued_work();
    int find_least_loaded_thread();
};

// FFI integration functions
extern "C" {
    void* execute_ffi_call(Goroutine* current_goroutine, void* ffi_function, void* args);
    void* migrate_to_ffi_thread(Goroutine* goroutine, void* ffi_func, void* args);
    bool is_goroutine_ffi_bound(Goroutine* goroutine);
    void adjust_stack_pointers(Goroutine* goroutine, void* new_stack);
}

// Global functions
std::shared_ptr<Goroutine> get_current_goroutine();
void set_current_goroutine(std::shared_ptr<Goroutine> goroutine);
std::shared_ptr<Goroutine> spawn_goroutine_v2(std::function<void()> func);

// Global variables
extern std::atomic<int64_t> g_active_goroutine_count;

// Stack overflow signal handler
void stack_overflow_handler(int sig, siginfo_t* info, void* context);
bool is_stack_overflow(std::shared_ptr<Goroutine> goroutine, void* fault_addr);

// Timer functions for runtime integration
extern "C" {
    int64_t __gots_set_timeout_v2(void* function_address, int64_t delay_ms);
    int64_t __gots_set_interval_v2(void* function_address, int64_t delay_ms);
    bool __gots_clear_timeout_v2(int64_t timer_id);
    bool __gots_clear_interval_v2(int64_t timer_id);
    
    // Async operation functions
    int64_t __gots_add_async_handle_v2(int64_t type, void* handle_data);
    void __gots_complete_async_handle_v2(int64_t async_id);
    void __gots_cancel_async_handle_v2(int64_t async_id);
    
    // Main goroutine functions
    void __runtime_spawn_main_goroutine_v2(void* function_address);
    void __runtime_wait_for_main_goroutine_v2();
    
    // Goroutine spawning
    void* __runtime_spawn_goroutine_v2(void* function_address);
}
