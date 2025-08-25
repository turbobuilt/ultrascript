#include "goroutine_system_v2.h"
#include <iostream>
#include <algorithm>
#include <cstdlib>
#include <chrono>
#include <thread>
#include <signal.h>
#include <sys/mman.h>
#include <unistd.h>
#include <cstring>

// Global goroutine counter for compatibility
std::atomic<int64_t> g_active_goroutine_count{0};

// Thread-local current goroutine
thread_local std::shared_ptr<Goroutine> tl_current_goroutine = nullptr;

// Global goroutine ID counter (accessible from other modules)
std::atomic<int64_t> g_next_goroutine_id{1};

// Stack overflow signal handling
static std::atomic<bool> g_signal_handlers_installed{false};

//=============================================================================
// Stack Management Implementation
//=============================================================================

StackManager& StackManager::instance() {
    static StackManager instance;
    return instance;
}

void* StackManager::allocate_stack(size_t size) {
    std::lock_guard<std::mutex> lock(pool_mutex_);
    
    // Try to reuse from appropriate pool
    if (size <= 8192 && !stack_pool_8k_.empty()) {
        void* stack = stack_pool_8k_.back();
        stack_pool_8k_.pop_back();
        return stack;
    }
    if (size <= 65536 && !stack_pool_64k_.empty()) {
        void* stack = stack_pool_64k_.back();
        stack_pool_64k_.pop_back();
        return stack;
    }
    if (size <= 524288 && !stack_pool_512k_.empty()) {
        void* stack = stack_pool_512k_.back();
        stack_pool_512k_.pop_back();
        return stack;
    }
    
    // Allocate new stack with guard page
    void* stack = mmap(nullptr, size + 4096, // Extra page for guard
                       PROT_READ | PROT_WRITE,
                       MAP_PRIVATE | MAP_ANONYMOUS,
                       -1, 0);
    
    if (stack == MAP_FAILED) {
        throw std::runtime_error("Failed to allocate goroutine stack");
    }
    
    // Set up guard page at the beginning
    if (mprotect(stack, 4096, PROT_NONE) != 0) {
        munmap(stack, size + 4096);
        throw std::runtime_error("Failed to set up stack guard page");
    }
    
    // Return pointer to usable stack area (after guard page)
    return static_cast<char*>(stack) + 4096;
}

void StackManager::deallocate_stack(void* stack, size_t size) {
    std::lock_guard<std::mutex> lock(pool_mutex_);
    
    // Return to appropriate pool for reuse
    if (size <= 8192 && stack_pool_8k_.size() < 100) {
        stack_pool_8k_.push_back(stack);
        return;
    }
    if (size <= 65536 && stack_pool_64k_.size() < 50) {
        stack_pool_64k_.push_back(stack);
        return;
    }
    if (size <= 524288 && stack_pool_512k_.size() < 20) {
        stack_pool_512k_.push_back(stack);
        return;
    }
    
    // Free the memory (including guard page)
    void* actual_base = static_cast<char*>(stack) - 4096;
    munmap(actual_base, size + 4096);
}

void StackManager::setup_guard_page(void* stack_base, size_t size) {
    // Guard page is already set up in allocate_stack
    // This is a no-op for compatibility
}

void* StackManager::grow_stack(void* old_stack, size_t old_size, size_t new_size) {
    void* new_stack = allocate_stack(new_size);
    memcpy(new_stack, old_stack, old_size);
    update_stack_pointers(old_stack, new_stack, old_size);
    deallocate_stack(old_stack, old_size);
    return new_stack;
}

void StackManager::update_stack_pointers(void* old_base, void* new_base, size_t size) {
    // Scan through stack and update any pointers that point into old stack
    uintptr_t* stack_ptr = static_cast<uintptr_t*>(new_base);
    uintptr_t old_start = reinterpret_cast<uintptr_t>(old_base);
    uintptr_t old_end = old_start + size;
    uintptr_t offset = reinterpret_cast<uintptr_t>(new_base) - old_start;
    
    for (size_t i = 0; i < size / sizeof(uintptr_t); ++i) {
        if (stack_ptr[i] >= old_start && stack_ptr[i] < old_end) {
            stack_ptr[i] += offset;
        }
    }
}

//=============================================================================
// Signal Handler for Stack Overflow
//=============================================================================

void stack_overflow_handler(int sig, siginfo_t* info, void* context) {
    auto current = get_current_goroutine();
    if (current && is_stack_overflow(current, info->si_addr)) {
        // Grow stack and continue
        try {
            current->grow_stack(current->get_stack_size() * 2);
            return; // Resume execution
        } catch (const std::exception& e) {
            std::cerr << "Failed to grow stack: " << e.what() << std::endl;
        }
    }
    
    // Not a stack overflow or growth failed, re-raise signal
    signal(SIGSEGV, SIG_DFL);
    raise(SIGSEGV);
}

bool is_stack_overflow(std::shared_ptr<Goroutine> goroutine, void* fault_addr) {
    if (!goroutine) return false;
    
    void* stack_base = goroutine->get_stack_base();
    size_t stack_size = goroutine->get_stack_size();
    
    uintptr_t fault = reinterpret_cast<uintptr_t>(fault_addr);
    uintptr_t stack_start = reinterpret_cast<uintptr_t>(stack_base);
    uintptr_t guard_page = stack_start - 4096; // Guard page is before stack
    
    // Check if fault is in guard page area
    return (fault >= guard_page && fault < stack_start);
}

//=============================================================================
// Promise.all Implementation
//=============================================================================

PromiseAllOperation::PromiseAllOperation(int count, std::shared_ptr<Goroutine> goroutine) 
    : remaining_operations_(count), results_(count), completed_(count, false),
      waiting_goroutine_(goroutine) {}

