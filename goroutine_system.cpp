#include "goroutine_system.h"
#include "goroutine_advanced.h"
#include <iostream>
#include <algorithm>

namespace ultraScript {

// Thread-local current goroutine
thread_local std::shared_ptr<Goroutine> current_goroutine = nullptr;

// Global cancelled timers
std::unordered_set<int64_t> g_cancelled_timers;
std::mutex g_cancelled_timers_mutex;

// Global goroutine counter
std::atomic<int64_t> g_active_goroutine_count{0};

// Goroutine implementation
Goroutine::Goroutine(int64_t id, std::function<void()> task, std::shared_ptr<Goroutine> parent)
    : id_(id), state_(GoroutineState::CREATED), task_(std::move(task)), parent_(parent), 
      is_main_goroutine_(false) {
    
    // Increment parent's child count if we have a parent
    if (parent) {
        parent->increment_child_count();
    }
}

Goroutine::~Goroutine() {
    // Signal exit and ensure clean shutdown
    should_exit_.store(true);
    
    // Wake up event loop without holding any locks
    event_loop_cv_.notify_all();
    
    // Give the thread time to exit the event loop cleanly
    if (thread_.joinable()) {
        thread_.join();
    }
    
    // Now safely unregister from scheduler after thread has completely finished
    GoroutineScheduler::instance().unregister_goroutine(id_);
}

void Goroutine::start() {
    state_ = GoroutineState::RUNNING;
    thread_ = std::thread(&Goroutine::run, this);
}

void Goroutine::increment_child_count() {
    child_count_.fetch_add(1);
    
    // Add child as async operation to keep event loop running
    child_async_op_id_ = add_async_operation(AsyncOpType::CHILD_GOROUTINE);
}

void Goroutine::decrement_child_count() {
    int old_count = child_count_.fetch_sub(1);
    
    // If no more children, complete the child async operation
    if (old_count == 1) { // old_count was 1, now it's 0
        complete_async_operation(child_async_op_id_);
    }
    
    // Trigger event loop to recheck active operations
    trigger_event_loop();
}

void Goroutine::run() {
    // Set thread-local current goroutine
    current_goroutine = shared_from_this();
    
    
    try {
        // Execute the main task
        task_();
    } catch (const std::exception& e) {
        std::cerr << "ERROR: Goroutine " << id_ << " exception: " << e.what() << std::endl;
    } catch (...) {
        std::cerr << "ERROR: Goroutine " << id_ << " unknown exception" << std::endl;
    }
    
    // After main code execution, run the event loop like Node.js
    // This handles timers, children, server handles, etc.
    run_event_loop();
    
    state_ = GoroutineState::COMPLETED;
    
    // Notify parent that we're done
    on_child_completed();
    
    // If this is the main goroutine, signal completion
    if (is_main_goroutine_) {
        GoroutineScheduler::instance().signal_main_goroutine_completion();
    }
    
    // NOTE: unregister_goroutine() is now called in destructor after thread_.join()
    // to prevent deadlock
}

// Node.js-style event loop - handles ALL async operations
void Goroutine::run_event_loop() {
    
    while (!should_exit_.load()) {
        std::unique_lock<std::mutex> lock(event_loop_mutex_);
        
        // Check if we have any reason to keep running
        bool has_work = has_active_operations();
        
        if (!has_work) {
            break;
        }
        
        // Clean up cancelled timers first
        std::vector<Timer> remaining_timers;
        while (!timer_queue_.empty()) {
            Timer timer = timer_queue_.top();
            timer_queue_.pop();
            
            {
                std::lock_guard<std::mutex> cancel_lock(g_cancelled_timers_mutex);
                if (g_cancelled_timers.find(timer.id) == g_cancelled_timers.end()) {
                    remaining_timers.push_back(timer);
                } else {
                    g_cancelled_timers.erase(timer.id);
                }
            }
        }
        
        // Re-add non-cancelled timers
        for (const auto& timer : remaining_timers) {
            timer_queue_.push(timer);
        }
        
        // Find next timer time if we have any timers
        std::chrono::steady_clock::time_point next_wake_time;
        bool has_timer = !timer_queue_.empty();
        if (has_timer) {
            next_wake_time = timer_queue_.top().execute_time;
        }
        
        auto now = std::chrono::steady_clock::now();
        
        // Check for ready timers and collect them
        std::vector<Timer> ready_timers;
        while (!timer_queue_.empty() && timer_queue_.top().execute_time <= now) {
            Timer timer = timer_queue_.top();
            timer_queue_.pop();
            ready_timers.push_back(timer);
        }
        
        // Execute timers outside the lock to prevent deadlock
        if (!ready_timers.empty()) {
            lock.unlock();
            
            for (const auto& timer : ready_timers) {
                
                // Reschedule if interval timer (reacquire lock briefly)
                if (timer.is_interval) {
                    {
                        std::lock_guard<std::mutex> relock(event_loop_mutex_);
                        Timer rescheduled_timer = timer;
                        rescheduled_timer.execute_time = now + std::chrono::milliseconds(timer.interval_ms);
                        timer_queue_.push(rescheduled_timer);
                    }
                }
                
                try {
                    typedef void (*TimerCallback)();
                    TimerCallback callback = reinterpret_cast<TimerCallback>(timer.function_address);
                    callback();
                } catch (const std::exception& e) {
                    std::cerr << "ERROR: Timer " << timer.id << " exception: " << e.what() << std::endl;
                } catch (...) {
                    std::cerr << "ERROR: Timer " << timer.id << " unknown exception" << std::endl;
                }
            }
            
            lock.lock();
            now = std::chrono::steady_clock::now();
        }
        
        // Wait for next event (timer or trigger)
        if (has_timer) {
            event_loop_cv_.wait_until(lock, next_wake_time, [this] {
                return should_exit_.load();
            });
        } else {
            event_loop_cv_.wait(lock, [this] {
                return should_exit_.load() || !has_active_operations();
            });
        }
    }
    
}

// Async operation management
int64_t Goroutine::add_async_operation(AsyncOpType type, void* handle_data) {
    std::lock_guard<std::mutex> lock(event_loop_mutex_);
    int64_t async_id = next_async_id_.fetch_add(1);
    async_operations_[async_id] = AsyncOperation(async_id, type, handle_data);
    
    trigger_event_loop();
    return async_id;
}

void Goroutine::complete_async_operation(int64_t async_id) {
    std::lock_guard<std::mutex> lock(event_loop_mutex_);
    auto it = async_operations_.find(async_id);
    if (it != async_operations_.end()) {
        async_operations_.erase(it);
        trigger_event_loop();
    }
}

void Goroutine::cancel_async_operation(int64_t async_id) {
    std::lock_guard<std::mutex> lock(event_loop_mutex_);
    auto it = async_operations_.find(async_id);
    if (it != async_operations_.end()) {
        it->second.is_active = false;
        trigger_event_loop();
    }
}

bool Goroutine::has_active_operations() const {
    // Check timers
    if (!timer_queue_.empty()) return true;
    
    // Check children
    if (child_count_.load() > 0) return true;
    
    // Check async operations
    for (const auto& op : async_operations_) {
        if (op.second.is_active) return true;
    }
    
    return false;
}

void Goroutine::trigger_event_loop() {
    // Wake up the event loop immediately
    event_loop_cv_.notify_one();
}

int64_t Goroutine::add_timer(int64_t delay_ms, void* function_address, bool is_interval) {
    Timer timer;
    timer.id = GoroutineScheduler::instance().get_next_timer_id();
    timer.execute_time = std::chrono::steady_clock::now() + std::chrono::milliseconds(delay_ms);
    timer.function_address = function_address;
    timer.is_interval = is_interval;
    timer.interval_ms = delay_ms;
    
    {
        std::lock_guard<std::mutex> lock(event_loop_mutex_);
        timer_queue_.push(timer);
    }
    
    trigger_event_loop();
    return timer.id;
}

bool Goroutine::cancel_timer(int64_t timer_id) {
    std::lock_guard<std::mutex> lock(g_cancelled_timers_mutex);
    g_cancelled_timers.insert(timer_id);
    trigger_event_loop();
    return true;
}

void Goroutine::signal_exit() {
    should_exit_.store(true);
    trigger_event_loop();
}

void Goroutine::set_as_main_goroutine() {
    is_main_goroutine_ = true;
}

void Goroutine::on_child_completed() {
    // Notify parent that we're done
    if (auto parent = parent_.lock()) {
        parent->decrement_child_count();
    }
}

// Advanced features implementation
void Goroutine::reset_task(std::function<void()> new_task) {
    // Can only reset if not currently running
    if (state_ != GoroutineState::RUNNING) {
        task_ = std::move(new_task);
        state_ = GoroutineState::CREATED;
        should_exit_.store(false);
    } else {
        std::cerr << "ERROR: Cannot reset task of running goroutine" << std::endl;
    }
}

void* Goroutine::allocate_shared_memory(size_t size) {
    void* ptr = g_shared_memory_pool.allocate(size);
    // std::cout << " bytes of shared memory at " << ptr << std::endl;
    return ptr;
}

void Goroutine::share_memory(void* ptr, std::shared_ptr<Goroutine> target) {
    if (ptr && target) {
        g_shared_memory_pool.add_ref(ptr);
        // std::cout << " with goroutine " << target->get_id() << std::endl;
    }
}

void Goroutine::release_shared_memory(void* ptr) {
    if (ptr) {
        g_shared_memory_pool.release(ptr);
    }
}

// Scheduler implementation
GoroutineScheduler::GoroutineScheduler() {
}

GoroutineScheduler::~GoroutineScheduler() {
    // Clean shutdown
}

GoroutineScheduler& GoroutineScheduler::instance() {
    // Use heap allocation to avoid static destruction order issues
    static GoroutineScheduler* instance = new GoroutineScheduler();
    return *instance;
}

std::shared_ptr<Goroutine> GoroutineScheduler::spawn(std::function<void()> task, std::shared_ptr<Goroutine> parent) {
    int64_t id = next_goroutine_id_.fetch_add(1);
    
    // If no parent specified, use current goroutine as parent
    if (!parent && current_goroutine) {
        parent = current_goroutine;
    }
    
    auto goroutine = std::make_shared<Goroutine>(id, std::move(task), parent);
    
    register_goroutine(goroutine);
    
    // Start the goroutine on its own thread
    goroutine->start();
    
    return goroutine;
}

void GoroutineScheduler::spawn_main_goroutine(std::function<void()> task) {
    int64_t id = 0; // Main goroutine gets ID 0
    
    main_goroutine_ = std::make_shared<Goroutine>(id, std::move(task), nullptr);
    main_goroutine_->set_as_main_goroutine();
    
    register_goroutine(main_goroutine_);
    
    // Start the main goroutine
    main_goroutine_->start();
    
}

void GoroutineScheduler::wait_for_main_goroutine() {
    
    // Wait for main goroutine to signal completion
    std::unique_lock<std::mutex> lock(main_completion_mutex_);
    main_completion_cv_.wait(lock, [this] { return main_goroutine_completed_.load(); });
    
}

void GoroutineScheduler::signal_main_goroutine_completion() {
    std::lock_guard<std::mutex> lock(main_completion_mutex_);
    main_goroutine_completed_.store(true);
    main_completion_cv_.notify_one();
}

void GoroutineScheduler::register_goroutine(std::shared_ptr<Goroutine> g) {
    std::lock_guard<std::mutex> lock(goroutines_mutex_);
    goroutines_[g->get_id()] = g;
}

void GoroutineScheduler::unregister_goroutine(int64_t id) {
    std::lock_guard<std::mutex> lock(goroutines_mutex_);
    goroutines_.erase(id);
}

size_t GoroutineScheduler::get_active_count() const {
    std::lock_guard<std::mutex> lock(goroutines_mutex_);
    return goroutines_.size();
}

// C interface implementation
extern "C" {

int64_t __gots_set_timeout(void* function_address, int64_t delay_ms) {
    if (!current_goroutine) {
        std::cerr << "ERROR: setTimeout called outside goroutine context" << std::endl;
        return -1;
    }
    
    return current_goroutine->add_timer(delay_ms, function_address, false);
}

int64_t __gots_set_interval(void* function_address, int64_t delay_ms) {
    if (!current_goroutine) {
        std::cerr << "ERROR: setInterval called outside goroutine context" << std::endl;
        return -1;
    }
    
    return current_goroutine->add_timer(delay_ms, function_address, true);
}

bool __gots_clear_timeout(int64_t timer_id) {
    // Timer cancellation works globally
    std::lock_guard<std::mutex> lock(g_cancelled_timers_mutex);
    g_cancelled_timers.insert(timer_id);
    return true;
}

bool __gots_clear_interval(int64_t timer_id) {
    return __gots_clear_timeout(timer_id);
}

// Async operation C interface
int64_t __gots_add_async_handle(int64_t type, void* handle_data) {
    if (!current_goroutine) {
        std::cerr << "ERROR: add_async_handle called outside goroutine context" << std::endl;
        return -1;
    }
    
    return current_goroutine->add_async_operation(static_cast<AsyncOpType>(type), handle_data);
}

void __gots_complete_async_handle(int64_t async_id) {
    if (!current_goroutine) {
        std::cerr << "ERROR: complete_async_handle called outside goroutine context" << std::endl;
        return;
    }
    
    current_goroutine->complete_async_operation(async_id);
}

void __gots_cancel_async_handle(int64_t async_id) {
    if (!current_goroutine) {
        std::cerr << "ERROR: cancel_async_handle called outside goroutine context" << std::endl;
        return;
    }
    
    current_goroutine->cancel_async_operation(async_id);
}

void __runtime_spawn_main_goroutine(void* function_address) {
    auto task = [function_address]() {
        typedef int (*FuncType)();
        FuncType func = reinterpret_cast<FuncType>(function_address);
        int result = func();
    };
    
    GoroutineScheduler::instance().spawn_main_goroutine(task);
}

void __runtime_wait_for_main_goroutine() {
    GoroutineScheduler::instance().wait_for_main_goroutine();
}

} // extern "C"

std::shared_ptr<Goroutine> spawn_goroutine(std::function<void()> task) {
    return GoroutineScheduler::instance().spawn(task, current_goroutine);
}

} // namespace ultraScript

// Timer helper functions for compatibility - need C linkage
extern "C" {
int64_t create_timer_new(int64_t delay_ms, void* callback, bool is_interval) {
    if (is_interval) {
        return ultraScript::__gots_set_interval(callback, delay_ms);
    } else {
        return ultraScript::__gots_set_timeout(callback, delay_ms);
    }
}

bool cancel_timer_new(int64_t timer_id) {
    return ultraScript::__gots_clear_timeout(timer_id);
}

void __new_goroutine_system_init() {
    // Initialize the goroutine system if needed
    // Most initialization happens automatically via singletons
}

void __new_goroutine_system_cleanup() {
    // Cleanup the goroutine system
    // Most cleanup happens automatically in destructors
}

} // extern "C"