#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <memory>
#include <atomic>
#include <mutex>
#include <thread>
#include <queue>
#include <condition_variable>

namespace ultraScript {

// Forward declarations
class HTTPRequest;
class HTTPResponse;
class HTTPServer;

// HTTP status codes
enum class HTTPStatus : int {
    OK = 200,
    CREATED = 201,
    NO_CONTENT = 204,
    BAD_REQUEST = 400,
    UNAUTHORIZED = 401,
    FORBIDDEN = 403,
    NOT_FOUND = 404,
    METHOD_NOT_ALLOWED = 405,
    INTERNAL_SERVER_ERROR = 500,
    NOT_IMPLEMENTED = 501,
    BAD_GATEWAY = 502,
    SERVICE_UNAVAILABLE = 503
};

// HTTP methods
enum class HTTPMethod {
    GET,
    POST,
    PUT,
    DELETE,
    HEAD,
    OPTIONS,
    PATCH,
    UNKNOWN
};

// High-performance HTTP request object
class HTTPRequest {
private:
    HTTPMethod method_;
    std::string url_;
    std::string path_;
    std::string query_string_;
    std::unordered_map<std::string, std::string> headers_;
    std::unordered_map<std::string, std::string> query_params_;
    std::string body_;
    int socket_fd_;

public:
    HTTPRequest(int socket_fd);
    ~HTTPRequest();

    // Parse HTTP request from socket
    bool parse(const char* raw_request, size_t length);
    
    // Getters
    HTTPMethod method() const { return method_; }
    const std::string& url() const { return url_; }
    const std::string& path() const { return path_; }
    const std::string& query_string() const { return query_string_; }
    const std::string& body() const { return body_; }
    int socket_fd() const { return socket_fd_; }
    
    // Header access
    std::string get_header(const std::string& name) const;
    bool has_header(const std::string& name) const;
    const std::unordered_map<std::string, std::string>& headers() const { return headers_; }
    
    // Query parameter access
    std::string get_query_param(const std::string& name) const;
    bool has_query_param(const std::string& name) const;
    const std::unordered_map<std::string, std::string>& query_params() const { return query_params_; }
    
private:
    void parse_url();
    void parse_query_string();
    HTTPMethod parse_method(const std::string& method_str);
};

// High-performance HTTP response object
class HTTPResponse {
private:
    HTTPStatus status_;
    std::unordered_map<std::string, std::string> headers_;
    std::string body_;
    int socket_fd_;
    bool headers_sent_;
    bool finished_;

public:
    HTTPResponse(int socket_fd);
    ~HTTPResponse();

    // Status and headers
    void set_status(HTTPStatus status) { status_ = status; }
    void set_header(const std::string& name, const std::string& value);
    void write_head(HTTPStatus status, const std::unordered_map<std::string, std::string>& headers = {});
    
    // Response body
    void write(const std::string& data);
    void write(const char* data, size_t length);
    void end(const std::string& data = "");
    
    // Convenience methods
    void json(const std::string& json_data);
    void html(const std::string& html_data);
    void send_file(const std::string& file_path);
    
    // Status
    bool is_finished() const { return finished_; }
    bool headers_sent() const { return headers_sent_; }
    
    // Public helper for status text
    static std::string get_status_text(HTTPStatus status);

private:
    void send_headers();
};

// HTTP server configuration
struct HTTPServerConfig {
    int port = 8080;
    std::string host = "0.0.0.0";
    int backlog = 128;
    int max_connections = 1000;
    int thread_pool_size = 8;
    int keep_alive_timeout = 30; // seconds
    bool enable_cors = false;
    size_t max_request_size = 1024 * 1024; // 1MB
    size_t max_header_size = 8192; // 8KB
};

// Request handler type
using HTTPRequestHandler = std::function<void(HTTPRequest&, HTTPResponse&)>;

// High-performance HTTP server optimized for goroutines
class HTTPServer {
private:
    HTTPServerConfig config_;
    HTTPRequestHandler request_handler_;
    std::atomic<bool> running_;
    std::atomic<bool> stopping_;
    int server_socket_;
    
    // Thread pool for handling connections
    std::vector<std::thread> worker_threads_;
    std::queue<int> connection_queue_;
    std::mutex queue_mutex_;
    std::condition_variable queue_cv_;
    
    // Connection management
    std::atomic<int> active_connections_;
    std::mutex connections_mutex_;
    std::unordered_set<int> active_sockets_;

public:
    HTTPServer();
    HTTPServer(const HTTPServerConfig& config);
    ~HTTPServer();

    // Server control
    bool listen(int port, const std::string& host = "0.0.0.0");
    bool listen(const HTTPServerConfig& config);
    void close();
    
    // Handler registration
    void on_request(HTTPRequestHandler handler);
    
    // Status
    bool is_running() const { return running_.load(); }
    int active_connection_count() const { return active_connections_.load(); }
    
    // High-performance static methods for runtime integration
    static HTTPServer* create_server(HTTPRequestHandler handler);
    static bool start_server(HTTPServer* server, int port, const std::string& host = "0.0.0.0");

private:
    void accept_loop();
    void worker_thread();
    void handle_connection(int client_socket);
    bool read_request(int socket, std::string& request_data);
    void process_request(int socket, const std::string& request_data);
    void send_error_response(int socket, HTTPStatus status, const std::string& message = "");
    void cleanup_socket(int socket);
    bool setup_server_socket();
};

// Global HTTP server registry for runtime integration
class HTTPServerRegistry {
private:
    static std::atomic<uint64_t> next_server_id_;
    static std::unordered_map<uint64_t, std::unique_ptr<HTTPServer>> servers_;
    static std::mutex registry_mutex_;

public:
    static uint64_t register_server(std::unique_ptr<HTTPServer> server);
    static HTTPServer* get_server(uint64_t server_id);
    static bool remove_server(uint64_t server_id);
    static void shutdown_all_servers();
};

// Runtime integration functions
extern "C" {
    // Core HTTP server functions for runtime.http
    void* __runtime_http_create_server_advanced(void* handler_ptr);
    int64_t __runtime_http_server_listen_advanced(void* server_ptr, int64_t port, const char* host);
    bool __runtime_http_server_close(void* server_ptr);
    
    // Request/Response object functions
    void* __runtime_http_request_get_method(void* request_ptr);
    void* __runtime_http_request_get_url(void* request_ptr);
    void* __runtime_http_request_get_header(void* request_ptr, const char* name);
    void* __runtime_http_request_get_body(void* request_ptr);
    
    void __runtime_http_response_set_status(void* response_ptr, int64_t status);
    void __runtime_http_response_set_header(void* response_ptr, const char* name, const char* value);
    void __runtime_http_response_write(void* response_ptr, const char* data, int64_t length);
    void __runtime_http_response_end(void* response_ptr, const char* data);
    void __runtime_http_response_json(void* response_ptr, const char* json_data);
}

} // namespace ultraScript