void PromiseAllOperation::complete_operation(int index, void* result) {
    {
        std::lock_guard<std::mutex> lock(results_mutex_);
        if (completed_[index]) return; // Already completed
        
        results_[index] = result;
        completed_[index] = true;
    }
    
    int remaining = remaining_operations_.fetch_sub(1) - 1;
    if (remaining == 0) {
        // All operations complete, resume goroutine
        if (waiting_goroutine_) {
            // Store results in goroutine context for retrieval
            waiting_goroutine_->set_promise_all_results(results_);
            
            // Schedule goroutine for execution
            EventDrivenScheduler::instance().schedule_regular(waiting_goroutine_);
        }
    }
}

//=============================================================================
// Goroutine Implementation
//=============================================================================

Goroutine::Goroutine(int64_t id, std::function<void()> function)
    : id_(id), main_function_(std::move(function)), 
      current_stack_size_(INITIAL_STACK_SIZE) {
    
    // Allocate initial stack
    stack_memory_ = StackManager::instance().allocate_stack(current_stack_size_);
    
    // Initialize context
    memset(&context_, 0, sizeof(context_));
    context_.stack_base = stack_memory_;
    context_.stack_top = static_cast<char*>(stack_memory_) + current_stack_size_;
    context_.stack_size = current_stack_size_;
    
    // Set initial stack pointer (grows downward)
    context_.rsp = reinterpret_cast<uint64_t>(context_.stack_top) - 16; // 16-byte alignment
    
    // Install signal handlers on first goroutine creation
    if (!g_signal_handlers_installed.exchange(true)) {
        struct sigaction sa;
        sa.sa_sigaction = stack_overflow_handler;
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = SA_SIGINFO;
        sigaction(SIGSEGV, &sa, nullptr);
    }
}

Goroutine::~Goroutine() {
    if (stack_memory_) {
        StackManager::instance().deallocate_stack(stack_memory_, current_stack_size_);
    }
    
    // Release FFI thread if bound
    if (is_ffi_bound() && bound_ffi_thread_) {
        FFIThreadPool::instance().release_thread(bound_ffi_thread_);
    }
}

void Goroutine::start() {
    state_.store(GoroutineState::RUNNING);
    // Note: Actual execution will be handled by the scheduler/thread worker
}

void Goroutine::suspend() {
    state_.store(GoroutineState::SUSPENDED);
}

void Goroutine::resume() {
    if (state_.load() == GoroutineState::SUSPENDED) {
        state_.store(GoroutineState::RUNNING);
        EventDrivenScheduler::instance().schedule_regular(shared_from_this());
    }
}

void Goroutine::yield() {
    // Context switch will be handled by the thread worker
    state_.store(GoroutineState::SUSPENDED);
}

void Goroutine::grow_stack(size_t new_size) {
    if (new_size <= current_stack_size_ || new_size > MAX_STACK_SIZE) {
        return;
    }
    
    void* old_stack = stack_memory_;
    size_t old_size = current_stack_size_;
    
    // Allocate new larger stack
    void* new_stack = StackManager::instance().allocate_stack(new_size);
    
    // Copy old stack contents
    memcpy(new_stack, old_stack, old_size);
    
    // Update all stack-relative pointers
    StackManager::instance().update_stack_pointers(old_stack, new_stack, old_size);
    
    // Update stack pointer in context
    uintptr_t offset = context_.rsp - reinterpret_cast<uintptr_t>(old_stack);
    context_.rsp = reinterpret_cast<uintptr_t>(static_cast<char*>(new_stack) + offset);
    
    // Update other stack-relative pointers (rbp, etc.)
    if (context_.rbp >= reinterpret_cast<uintptr_t>(old_stack) && 
        context_.rbp < reinterpret_cast<uintptr_t>(old_stack) + old_size) {
        uintptr_t rbp_offset = context_.rbp - reinterpret_cast<uintptr_t>(old_stack);
        context_.rbp = reinterpret_cast<uintptr_t>(new_stack) + rbp_offset;
    }
    
    // Update context stack info
    context_.stack_base = new_stack;
    context_.stack_top = static_cast<char*>(new_stack) + new_size;
    context_.stack_size = new_size;
    
    // Free old stack
    StackManager::instance().deallocate_stack(old_stack, old_size);
    
    stack_memory_ = new_stack;
    current_stack_size_ = new_size;
}

bool Goroutine::check_stack_overflow() {
    // Check if we're getting close to the guard page
    uintptr_t current_sp = context_.rsp;
    uintptr_t stack_bottom = reinterpret_cast<uintptr_t>(stack_memory_);
    
    // If we're within 1KB of the bottom, consider it overflow risk
    return (current_sp - stack_bottom) < 1024;
}

void Goroutine::setup_guard_page() {
    // Guard page is already set up by StackManager
}

int64_t Goroutine::add_async_operation(std::shared_ptr<AsyncOperation> op) {
    int64_t op_id = AsyncManager::instance().create_async_operation(op->type, shared_from_this());
    pending_async_ops_[op_id] = op;
    return op_id;
}

void Goroutine::complete_async_operation(int64_t op_id, void* result) {
    auto it = pending_async_ops_.find(op_id);
    if (it != pending_async_ops_.end()) {
        it->second->result_data = result;
        it->second->completed.store(true);
        pending_async_ops_.erase(it);
        
        // Resume this goroutine
        EventDrivenScheduler::instance().schedule_regular(shared_from_this());
    }
}

int64_t Goroutine::add_timer(int64_t delay_ms, void* callback, bool is_interval) {
    return EventSystem::instance().create_timer(delay_ms, is_interval, shared_from_this());
}

void Goroutine::cancel_timer(int64_t timer_id) {
    EventSystem::instance().cancel_timer(timer_id);
}

void Goroutine::set_promise_all_results(const std::vector<void*>& results) {
    // Store results in goroutine for later retrieval by JIT code
    // This would need integration with the runtime system
}

//=============================================================================
// FFI Thread Implementation
//=============================================================================

FFIThread::FFIThread() : should_exit_(false) {
    native_thread_ = std::thread(&FFIThread::main_loop, this);
}

FFIThread::~FFIThread() {
    should_exit_ = true;
    work_signal_.notify_all();
    if (native_thread_.joinable()) {
        native_thread_.join();
    }
}

