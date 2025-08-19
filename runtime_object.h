
#pragma once

#include <cstdint>
#include <unordered_map>
#include <string>
#include <cstring>



struct TimeObject {
    static constexpr const char* OBJECT_NAME = "time";
    void* now_millis;
    void* now_nanos;
    void* timezone_offset;
    void* sleep;
    void* sleep_nanos;
};

struct DateObject {
    static constexpr const char* OBJECT_NAME = "date";
    void* constructor;
    void* now;
    void* getTime;
    void* setTime;
    void* toISOString;
    void* toLocaleString;
    void* getFullYear;
    void* getMonth;
    void* getDate;
    void* getDay;
    void* getHours;
    void* getMinutes;
    void* getSeconds;
    void* getMilliseconds;
    void* setFullYear;
    void* setMonth;
    void* setDate;
    void* setHours;
    void* setMinutes;
    void* setSeconds;
    void* setMilliseconds;
    void* add;        // add(amount, unit)
    void* subtract;   // subtract(amount, unit)
    void* format;     // format(formatString)
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
    void* createServer;
    void* serverListen;
    void* serverClose;
    void* request;
    void* get;
    void* post;
    void* requestGetMethod;
    void* requestGetUrl;
    void* requestGetHeader;
    void* requestGetBody;
    void* responseSetStatus;
    void* responseSetHeader;
    void* responseWrite;
    void* responseEnd;
    void* responseJson;
    void* responseHtml;
    void* responseSendFile;
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
    void* init;
    void* cleanup;
    void* version;
    void* set_seed;
    void* manual_seed;
    void* device_cpu;
    void* device_cuda;
    void* cuda_is_available;
    void* cuda_device_count;
    void* cuda_empty_cache;
    void* dtype_float32;
    void* dtype_float64;
    void* dtype_int32;
    void* dtype_int64;
    void* dtype_bool;
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
    void* tensor_ndim;
    void* tensor_size;
    void* tensor_numel;
    void* tensor_dtype;
    void* tensor_device;
    void* tensor_data_ptr;
    void* tensor_add;
    void* tensor_sub;
    void* tensor_mul;
    void* tensor_div;
    void* tensor_matmul;
    void* tensor_add_scalar;
    void* tensor_sub_scalar;
    void* tensor_mul_scalar;
    void* tensor_div_scalar;
    void* tensor_sin;
    void* tensor_cos;
    void* tensor_tan;
    void* tensor_exp;
    void* tensor_log;
    void* tensor_sqrt;
    void* tensor_abs;
    void* tensor_neg;
    void* tensor_reshape;
    void* tensor_view;
    void* tensor_transpose;
    void* tensor_permute;
    void* tensor_squeeze;
    void* tensor_unsqueeze;
    void* tensor_free;
    void* tensor_clone;
    void* tensor_detach;
    void* tensor_to;
    void* nn_linear;
    void* nn_conv2d;
    void* nn_relu;
    void* nn_sigmoid;
    void* nn_softmax;
    void* nn_cross_entropy;
    void* tensor_backward;
    void* tensor_grad;
    void* tensor_set_requires_grad;
    void* tensor_requires_grad;
    void* save_tensor;
    void* load_tensor;
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
    void* create;
};

// Main runtime object
struct RuntimeObject {
    std::unordered_map<std::string, void*> function_registry;
    TimeObject time;
    DateObject date;  // Add DateObject to runtime
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
    void* eval;
    void* compile;
    void* typeof;
    void* instanceof;
    void* go;
    void* goMap;
    void initialize() {
        // Basic initialization
    }
};

extern RuntimeObject* global_runtime;

struct RuntimeMethodInfo {
    const char* object_path;
    void* function_pointer;
    bool is_async;
    int arg_count;
};

extern std::unordered_map<std::string, RuntimeMethodInfo> runtime_method_registry;

void initialize_runtime_object();

inline void* resolve_runtime_method(const char* object_name, const char* method_name) {
    if (!global_runtime) return nullptr;
    if (strcmp(object_name, "time") == 0) {
        if (strcmp(method_name, "now") == 0) return global_runtime->time.now_millis;
        if (strcmp(method_name, "nowNanos") == 0) return global_runtime->time.now_nanos;
        if (strcmp(method_name, "sleep") == 0) return global_runtime->time.sleep;
    } else if (strcmp(object_name, "fs") == 0) {
        if (strcmp(method_name, "readFile") == 0) return global_runtime->fs.readFile;
        if (strcmp(method_name, "writeFile") == 0) return global_runtime->fs.writeFile;
    } else if (strcmp(object_name, "http") == 0) {
        if (strcmp(method_name, "createServer") == 0) return global_runtime->http.createServer;
        if (strcmp(method_name, "serverListen") == 0) return global_runtime->http.serverListen;
        if (strcmp(method_name, "serverClose") == 0) return global_runtime->http.serverClose;
        if (strcmp(method_name, "request") == 0) return global_runtime->http.request;
        if (strcmp(method_name, "get") == 0) return global_runtime->http.get;
        if (strcmp(method_name, "post") == 0) return global_runtime->http.post;
    } else if (strcmp(object_name, "lock") == 0) {
        if (strcmp(method_name, "create") == 0) return global_runtime->lock.create;
    }
    return nullptr;
}

#define RUNTIME_METHOD_ADDRESS(obj, method) \
    ((void*)&global_runtime->obj.method)

