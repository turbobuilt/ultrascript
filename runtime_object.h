#pragma once

#include <cstdint>
#include <unordered_map>
#include <string>
#include "stdlib/torch/torch_ffi.h"

namespace ultraScript {

// Runtime object structure designed for JIT optimization
// The JIT compiler recognizes these constant paths and converts them to direct function calls
// For example: runtime.ti    // System modules - isolated module instances per namespace
    ChildProcessObject child_process;
    ConsoleObject console;
    CryptoObject crypto;
    
    // High-performance timer object
    TimerObject timer;
    
    // High-performance VM object
    VMObject vm;
    
    // High-performance buffer object
    BufferObject buffer;
    
    // High-performance torch object for deep learning
    TorchObject torch;
};

// Macro for JIT to use when generating code
// This allows the JIT to emit a direct CALL instruction instead of property lookups
#define RUNTIME_METHOD_ADDRESS(obj, method) 
    ((void*)&global_runtime->obj.method)ow() -> __runtime_time_now_millis() with zero overhead

struct RuntimeObject {
    // Each sub-object is a constant structure with function pointers
    // The JIT can resolve these at compile time
    
    struct TimeObject {
        static constexpr const char* OBJECT_NAME = "time";
        
        // Function pointers stored as raw addresses for maximum performance
        void* now_millis;        // runtime.time.now() -> returns milliseconds
        void* now_nanos;         // runtime.time.nowNanos() -> returns nanoseconds  
        void* timezone_offset;   // runtime.time.timezoneOffset()
        void* sleep;            // runtime.time.sleep(ms)
        void* sleep_nanos;      // runtime.time.sleepNanos(ns)
    };
    
    struct ProcessObject {
        static constexpr const char* OBJECT_NAME = "process";
        
        void* pid;
        void* ppid;
        void* uid;
        void* gid;
        void* cwd;
        void* chdir;
        void* exit;
        void* argv;
        void* env;
        void* platform;
        void* arch;
        void* version;
        void* memoryUsage;
        void* cpuUsage;
    };
    
    struct FSObject {
        static constexpr const char* OBJECT_NAME = "fs";
        
        // Sync operations
        void* open;
        void* close;
        void* read;
        void* write;
        void* readFile;
        void* writeFile;
        void* exists;
        void* stat;
        void* mkdir;
        void* rmdir;
        void* unlink;
        void* rename;
        void* readdir;
        
        // Async operations (return promises)
        void* openAsync;
        void* readAsync;
        void* writeAsync;
        void* readFileAsync;
        void* writeFileAsync;
    };
    
    struct NetObject {
        static constexpr const char* OBJECT_NAME = "net";
        
        void* createServer;
        void* connect;
        void* socket;
        void* bind;
        void* listen;
        void* accept;
        void* send;
        void* recv;
    };
    
    struct HTTPObject {
        static constexpr const char* OBJECT_NAME = "http";
        
        // HTTP server functions
        void* createServer;         // runtime.http.createServer(handler)
        void* serverListen;         // runtime.http.serverListen(server, port, host)
        void* serverClose;          // runtime.http.serverClose(server)
        
        // HTTP client functions
        void* request;              // runtime.http.request(options, callback)
        void* get;                  // runtime.http.get(url, callback)
        void* post;                 // runtime.http.post(url, data, callback)
        
        // HTTP request object methods
        void* requestGetMethod;     // req.method
        void* requestGetUrl;        // req.url
        void* requestGetHeader;     // req.getHeader(name)
        void* requestGetBody;       // req.body
        
        // HTTP response object methods
        void* responseSetStatus;    // res.setStatus(code)
        void* responseSetHeader;    // res.setHeader(name, value)
        void* responseWrite;        // res.write(data)
        void* responseEnd;          // res.end(data)
        void* responseJson;         // res.json(data)
        void* responseHtml;         // res.html(data)
        void* responseSendFile;     // res.sendFile(path)
    };
    
    struct CryptoObject {
        static constexpr const char* OBJECT_NAME = "crypto";
        
        void* randomBytes;
        void* createHash;
        void* createHmac;
        void* pbkdf2;
        void* sign;
        void* verify;
    };
    
    struct BufferObject {
        static constexpr const char* OBJECT_NAME = "buffer";
        
        void* alloc;
        void* from;
        void* concat;
        void* isBuffer;
    };
    
    struct TorchObject {
        static constexpr const char* OBJECT_NAME = "torch";
        
        // Core functions
        void* init;
        void* cleanup;
        void* version;
        void* set_seed;
        void* manual_seed;
        
        // Device functions
        void* device_cpu;
        void* device_cuda;
        void* cuda_is_available;
        void* cuda_device_count;
        void* cuda_empty_cache;
        
        // Data type functions
        void* dtype_float32;
        void* dtype_float64;
        void* dtype_int32;
        void* dtype_int64;
        void* dtype_bool;
        
        // Tensor creation functions
        void* tensor_empty;
        void* tensor_zeros;
        void* tensor_ones;
        void* tensor_randn;
        void* tensor_rand;
        void* tensor_from_blob;
        void* tensor_from_array_float32;
        void* tensor_from_array_float64;
        void* tensor_from_array_int32;
        void* tensor_from_array_int64;
        
        // Tensor property functions
        void* tensor_ndim;
        void* tensor_size;
        void* tensor_numel;
        void* tensor_dtype;
        void* tensor_device;
        void* tensor_data_ptr;
        
        // Tensor arithmetic operations
        void* tensor_add;
        void* tensor_sub;
        void* tensor_mul;
        void* tensor_div;
        void* tensor_matmul;
        void* tensor_add_scalar;
        void* tensor_sub_scalar;
        void* tensor_mul_scalar;
        void* tensor_div_scalar;
        