void FFIThread::bind_goroutine(std::shared_ptr<Goroutine> goroutine) {
    std::lock_guard<std::mutex> lock(work_mutex_);
    bound_goroutine_ = goroutine;
    is_bound_.store(true);
    work_signal_.notify_one();
}

void FFIThread::execute_with_native_stack() {
    // This will be called from the native thread context
    // The goroutine's stack has been migrated here
    if (bound_goroutine_) {
        // Continue execution on native stack
        // This is complex and would need more implementation
    }
}

void FFIThread::release_binding() {
    std::lock_guard<std::mutex> lock(work_mutex_);
    bound_goroutine_ = nullptr;
    is_bound_.store(false);
}

void* FFIThread::get_native_stack() const {
    // Return the native OS thread's stack
    // This is a simplified version - would need actual implementation
    return nullptr;
}

void FFIThread::continue_execution(std::shared_ptr<Goroutine> goroutine) {
    // Continue executing goroutine on this native thread
    // Complex implementation needed
}

void FFIThread::main_loop() {
    while (!should_exit_) {
        std::unique_lock<std::mutex> lock(work_mutex_);
        work_signal_.wait(lock, [this] { return bound_goroutine_ || should_exit_; });
        
        if (should_exit_) break;
        
        if (bound_goroutine_) {
            // Execute goroutine on this native thread
            execute_with_native_stack();
        }
    }
}

//=============================================================================
// FFI Thread Pool Implementation
//=============================================================================

FFIThreadPool& FFIThreadPool::instance() {
    static FFIThreadPool instance;
    return instance;
}

FFIThread* FFIThreadPool::acquire_thread_for_binding() {
    std::lock_guard<std::mutex> lock(allocation_mutex_);
    
    for (auto& thread : ffi_threads_) {
        if (thread->is_available()) {
            available_count_.fetch_sub(1);
            return thread.get();
        }
    }
    
    return nullptr; // No threads available
}

void FFIThreadPool::release_thread(FFIThread* thread) {
    if (thread) {
        thread->release_binding();
        available_count_.fetch_add(1);
    }
}

void FFIThreadPool::initialize_pool(int thread_count) {
    std::lock_guard<std::mutex> lock(allocation_mutex_);
    
    ffi_threads_.clear();
    ffi_threads_.reserve(thread_count);
    
    for (int i = 0; i < thread_count; ++i) {
        ffi_threads_.push_back(std::make_unique<FFIThread>());
    }
    
    available_count_.store(thread_count);
}

void FFIThreadPool::shutdown() {
    std::lock_guard<std::mutex> lock(allocation_mutex_);
    ffi_threads_.clear();
    available_count_.store(0);
}

// Global utility functions
std::shared_ptr<Goroutine> get_current_goroutine() {
    return tl_current_goroutine;
}

void set_current_goroutine(std::shared_ptr<Goroutine> goroutine) {
    tl_current_goroutine = goroutine;
}

//=============================================================================
// Thread Worker Implementation
//=============================================================================

ThreadWorker::ThreadWorker(int thread_id) : thread_id_(thread_id), stack_depth_(0) {}

ThreadWorker::~ThreadWorker() {
    should_exit_.store(true);
    work_signal_.notify_all();
}

void ThreadWorker::main_loop() {
    while (!should_exit_.load()) {
        // 1. Wait for work assignment (blocks until work arrives)
        wait_for_work();
        
        if (should_exit_.load()) break;
        
        // 2. Determine work type and execute using trampoline pattern
        Continuation cont;
        stack_depth_ = 0;
        
        if (assigned_work_) {
            // Direct work assignment
            cont = Continuation(ContinuationAction::RUN_GOROUTINE, assigned_work_);
        } else {
            // Queue check request (race condition fix)
            cont = Continuation(ContinuationAction::CHECK_QUEUE);
        }
        
        while (cont.action != ContinuationAction::DONE) {
            switch (cont.action) {
                case ContinuationAction::RUN_GOROUTINE:
                    cont = execute_goroutine(cont.goroutine);
                    break;
                    
                case ContinuationAction::CHECK_QUEUE:
                    cont = check_and_get_next_work();
                    break;
                    
                case ContinuationAction::DONE:
                    break;
            }
        }
        
        // 3. Clear work assignment and return to idle state
        assigned_work_ = nullptr;
    }
}

bool ThreadWorker::try_assign_work(std::shared_ptr<Goroutine> goroutine) {
    // Atomic check-and-set to claim this thread
    bool expected = true;
    if (is_idle_.compare_exchange_strong(expected, false)) {
        // Successfully claimed thread
        {
            std::lock_guard<std::mutex> lock(work_mutex_);
            assigned_work_ = goroutine;
        }
        work_signal_.notify_one();
        return true;
    }
    return false; // Thread was busy
}

bool ThreadWorker::try_assign_queued_work() {
    // Race condition fix - wake thread to check queues
    bool expected = true;
    if (is_idle_.compare_exchange_strong(expected, false)) {
        // Successfully claimed idle thread
        {
            std::lock_guard<std::mutex> lock(work_mutex_);
            assigned_work_ = nullptr; // Signal: no direct work, check queue
        }
        work_signal_.notify_one();
        return true;
    }
    return false; // Thread was busy
}

void ThreadWorker::wait_for_work() {
    std::unique_lock<std::mutex> lock(work_mutex_);
    
    // Mark as idle and wait for work assignment
    is_idle_.store(true);
    work_signal_.wait(lock, [this] { 
        return assigned_work_ != nullptr || !is_idle_.load() || should_exit_.load(); 
    });
    
    // Handle both direct work assignment and queue check requests
    if ((assigned_work_ || !is_idle_.load()) && !should_exit_.load()) {
        is_idle_.store(false);
    }
}

void ThreadWorker::wake_for_work() {
    work_signal_.notify_one();
}

Continuation ThreadWorker::execute_goroutine(std::shared_ptr<Goroutine> goroutine) {
    // Set thread affinity for cache locality
    goroutine->set_preferred_thread(thread_id_);
    
    // Mark goroutine as running
    bool expected = false;
    if (!goroutine->is_running()) {
        goroutine->set_running(true);
    } else {
        // Race condition - goroutine already running elsewhere
        return {ContinuationAction::CHECK_QUEUE};
    }
    
    // Set as current goroutine for this thread
    set_current_goroutine(goroutine);
    
    // Execute goroutine (context switch happens here)
    bool completed = run_goroutine_until_yield_or_complete(goroutine);
    
    // Clear current goroutine
    set_current_goroutine(nullptr);
    
    // Mark as not running
    goroutine->set_running(false);
    
    if (completed) {
        // Goroutine finished - check for more work to avoid going idle
        return {ContinuationAction::CHECK_QUEUE};
    } else {
        // Goroutine yielded (async operation) - thread can go idle
        return {ContinuationAction::DONE};
    }
}

Continuation ThreadWorker::check_and_get_next_work() {
    // Increment stack depth to prevent infinite recursion
    stack_depth_++;
    
    if (stack_depth_ >= MAX_STACK_DEPTH) {
        // Stack getting too deep - queue any remaining work and return
        // This prevents stack overflow in pathological cases
        return {ContinuationAction::DONE};
    }
    
    // Try to get queued work, preferring this thread
    auto next_work = EventDrivenScheduler::instance().try_get_queued_work(thread_id_);
    
    if (next_work) {
        return {ContinuationAction::RUN_GOROUTINE, next_work};
    }
    
    // No more work - thread will go idle
    return {ContinuationAction::DONE};
}

bool ThreadWorker::run_goroutine_until_yield_or_complete(std::shared_ptr<Goroutine> goroutine) {
    try {
        // Check stack overflow
        if (goroutine->check_stack_overflow()) {
            goroutine->grow_stack(goroutine->get_stack_size() * 2);
        }
        
        // For now, just run the main function
        // In a full implementation, this would handle context switching
        if (goroutine->get_state() == GoroutineState::CREATED) {
            // First time running - execute main function
            goroutine->execute_main_function();
            goroutine->set_state(GoroutineState::COMPLETED);
            return true; // Completed
        } else if (goroutine->get_state() == GoroutineState::RUNNING) {
            // Resume execution - this would need proper context switching
            // For now, just mark as completed
            goroutine->set_state(GoroutineState::COMPLETED);
            return true;
        }
        
        return false; // Yielded
    } catch (const std::exception& e) {
        std::cerr << "Goroutine " << goroutine->get_id() << " threw exception: " << e.what() << std::endl;
        goroutine->set_state(GoroutineState::COMPLETED);
        return true; // Completed with error
    }
}

//=============================================================================
// Event System Implementation
//=============================================================================

EventSystem& EventSystem::instance() {
    static EventSystem instance;
    return instance;
}

void EventSystem::initialize(int num_threads) {
    num_threads_ = num_threads;
    thread_loops_.resize(num_threads_);
    
    for (int i = 0; i < num_threads_; ++i) {
        thread_loops_[i].epoll_fd = epoll_create1(EPOLL_CLOEXEC);
        if (thread_loops_[i].epoll_fd == -1) {
            throw std::runtime_error("Failed to create epoll instance");
        }
    }
}

void EventSystem::shutdown() {
    for (auto& loop : thread_loops_) {
        if (loop.epoll_fd >= 0) {
            close(loop.epoll_fd);
        }
        
        // Close all timer fds
        for (int fd : loop.timer_fds) {
            close(fd);
        }
    }
    thread_loops_.clear();
}

int64_t EventSystem::create_timer(int64_t delay_ms, bool is_interval, std::shared_ptr<Goroutine> goroutine) {
    int timer_fd = timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC);
    if (timer_fd == -1) return -1;
    
    struct itimerspec timer_spec;
    timer_spec.it_value.tv_sec = delay_ms / 1000;
    timer_spec.it_value.tv_nsec = (delay_ms % 1000) * 1000000;
    
    if (is_interval) {
        timer_spec.it_interval = timer_spec.it_value;
    } else {
        timer_spec.it_interval.tv_sec = 0;
        timer_spec.it_interval.tv_nsec = 0;
    }
    
    if (timerfd_settime(timer_fd, 0, &timer_spec, nullptr) == -1) {
        close(timer_fd);
        return -1;
    }
    
    // Add to epoll (use thread 0 for now, could distribute better)
    struct epoll_event event;
    event.events = EPOLLIN;
    event.data.fd = timer_fd;
    
    int thread_id = 0; // Simple assignment to thread 0
    if (thread_id < num_threads_) {
        epoll_ctl(thread_loops_[thread_id].epoll_fd, EPOLL_CTL_ADD, timer_fd, &event);
        
        // Create async operation for timer
        auto async_op = std::make_shared<AsyncOperation>();
        async_op->type = AsyncOpType::TIMER;
        async_op->waiting_goroutine = goroutine;
        
        thread_loops_[thread_id].fd_to_op[timer_fd] = async_op;
        thread_loops_[thread_id].timer_fds.push_back(timer_fd);
    }
    
    return timer_fd; // Use fd as timer ID
}

void EventSystem::cancel_timer(int64_t timer_id) {
    int timer_fd = static_cast<int>(timer_id);
    
    // Find and remove from appropriate thread loop
    for (int i = 0; i < num_threads_; ++i) {
        auto& loop = thread_loops_[i];
        auto it = loop.fd_to_op.find(timer_fd);
        if (it != loop.fd_to_op.end()) {
            epoll_ctl(loop.epoll_fd, EPOLL_CTL_DEL, timer_fd, nullptr);
            loop.fd_to_op.erase(it);
            
            // Remove from timer_fds vector
            auto timer_it = std::find(loop.timer_fds.begin(), loop.timer_fds.end(), timer_fd);
            if (timer_it != loop.timer_fds.end()) {
                loop.timer_fds.erase(timer_it);
            }
            
            close(timer_fd);
            break;
        }
    }
}