        // Tensor mathematical functions
        void* tensor_sin;
        void* tensor_cos;
        void* tensor_tan;
        void* tensor_exp;
        void* tensor_log;
        void* tensor_sqrt;
        void* tensor_abs;
        void* tensor_neg;
        
        // Tensor shape operations
        void* tensor_reshape;
        void* tensor_view;
        void* tensor_transpose;
        void* tensor_permute;
        void* tensor_squeeze;
        void* tensor_unsqueeze;
        
        // Tensor memory management
        void* tensor_free;
        void* tensor_clone;
        void* tensor_detach;
        void* tensor_to;
        
        // Neural network operations
        void* nn_linear;
        void* nn_conv2d;
        void* nn_relu;
        void* nn_sigmoid;
        void* nn_softmax;
        void* nn_cross_entropy;
        
        // Autograd operations
        void* tensor_backward;
        void* tensor_grad;
        void* tensor_set_requires_grad;
        void* tensor_requires_grad;
        
        // I/O operations
        void* save_tensor;
        void* load_tensor;
        
        // Utilities
        void* print_tensor;
        void* last_error;
        void* clear_error;
    };
    
    struct OSObject {
        static constexpr const char* OBJECT_NAME = "os";
        
        void* hostname;
        void* type;
        void* platform;
        void* release;
        void* arch;
        void* cpus;
        void* networkInterfaces;
        void* homedir;
        void* tmpdir;
        void* uptime;
        void* freemem;
        void* totalmem;
    };
    
    struct PathObject {
        static constexpr const char* OBJECT_NAME = "path";
        
        void* basename;
        void* dirname;
        void* extname;
        void* join;
        void* resolve;
        void* relative;
        void* normalize;
        void* parse;
        void* format;
        void* isAbsolute;
        void* sep;
        void* delimiter;
    };
    
    struct ConsoleObject {
        static constexpr const char* OBJECT_NAME = "console";
        
        void* log;
        void* error;
        void* warn;
        void* info;
        void* debug;
        void* trace;
        void* time;
        void* timeEnd;
        void* profile;
        void* profileEnd;
    };
    
    struct JITObject {
        static constexpr const char* OBJECT_NAME = "jit";
        
        void* compile;
        void* optimize;
        void* stats;
        void* disassemble;
    };
    
    struct GCObject {
        static constexpr const char* OBJECT_NAME = "gc";
        
        void* collect;
        void* heapSize;
        void* heapUsed;
        void* nextGC;
    };
    
    struct LockObject {
        static constexpr const char* OBJECT_NAME = "lock";
        
        void* create;           // runtime.lock.create() -> new Lock()
    };
    
    // Runtime object layout - designed for cache efficiency
    TimeObject time;
    ProcessObject process;
    FSObject fs;
    NetObject net;
    HTTPObject http;
    CryptoObject crypto;
    BufferObject buffer;
    OSObject os;
    PathObject path;
    ConsoleObject console;
    JITObject jit;
    GCObject gc;
    LockObject lock;
    
    // Direct function pointers for frequently used operations
    void* eval;
    void* compile;
    void* typeof;
    void* instanceof;
    
    // Goroutine functions
    void* go;
    void* goMap;
};

// Global runtime instance - constant after initialization
extern RuntimeObject* global_runtime;

// Registration info for JIT optimization
struct RuntimeMethodInfo {
    const char* object_path;     // e.g., "time.now"
    void* function_pointer;      // Direct function pointer
    bool is_async;              // Returns a promise
    int arg_count;              // Expected argument count (-1 for variadic)
};

// Registry of all runtime methods for JIT optimization
extern std::unordered_map<std::string, RuntimeMethodInfo> runtime_method_registry;

// Initialize the runtime object and method registry
void initialize_runtime_object();

// Helper for JIT to resolve runtime calls at compile time
inline void* resolve_runtime_method(const char* object_name, const char* method_name) {
    // This function is designed to be called at JIT compile time
    // It returns the direct function pointer for inlining
    
    if (strcmp(object_name, "time") == 0) {
        if (strcmp(method_name, "now") == 0) return global_runtime->time.now_millis;
        if (strcmp(method_name, "nowNanos") == 0) return global_runtime->time.now_nanos;
        if (strcmp(method_name, "sleep") == 0) return global_runtime->time.sleep;
        // ... etc
    } else if (strcmp(object_name, "fs") == 0) {
        if (strcmp(method_name, "readFile") == 0) return global_runtime->fs.readFile;
        if (strcmp(method_name, "writeFile") == 0) return global_runtime->fs.writeFile;
        // ... etc
    } else if (strcmp(object_name, "http") == 0) {
        if (strcmp(method_name, "createServer") == 0) return global_runtime->http.createServer;
        if (strcmp(method_name, "serverListen") == 0) return global_runtime->http.serverListen;
        if (strcmp(method_name, "serverClose") == 0) return global_runtime->http.serverClose;
        if (strcmp(method_name, "request") == 0) return global_runtime->http.request;
        if (strcmp(method_name, "get") == 0) return global_runtime->http.get;
        if (strcmp(method_name, "post") == 0) return global_runtime->http.post;
        // ... etc
    } else if (strcmp(object_name, "lock") == 0) {
        if (strcmp(method_name, "create") == 0) return global_runtime->lock.create;
        // ... etc
    }
    // ... more objects
    
    return nullptr;
}

// Macro for JIT to use when generating code
// This allows the JIT to emit a direct CALL instruction instead of property lookups
#define RUNTIME_METHOD_ADDRESS(obj, method) \
    ((void*)&global_runtime->obj.method)

} // namespace ultraScript