void EventSystem::add_io_operation(int fd, uint32_t events, std::shared_ptr<AsyncOperation> op) {
    struct epoll_event event;
    event.events = events;
    event.data.fd = fd;
    
    int thread_id = 0; // Simple assignment to thread 0
    if (thread_id < num_threads_) {
        epoll_ctl(thread_loops_[thread_id].epoll_fd, EPOLL_CTL_ADD, fd, &event);
        thread_loops_[thread_id].fd_to_op[fd] = op;
    }
}

void EventSystem::remove_io_operation(int fd) {
    for (int i = 0; i < num_threads_; ++i) {
        auto& loop = thread_loops_[i];
        auto it = loop.fd_to_op.find(fd);
        if (it != loop.fd_to_op.end()) {
            epoll_ctl(loop.epoll_fd, EPOLL_CTL_DEL, fd, nullptr);
            loop.fd_to_op.erase(it);
            break;
        }
    }
}

void EventSystem::process_events(int thread_id, int timeout_ms) {
    if (thread_id >= num_threads_) return;
    
    auto& loop = thread_loops_[thread_id];
    struct epoll_event events[64];
    
    int num_events = epoll_wait(loop.epoll_fd, events, 64, timeout_ms);
    for (int i = 0; i < num_events; ++i) {
        int fd = events[i].data.fd;
        uint32_t event_mask = events[i].events;
        
        auto it = loop.fd_to_op.find(fd);
        if (it != loop.fd_to_op.end()) {
            if (std::find(loop.timer_fds.begin(), loop.timer_fds.end(), fd) != loop.timer_fds.end()) {
                process_timer_event(fd);
            } else {
                process_io_event(fd, event_mask);
            }
        }
    }
}

void EventSystem::process_timer_event(int timer_fd) {
    // Read the timer to clear it
    uint64_t expired_count;
    read(timer_fd, &expired_count, sizeof(expired_count));
    
    // Find goroutine associated with this timer
    auto goroutine = find_goroutine_for_timer(timer_fd);
    if (goroutine) {
        EventDrivenScheduler::instance().on_async_event_complete(goroutine, true);
    }
}

void EventSystem::process_io_event(int fd, uint32_t events) {
    // Find async operation associated with this fd
    auto async_op = find_async_op_for_fd(fd);
    if (async_op && !async_op->waiting_goroutine.expired()) {
        auto goroutine = async_op->waiting_goroutine.lock();
        if (goroutine) {
            // Store I/O result in async operation
            async_op->result_data = create_io_result(fd, events);
            async_op->completed.store(true);
            
            // Schedule goroutine to resume
            EventDrivenScheduler::instance().on_async_event_complete(goroutine, false);
        }
    }
}

std::shared_ptr<Goroutine> EventSystem::find_goroutine_for_timer(int timer_fd) {
    for (auto& loop : thread_loops_) {
        auto it = loop.fd_to_op.find(timer_fd);
        if (it != loop.fd_to_op.end()) {
            return it->second->waiting_goroutine.lock();
        }
    }
    return nullptr;
}

std::shared_ptr<AsyncOperation> EventSystem::find_async_op_for_fd(int fd) {
    for (auto& loop : thread_loops_) {
        auto it = loop.fd_to_op.find(fd);
        if (it != loop.fd_to_op.end()) {
            return it->second;
        }
    }
    return nullptr;
}

void* EventSystem::create_io_result(int fd, uint32_t events) {
    // Create a simple result structure
    // In a real implementation, this would be more sophisticated
    struct IOResult {
        int fd;
        uint32_t events;
    };
    
    auto* result = new IOResult{fd, events};
    return result;
}

//=============================================================================
// Async Manager Implementation
//=============================================================================

AsyncManager& AsyncManager::instance() {
    static AsyncManager instance;
    return instance;
}

int64_t AsyncManager::create_async_operation(AsyncOpType type, std::shared_ptr<Goroutine> goroutine) {
    std::lock_guard<std::mutex> lock(ops_mutex_);
    
    int64_t op_id = next_op_id_.fetch_add(1);
    auto async_op = std::make_shared<AsyncOperation>();
    async_op->id = op_id;
    async_op->type = type;
    async_op->waiting_goroutine = goroutine;
    
    active_ops_[op_id] = async_op;
    return op_id;
}

void AsyncManager::complete_async_operation(int64_t op_id, void* result) {
    std::lock_guard<std::mutex> lock(ops_mutex_);
    
    auto it = active_ops_.find(op_id);
    if (it != active_ops_.end()) {
        it->second->result_data = result;
        it->second->completed.store(true);
        
        if (it->second->completion_callback) {
            it->second->completion_callback(result);
        }
        
        // Wake up waiting goroutine
        if (auto goroutine = it->second->waiting_goroutine.lock()) {
            EventDrivenScheduler::instance().schedule_regular(goroutine);
        }
        
        active_ops_.erase(it);
    }
}

void AsyncManager::handle_promise_all_completion(int64_t op_id, int result_index, void* result) {
    // This would handle Promise.all specific coordination
    // Implementation depends on how Promise.all is integrated
}

std::shared_ptr<AsyncOperation> AsyncManager::get_operation(int64_t op_id) {
    std::lock_guard<std::mutex> lock(ops_mutex_);
    
    auto it = active_ops_.find(op_id);
    if (it != active_ops_.end()) {
        return it->second;
    }
    return nullptr;
}

//=============================================================================
// Event Driven Scheduler Implementation
//=============================================================================

EventDrivenScheduler& EventDrivenScheduler::instance() {
    static EventDrivenScheduler instance;
    return instance;
}

void EventDrivenScheduler::initialize(int num_threads) {
    if (num_threads <= 0) {
        num_threads = std::thread::hardware_concurrency();
        if (num_threads <= 0) num_threads = 4; // Fallback
    }
    
    num_threads_ = num_threads;
    
    // Initialize thread workers
    thread_workers_.reserve(num_threads_);
    for (int i = 0; i < num_threads_; ++i) {
        thread_workers_.push_back(std::make_unique<ThreadWorker>(i));
    }
    
    // Start thread worker threads
    for (int i = 0; i < num_threads_; ++i) {
        std::thread worker_thread(&ThreadWorker::main_loop, thread_workers_[i].get());
        worker_thread.detach(); // Let them run independently
    }
    
    // Initialize FFI thread pool
    ffi_thread_pool_ = std::make_unique<FFIThreadPool>();
    ffi_thread_pool_->initialize_pool(1000); // 1000 FFI threads
    
    // Initialize event system
    EventSystem::instance().initialize(num_threads_);
}

void EventDrivenScheduler::shutdown() {
    should_shutdown_.store(true);
    
    // Shutdown thread workers
    for (auto& worker : thread_workers_) {
        worker->should_exit_.store(true);
        worker->wake_for_work();
    }
    
    // Shutdown FFI thread pool
    if (ffi_thread_pool_) {
        ffi_thread_pool_->shutdown();
    }
    
    // Shutdown event system
    EventSystem::instance().shutdown();
    
    thread_workers_.clear();
}

void EventDrivenScheduler::wait_for_completion() {
    // Wait for all queues to be empty and all threads to be idle
    bool all_idle = false;
    while (!all_idle) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        
        // Check if queues are empty
        std::lock_guard<std::mutex> lock(queue_mutex_);
        if (!priority_queue_.empty() || !regular_queue_.empty()) {
            continue;
        }
        
        // Check if all threads are idle
        all_idle = true;
        for (auto& worker : thread_workers_) {
            if (!worker->is_idle_.load()) {
                all_idle = false;
                break;
            }
        }
    }
}

void EventDrivenScheduler::schedule_priority(std::shared_ptr<Goroutine> goroutine) {
    // CRITICAL: Atomic operation to prevent lost work
    std::lock_guard<std::mutex> global_lock(scheduling_mutex_);
    
    // 1. Try to wake an idle thread immediately
    if (try_wake_idle_thread(goroutine)) {
        return; // Success - thread is handling the goroutine
    }
    
    // 2. No idle threads available - queue for later
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        priority_queue_.push(goroutine);
    }
    
    // 3. CRITICAL FIX: Try to wake threads again after queuing
    // This handles the race condition where threads went idle between steps 1 and 2
    try_wake_idle_thread_for_queued_work();
}

void EventDrivenScheduler::schedule_regular(std::shared_ptr<Goroutine> goroutine) {
    // CRITICAL: Same race condition fix applied to regular scheduling
    std::lock_guard<std::mutex> global_lock(scheduling_mutex_);
    
    // 1. Try to wake an idle thread immediately
    if (try_wake_idle_thread(goroutine)) {
        return; // Success - thread is handling the goroutine
    }
    
    // 2. No idle threads available - queue for later
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        regular_queue_.push(goroutine);
    }
    
    // 3. CRITICAL FIX: Try to wake threads again after queuing
    // This handles the race condition where threads went idle between steps 1 and 2
    try_wake_idle_thread_for_queued_work();
}

bool EventDrivenScheduler::bind_goroutine_to_ffi_thread(std::shared_ptr<Goroutine> goroutine) {
    if (!goroutine->can_migrate_to_ffi()) {
        return false;
    }
    
    FFIThread* ffi_thread = acquire_ffi_thread();
    if (!ffi_thread) {
        return false; // No FFI threads available
    }
    
    // Handle thread affinity conflicts
    int old_preferred_thread = goroutine->get_preferred_thread();
    if (old_preferred_thread != -1) {
        goroutine->clear_preferred_thread();
        clear_affinity_conflicts_for_ffi_binding(old_preferred_thread);
    }
    
    // Bind goroutine to FFI thread
    goroutine->set_ffi_bound(true);
    goroutine->set_bound_ffi_thread(ffi_thread);
    ffi_thread->bind_goroutine(goroutine);
    
    total_ffi_bound_goroutines_.fetch_add(1);
    
    return true;
}

std::shared_ptr<Goroutine> EventDrivenScheduler::try_get_queued_work(int preferred_thread_id) {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    
    // Helper to find preferred work in a queue
    auto find_preferred = [preferred_thread_id](auto& queue) -> std::shared_ptr<Goroutine> {
        // Simple approach: scan queue for preferred thread work
        std::queue<std::shared_ptr<Goroutine>> temp_queue;
        std::shared_ptr<Goroutine> found = nullptr;
        
        while (!queue.empty()) {
            auto item = queue.front();
            queue.pop();
            
            if (!found && item->get_preferred_thread() == preferred_thread_id) {
                found = item; // Found preferred work
            } else {
                temp_queue.push(item); // Keep for later
            }
        }
        
        // Put back non-preferred items
        while (!temp_queue.empty()) {
            queue.push(temp_queue.front());
            temp_queue.pop();
        }
        
        return found;
    };
    
    // 1. Look for preferred work in priority queue
    auto work = find_preferred(priority_queue_);
    if (work) {
        work->set_preferred_thread(preferred_thread_id);
        return work;
    }
    
    // 2. Look for preferred work in regular queue  
    work = find_preferred(regular_queue_);
    if (work) {
        work->set_preferred_thread(preferred_thread_id);
        return work;
    }
    
    // 3. No preferred work - take any priority work
    if (!priority_queue_.empty()) {
        work = priority_queue_.front();
        priority_queue_.pop();
        work->set_preferred_thread(preferred_thread_id);
        return work;
    }
    
    // 4. Finally take any regular work
    if (!regular_queue_.empty()) {
        work = regular_queue_.front();
        regular_queue_.pop();
        work->set_preferred_thread(preferred_thread_id);
        return work;
    }
    
    return nullptr; // No work available
}

void EventDrivenScheduler::on_async_event_complete(std::shared_ptr<Goroutine> goroutine, bool is_timer) {
    // Check if goroutine is already running
    if (!goroutine->is_running()) {
        // Not running - schedule it immediately
        if (is_timer) {
            schedule_priority(goroutine);  // Timer callbacks are high priority
        } else {
            schedule_regular(goroutine);   // Regular async completion
        }
    }
    // If already running, the running thread will pick up the async result
}

void EventDrivenScheduler::notify_thread_available(int thread_id) {
    // This could be used for more sophisticated load balancing
    // For now, it's a no-op
}

void EventDrivenScheduler::clear_affinity_conflicts_for_ffi_binding(int old_thread_id) {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    
    int alternative_thread_id = find_least_loaded_thread();
    int goroutines_migrated = 0;
    
    // Helper to migrate affinities in a queue
    auto migrate_queue_affinities = [&](auto& queue) {
        std::queue<std::shared_ptr<Goroutine>> temp_queue;
        
        while (!queue.empty()) {
            auto goroutine = queue.front();
            queue.pop();
            
            if (goroutine->get_preferred_thread() == old_thread_id) {
                goroutine->set_preferred_thread(alternative_thread_id);
                goroutines_migrated++;
            }
            
            temp_queue.push(goroutine);
        }
        
        // Put everything back
        while (!temp_queue.empty()) {
            queue.push(temp_queue.front());
            temp_queue.pop();
        }
    };
    
    // Migrate affinities in both queues
    migrate_queue_affinities(priority_queue_);
    migrate_queue_affinities(regular_queue_);
    
    // Optional logging
    if (goroutines_migrated > 0) {
        std::cout << "Migrated " << goroutines_migrated 
                  << " goroutine affinities from thread " << old_thread_id 
                  << " to thread " << alternative_thread_id << std::endl;
    }
}

bool EventDrivenScheduler::try_wake_idle_thread(std::shared_ptr<Goroutine> goroutine) {
    // Try threads in order, preferring goroutine's preferred thread
    int preferred = goroutine->get_preferred_thread();
    
    // First try preferred thread if it exists
    if (preferred >= 0 && preferred < num_threads_) {
        if (thread_workers_[preferred]->try_assign_work(goroutine)) {
            return true;
        }
    }
    
    // Then try any idle thread
    for (int i = 0; i < num_threads_; ++i) {
        if (i == preferred) continue; // Already tried
        if (thread_workers_[i]->try_assign_work(goroutine)) {
            return true;
        }
    }
    
    return false; // No idle threads found
}

bool EventDrivenScheduler::try_wake_idle_thread_for_queued_work() {
    // This function specifically handles the race condition case
    // It tries to wake an idle thread to process queued work
    
    for (int i = 0; i < num_threads_; ++i) {
        if (thread_workers_[i]->try_assign_queued_work()) {
            return true; // Successfully woke thread to check queue
        }
    }
    
    return false; // No idle threads found
}

void EventDrivenScheduler::wake_threads_for_queued_work() {
    // Wake multiple threads if there's a lot of queued work
    // For now, just try to wake one
    try_wake_idle_thread_for_queued_work();
}

int EventDrivenScheduler::find_least_loaded_thread() {
    // Simple round-robin for now
    static std::atomic<int> counter{0};
    return counter.fetch_add(1) % num_threads_;
}

//=============================================================================
// FFI Integration Functions
//=============================================================================

extern "C" void* execute_ffi_call(Goroutine* current_goroutine, void* ffi_function, void* args) {
    if (current_goroutine->is_ffi_bound()) {
        // ZERO OVERHEAD PATH: Direct call on dedicated thread
        typedef void* (*FFIFunction)(void*);
        return reinterpret_cast<FFIFunction>(ffi_function)(args);
    } else {
        // FIRST FFI CALL: Bind goroutine to dedicated OS thread
        return migrate_to_ffi_thread(current_goroutine, ffi_function, args);
    }
}

extern "C" void* migrate_to_ffi_thread(Goroutine* goroutine, void* ffi_func, void* args) {
    // Get shared_ptr from raw pointer (this is tricky and would need proper implementation)
    auto goroutine_shared = goroutine->shared_from_this();
    
    // 1. Acquire dedicated OS thread from pool
    FFIThread* ffi_thread = EventDrivenScheduler::instance().acquire_ffi_thread();
    if (!ffi_thread) {
        // Fallback: execute on current thread (not optimal but works)
        typedef void* (*FFIFunction)(void*);
        return reinterpret_cast<FFIFunction>(ffi_func)(args);
    }
    
    // 2. Handle thread affinity conflicts
    int old_preferred_thread = goroutine->get_preferred_thread();
    if (old_preferred_thread != -1) {
        goroutine->clear_preferred_thread();
        EventDrivenScheduler::instance().clear_affinity_conflicts_for_ffi_binding(old_preferred_thread);
    }
    
    // 3. Copy goroutine's stack to native OS thread stack (simplified)
    void* native_stack = ffi_thread->get_native_stack();
    if (native_stack) {
        // In a full implementation, this would do proper stack migration
        adjust_stack_pointers(goroutine, native_stack);
    }
    
    // 4. Bind goroutine permanently to this OS thread
    goroutine->set_ffi_bound(true);
    goroutine->set_bound_ffi_thread(ffi_thread);
    ffi_thread->bind_goroutine(goroutine_shared);
    
    // 5. Execute first FFI call on native thread
    typedef void* (*FFIFunction)(void*);
    void* result = reinterpret_cast<FFIFunction>(ffi_func)(args);
    
    // 6. Continue execution on this OS thread
    ffi_thread->continue_execution(goroutine_shared);
    
    return result;
}

extern "C" bool is_goroutine_ffi_bound(Goroutine* goroutine) {
    return goroutine ? goroutine->is_ffi_bound() : false;
}

extern "C" void adjust_stack_pointers(Goroutine* goroutine, void* new_stack) {
    // This would adjust all stack pointers after migration
    // Simplified version for now
    if (goroutine && new_stack) {
        // Update stack base in context
        goroutine->get_context().stack_base = new_stack;
    }
}

//=============================================================================
// Runtime Integration Functions
//=============================================================================

extern "C" int64_t __gots_set_timeout_v2(void* function_address, int64_t delay_ms) {
    auto current = get_current_goroutine();
    if (current) {
        return current->add_timer(delay_ms, function_address, false);
    }
    return -1;
}

extern "C" int64_t __gots_set_interval_v2(void* function_address, int64_t delay_ms) {
    auto current = get_current_goroutine();
    if (current) {
        return current->add_timer(delay_ms, function_address, true);
    }
    return -1;
}

extern "C" bool __gots_clear_timeout_v2(int64_t timer_id) {
    auto current = get_current_goroutine();
    if (current) {
        current->cancel_timer(timer_id);
        return true;
    }
    return false;
}

extern "C" bool __gots_clear_interval_v2(int64_t timer_id) {
    return __gots_clear_timeout_v2(timer_id);
}

extern "C" int64_t __gots_add_async_handle_v2(int64_t type, void* handle_data) {
    auto current = get_current_goroutine();
    if (current) {
        auto async_op = std::make_shared<AsyncOperation>();
        async_op->type = static_cast<AsyncOpType>(type);
        async_op->handle_data = handle_data;
        return current->add_async_operation(async_op);
    }
    return -1;
}

extern "C" void __gots_complete_async_handle_v2(int64_t async_id) {
    auto current = get_current_goroutine();
    if (current) {
        current->complete_async_operation(async_id, nullptr);
    }
}

extern "C" void __gots_cancel_async_handle_v2(int64_t async_id) {
    // Cancel async operation
    AsyncManager::instance().complete_async_operation(async_id, nullptr);
}

extern "C" void __runtime_spawn_main_goroutine_v2(void* function_address) {
    // Create main goroutine
    typedef void (*MainFunction)();
    auto main_func = reinterpret_cast<MainFunction>(function_address);
    
    auto goroutine = std::make_shared<Goroutine>(g_next_goroutine_id.fetch_add(1), 
                                                 [main_func]() { main_func(); });
    
    // Initialize scheduler
    EventDrivenScheduler::instance().initialize();
    
    // Schedule main goroutine
    EventDrivenScheduler::instance().schedule_regular(goroutine);
}

extern "C" void __runtime_wait_for_main_goroutine_v2() {
    // This would wait for all goroutines to complete
    // For now, just sleep a bit
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
}

extern "C" void* __runtime_spawn_goroutine_v2(void* function_address) {
    typedef void (*GoroutineFunction)();
    auto func = reinterpret_cast<GoroutineFunction>(function_address);
    
    auto goroutine = std::make_shared<Goroutine>(g_next_goroutine_id.fetch_add(1),
                                                 [func]() { func(); });
    
    // Schedule the new goroutine
    EventDrivenScheduler::instance().schedule_regular(goroutine);
    
    // Return raw pointer (caller needs to manage lifetime properly)
    return goroutine.get();
}

// Version that accepts std::function for C++ code
std::shared_ptr<Goroutine> spawn_goroutine_v2(std::function<void()> func) {
    auto goroutine = std::make_shared<Goroutine>(g_next_goroutine_id.fetch_add(1), func);
    
    // Schedule the new goroutine
    EventDrivenScheduler::instance().schedule_regular(goroutine);
    
    return goroutine;
}

//=============================================================================
// Legacy Compatibility Functions
//=============================================================================

extern "C" void __new_goroutine_system_init() {
    // Initialize the V2 goroutine system
    EventDrivenScheduler::instance().initialize();
}

extern "C" void __new_goroutine_system_cleanup() {
    // Cleanup the V2 goroutine system
    EventDrivenScheduler::instance().shutdown();
}

extern "C" void __runtime_wait_for_main_goroutine() {
    // Wait for all goroutines to complete
    // This is a simplified version - in a full implementation,
    // we would wait for all scheduled goroutines to finish
    
    // For now, just wait a bit for any running goroutines
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Wait for thread pool to finish current work
    EventDrivenScheduler::instance().wait_for_completion();
}

// Legacy timer functions that redirect to V2 versions
extern "C" int64_t __gots_set_timeout(void* function_address, int64_t delay_ms) {
    return __gots_set_timeout_v2(function_address, delay_ms);
}

extern "C" int64_t __gots_set_interval(void* function_address, int64_t interval_ms) {
    return __gots_set_interval_v2(function_address, interval_ms);
}

extern "C" void __gots_clear_timeout(int64_t timer_id) {
    __gots_clear_timeout_v2(timer_id);
}

// Await goroutine functions - spawn and wait for result
extern "C" void* __goroutine_spawn_and_wait_direct(void* function_address) {
    typedef int64_t (*GoroutineFunction)();
    auto func = reinterpret_cast<GoroutineFunction>(function_address);
    
    auto goroutine = std::make_shared<Goroutine>(g_next_goroutine_id.fetch_add(1),
                                                 [func]() { 
                                                     int64_t result = func();
                                                     return result;
                                                 });
    
    // Schedule the goroutine
    EventDrivenScheduler::instance().schedule_regular(goroutine);
    
    // Wait for completion and return result
    // This is a simplified implementation - in full version would use proper synchronization
    while (goroutine->get_state() != GoroutineState::COMPLETED) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    
    // For now, return the goroutine pointer - in full implementation would return actual result
    return goroutine.get();
}

extern "C" void* __goroutine_spawn_and_wait_fast(void* func_address) {
    // High-performance path: Direct function address execution
    if (func_address) {
        // Call the compiled function directly
        typedef int64_t (*GoroutineFunction)();
        auto func = reinterpret_cast<GoroutineFunction>(func_address);
        
        auto goroutine = std::make_shared<Goroutine>(g_next_goroutine_id.fetch_add(1),
                                                     [func]() -> void { 
                                                         int64_t result = func();
                                                         // Note: Cannot set result from inside lambda as we don't have goroutine ref here
                                                         // This will be fixed in proper implementation
                                                     });
        
        // Schedule the goroutine
        EventDrivenScheduler::instance().schedule_regular(goroutine);
        
        // Wait for completion and return result
        while (goroutine->get_state() != GoroutineState::COMPLETED) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        
        return goroutine.get();
    }
    
    // Fallback
    return nullptr;
}

extern "C" void* __goroutine_spawn_direct(void* function_address) {
    return __runtime_spawn_goroutine_v2(function_address);
}
