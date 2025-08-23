#include "runtime_http_server.h"
#include "runtime.h"
#include "goroutine_system.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sstream>
#include <algorithm>
#include <cstring>
#include <fstream>



// Static members initialization
std::atomic<uint64_t> HTTPServerRegistry::next_server_id_{1};
std::unordered_map<uint64_t, std::unique_ptr<HTTPServer>> HTTPServerRegistry::servers_;
std::mutex HTTPServerRegistry::registry_mutex_;

// ============================================================================
// HTTPRequest Implementation
// ============================================================================

HTTPRequest::HTTPRequest(int socket_fd) 
    : method_(HTTPMethod::UNKNOWN), socket_fd_(socket_fd) {
}

HTTPRequest::~HTTPRequest() = default;

bool HTTPRequest::parse(const char* raw_request, size_t length) {
    std::string request_str(raw_request, length);
    std::istringstream request_stream(request_str);
    std::string line;
    
    // Parse request line (METHOD /path HTTP/1.1)
    if (!std::getline(request_stream, line)) {
        return false;
    }
    
    // Remove carriage return if present
    if (!line.empty() && line.back() == '\r') {
        line.pop_back();
    }
    
    std::istringstream request_line(line);
    std::string method_str, http_version;
    if (!(request_line >> method_str >> url_ >> http_version)) {
        return false;
    }
    
    method_ = parse_method(method_str);
    parse_url();
    
    // Parse headers
    while (std::getline(request_stream, line) && !line.empty()) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        
        if (line.empty()) break; // End of headers
        
        size_t colon_pos = line.find(':');
        if (colon_pos != std::string::npos) {
            std::string header_name = line.substr(0, colon_pos);
            std::string header_value = line.substr(colon_pos + 1);
            
            // Trim whitespace
            header_name.erase(0, header_name.find_first_not_of(" \t"));
            header_name.erase(header_name.find_last_not_of(" \t") + 1);
            header_value.erase(0, header_value.find_first_not_of(" \t"));
            header_value.erase(header_value.find_last_not_of(" \t") + 1);
            
            // Convert header name to lowercase for case-insensitive lookup
            std::transform(header_name.begin(), header_name.end(), header_name.begin(), ::tolower);
            headers_[header_name] = header_value;
        }
    }
    
    // Parse body (rest of the request)
    std::string body_line;
    while (std::getline(request_stream, body_line)) {
        body_ += body_line + "\n";
    }
    
    // Remove trailing newline if present
    if (!body_.empty() && body_.back() == '\n') {
        body_.pop_back();
    }
    
    return true;
}

void HTTPRequest::parse_url() {
    size_t query_pos = url_.find('?');
    if (query_pos != std::string::npos) {
        path_ = url_.substr(0, query_pos);
        query_string_ = url_.substr(query_pos + 1);
        parse_query_string();
    } else {
        path_ = url_;
        query_string_.clear();
    }
}

void HTTPRequest::parse_query_string() {
    if (query_string_.empty()) return;
    
    std::istringstream query_stream(query_string_);
    std::string param;
    
    while (std::getline(query_stream, param, '&')) {
        size_t eq_pos = param.find('=');
        if (eq_pos != std::string::npos) {
            std::string key = param.substr(0, eq_pos);
            std::string value = param.substr(eq_pos + 1);
            query_params_[key] = value;
        } else {
            query_params_[param] = "";
        }
    }
}

HTTPMethod HTTPRequest::parse_method(const std::string& method_str) {
    if (method_str == "GET") return HTTPMethod::GET;
    if (method_str == "POST") return HTTPMethod::POST;
    if (method_str == "PUT") return HTTPMethod::PUT;
    if (method_str == "DELETE") return HTTPMethod::DELETE;
    if (method_str == "HEAD") return HTTPMethod::HEAD;
    if (method_str == "OPTIONS") return HTTPMethod::OPTIONS;
    if (method_str == "PATCH") return HTTPMethod::PATCH;
    return HTTPMethod::UNKNOWN;
}

std::string HTTPRequest::get_header(const std::string& name) const {
    std::string lower_name = name;
    std::transform(lower_name.begin(), lower_name.end(), lower_name.begin(), ::tolower);
    
    auto it = headers_.find(lower_name);
    return (it != headers_.end()) ? it->second : "";
}

bool HTTPRequest::has_header(const std::string& name) const {
    std::string lower_name = name;
    std::transform(lower_name.begin(), lower_name.end(), lower_name.begin(), ::tolower);
    return headers_.find(lower_name) != headers_.end();
}

std::string HTTPRequest::get_query_param(const std::string& name) const {
    auto it = query_params_.find(name);
    return (it != query_params_.end()) ? it->second : "";
}

bool HTTPRequest::has_query_param(const std::string& name) const {
    return query_params_.find(name) != query_params_.end();
}

// ============================================================================
// HTTPResponse Implementation
// ============================================================================

HTTPResponse::HTTPResponse(int socket_fd) 
    : status_(HTTPStatus::OK), socket_fd_(socket_fd), headers_sent_(false), finished_(false) {
    // Set default headers
    set_header("Server", "UltraScript/1.0");
    set_header("Connection", "close");
}

HTTPResponse::~HTTPResponse() {
    if (!finished_) {
        end();
    }
}

void HTTPResponse::set_header(const std::string& name, const std::string& value) {
    if (headers_sent_) return;
    headers_[name] = value;
}

void HTTPResponse::write_head(HTTPStatus status, const std::unordered_map<std::string, std::string>& headers) {
    if (headers_sent_) return;
    
    status_ = status;
    for (const auto& header : headers) {
        headers_[header.first] = header.second;
    }
    
    send_headers();
}

void HTTPResponse::write(const std::string& data) {
    write(data.c_str(), data.length());
}

void HTTPResponse::write(const char* data, size_t length) {
    if (finished_) return;
    
    if (!headers_sent_) {
        send_headers();
    }
    
    ssize_t bytes_sent = send(socket_fd_, data, length, MSG_NOSIGNAL);
    if (bytes_sent < 0) {
        // Handle error - connection might be closed
        finished_ = true;
    }
}

void HTTPResponse::end(const std::string& data) {
    if (finished_) return;
    
    if (!data.empty()) {
        write(data);
    }
    
    finished_ = true;
}

void HTTPResponse::json(const std::string& json_data) {
    set_header("Content-Type", "application/json");
    set_header("Content-Length", std::to_string(json_data.length()));
    end(json_data);
}

void HTTPResponse::html(const std::string& html_data) {
    set_header("Content-Type", "text/html; charset=utf-8");
    set_header("Content-Length", std::to_string(html_data.length()));
    end(html_data);
}

void HTTPResponse::send_file(const std::string& file_path) {
    std::ifstream file(file_path, std::ios::binary);
    if (!file.is_open()) {
        status_ = HTTPStatus::NOT_FOUND;
        end("File not found");
        return;
    }
    
    // Get file size
    file.seekg(0, std::ios::end);
    size_t file_size = file.tellg();
    file.seekg(0, std::ios::beg);
    
    // Set content type based on file extension
    size_t dot_pos = file_path.find_last_of('.');
    if (dot_pos != std::string::npos) {
        std::string ext = file_path.substr(dot_pos + 1);
        if (ext == "html" || ext == "htm") {
            set_header("Content-Type", "text/html");
        } else if (ext == "css") {
            set_header("Content-Type", "text/css");
        } else if (ext == "js") {
            set_header("Content-Type", "application/javascript");
        } else if (ext == "json") {
            set_header("Content-Type", "application/json");
        } else if (ext == "png") {
            set_header("Content-Type", "image/png");
        } else if (ext == "jpg" || ext == "jpeg") {
            set_header("Content-Type", "image/jpeg");
        } else {
            set_header("Content-Type", "application/octet-stream");
        }
    }
    
    set_header("Content-Length", std::to_string(file_size));
    
    if (!headers_sent_) {
        send_headers();
    }
    
    // Send file in chunks
    char buffer[8192];
    while (file.read(buffer, sizeof(buffer)) || file.gcount() > 0) {
        write(buffer, file.gcount());
    }
    
    finished_ = true;
}

void HTTPResponse::send_headers() {
    if (headers_sent_) return;
    
    std::ostringstream response;
    response << "HTTP/1.1 " << static_cast<int>(status_) << " " << get_status_text(status_) << "\r\n";
    
    for (const auto& header : headers_) {
        response << header.first << ": " << header.second << "\r\n";
    }
    
    response << "\r\n";
    
    std::string response_str = response.str();
    ssize_t bytes_sent = send(socket_fd_, response_str.c_str(), response_str.length(), MSG_NOSIGNAL);
    
    if (bytes_sent > 0) {
        headers_sent_ = true;
    }
}

std::string HTTPResponse::get_status_text(HTTPStatus status) {
    switch (status) {
        case HTTPStatus::OK: return "OK";
        case HTTPStatus::CREATED: return "Created";
        case HTTPStatus::NO_CONTENT: return "No Content";
        case HTTPStatus::BAD_REQUEST: return "Bad Request";
        case HTTPStatus::UNAUTHORIZED: return "Unauthorized";
        case HTTPStatus::FORBIDDEN: return "Forbidden";
        case HTTPStatus::NOT_FOUND: return "Not Found";
        case HTTPStatus::METHOD_NOT_ALLOWED: return "Method Not Allowed";
        case HTTPStatus::INTERNAL_SERVER_ERROR: return "Internal Server Error";
        case HTTPStatus::NOT_IMPLEMENTED: return "Not Implemented";
        case HTTPStatus::BAD_GATEWAY: return "Bad Gateway";
        case HTTPStatus::SERVICE_UNAVAILABLE: return "Service Unavailable";
        default: return "Unknown";
    }
}

// ============================================================================
// HTTPServer Implementation
// ============================================================================

HTTPServer::HTTPServer() : HTTPServer(HTTPServerConfig{}) {}

HTTPServer::HTTPServer(const HTTPServerConfig& config) 
    : config_(config), running_(false), stopping_(false), server_socket_(-1), active_connections_(0) {
}

HTTPServer::~HTTPServer() {
    close();
}

bool HTTPServer::listen(int port, const std::string& host) {
    HTTPServerConfig config = config_;
    config.port = port;
    config.host = host;
    return listen(config);
}

bool HTTPServer::listen(const HTTPServerConfig& config) {
    if (running_.load()) {
        return false; // Already running
    }
    
    config_ = config;
    
    if (!setup_server_socket()) {
        return false;
    }
    
    running_.store(true);
    stopping_.store(false);
    
    // Start worker threads
    for (int i = 0; i < config_.thread_pool_size; ++i) {
        worker_threads_.emplace_back(&HTTPServer::worker_thread, this);
    }
    
    // Start accept loop in a separate thread
    std::thread accept_thread(&HTTPServer::accept_loop, this);
    accept_thread.detach();
    
    return true;
}

void HTTPServer::close() {
    if (!running_.load()) return;
    
    stopping_.store(true);
    running_.store(false);
    
    // Close server socket to stop accepting new connections
    if (server_socket_ >= 0) {
        ::close(server_socket_);
        server_socket_ = -1;
    }
    
    // Wake up all worker threads
    queue_cv_.notify_all();
    
    // Wait for worker threads to finish
    for (auto& thread : worker_threads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    worker_threads_.clear();
    
    // Close all active connections
    {
        std::lock_guard<std::mutex> lock(connections_mutex_);
        for (int socket : active_sockets_) {
            ::close(socket);
        }
        active_sockets_.clear();
    }
    
    active_connections_.store(0);
}

void HTTPServer::on_request(HTTPRequestHandler handler) {
    request_handler_ = handler;
}

HTTPServer* HTTPServer::create_server(HTTPRequestHandler handler) {
    auto server = std::make_unique<HTTPServer>();
    server->on_request(handler);
    return server.release();
}

bool HTTPServer::start_server(HTTPServer* server, int port, const std::string& host) {
    if (!server) return false;
    return server->listen(port, host);
}

void HTTPServer::accept_loop() {
    while (running_.load() && !stopping_.load()) {
        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);
        
        int client_socket = accept(server_socket_, (struct sockaddr*)&client_addr, &addr_len);
        
        if (client_socket < 0) {
            if (running_.load() && !stopping_.load()) {
                // Real error, not just shutdown
                continue;
            } else {
                break; // Shutting down
            }
        }
        
        // Check connection limit
        if (active_connections_.load() >= config_.max_connections) {
            send_error_response(client_socket, HTTPStatus::SERVICE_UNAVAILABLE);
            ::close(client_socket);
            continue;
        }
        
        // Add to active connections
        {
            std::lock_guard<std::mutex> lock(connections_mutex_);
            active_sockets_.insert(client_socket);
        }
        active_connections_.fetch_add(1);
        
        // Queue for worker thread
        {
            std::lock_guard<std::mutex> lock(queue_mutex_);
            connection_queue_.push(client_socket);
        }
        queue_cv_.notify_one();
    }
}

void HTTPServer::worker_thread() {
    while (running_.load() || !connection_queue_.empty()) {
        int client_socket = -1;
        
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            queue_cv_.wait(lock, [this] { 
                return !connection_queue_.empty() || stopping_.load(); 
            });
            
            if (!connection_queue_.empty()) {
                client_socket = connection_queue_.front();
                connection_queue_.pop();
            }
        }
        
        if (client_socket >= 0) {
            handle_connection(client_socket);
        }
    }
}

void HTTPServer::handle_connection(int client_socket) {
    std::string request_data;
    
    if (read_request(client_socket, request_data)) {
        process_request(client_socket, request_data);
    } else {
        send_error_response(client_socket, HTTPStatus::BAD_REQUEST);
    }
    
    cleanup_socket(client_socket);
}

bool HTTPServer::read_request(int socket, std::string& request_data) {
    char buffer[8192];
    std::string headers;
    size_t content_length = 0;
    bool headers_complete = false;
    
    // Set socket timeout
    struct timeval timeout;
    timeout.tv_sec = 30; // 30 second timeout
    timeout.tv_usec = 0;
    setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    
    while (!headers_complete) {
        ssize_t bytes_read = recv(socket, buffer, sizeof(buffer) - 1, 0);
        
        if (bytes_read <= 0) {
            return false; // Connection closed or error
        }
        
        buffer[bytes_read] = '\0';
        headers += buffer;
        
        // Check for end of headers
        size_t header_end = headers.find("\r\n\r\n");
        if (header_end != std::string::npos) {
            headers_complete = true;
            
            // Parse Content-Length header
            size_t content_length_pos = headers.find("Content-Length:");
            if (content_length_pos == std::string::npos) {
                content_length_pos = headers.find("content-length:");
            }
            
            if (content_length_pos != std::string::npos) {
                size_t value_start = headers.find(':', content_length_pos) + 1;
                size_t value_end = headers.find('\r', value_start);
                if (value_end != std::string::npos) {
                    std::string length_str = headers.substr(value_start, value_end - value_start);
                    // Trim whitespace
                    length_str.erase(0, length_str.find_first_not_of(" \t"));
                    length_str.erase(length_str.find_last_not_of(" \t") + 1);
                    content_length = std::stoull(length_str);
                }
            }
            
            // Check if we already have the complete body
            size_t body_start = header_end + 4;
            size_t body_received = headers.length() - body_start;
            
            if (body_received >= content_length) {
                request_data = headers;
                return true;
            }
            
            // Read remaining body
            request_data = headers;
            size_t remaining = content_length - body_received;
            
            while (remaining > 0) {
                bytes_read = recv(socket, buffer, std::min(remaining, sizeof(buffer) - 1), 0);
                if (bytes_read <= 0) {
                    return false;
                }
                
                buffer[bytes_read] = '\0';
                request_data += buffer;
                remaining -= bytes_read;
            }
            
            return true;
        }
        
        // Check for maximum header size
        if (headers.length() > config_.max_header_size) {
            return false;
        }
    }
    
    return false;
}

void HTTPServer::process_request(int socket, const std::string& request_data) {
    if (!request_handler_) {
        send_error_response(socket, HTTPStatus::NOT_IMPLEMENTED);
        return;
    }
    
    HTTPRequest request(socket);
    HTTPResponse response(socket);
    
    if (!request.parse(request_data.c_str(), request_data.length())) {
        send_error_response(socket, HTTPStatus::BAD_REQUEST);
        return;
    }
    
    try {
        // For standalone version, execute directly without goroutines
        // In full UltraScript runtime, this would use goroutines
        #ifdef ULTRASCRIPT_STANDALONE
            request_handler_(request, response);
        #else
            // Execute request handler in goroutine for maximum performance
            if (current_goroutine) {
                // Already in a goroutine context, execute directly
                request_handler_(request, response);
            } else {
                // Spawn a new goroutine for this request
                spawn_goroutine([this, request = std::move(request), response = std::move(response)]() mutable {
                    request_handler_(request, response);
                });
            }
        #endif
    } catch (const std::exception& e) {
        send_error_response(socket, HTTPStatus::INTERNAL_SERVER_ERROR, e.what());
    } catch (...) {
        send_error_response(socket, HTTPStatus::INTERNAL_SERVER_ERROR);
    }
}

void HTTPServer::send_error_response(int socket, HTTPStatus status, const std::string& message) {
    std::ostringstream response;
    response << "HTTP/1.1 " << static_cast<int>(status) << " " << HTTPResponse::get_status_text(status) << "\r\n";
    response << "Content-Type: text/plain\r\n";
    response << "Connection: close\r\n";
    
    std::string body = message.empty() ? HTTPResponse::get_status_text(status) : message;
    response << "Content-Length: " << body.length() << "\r\n";
    response << "\r\n";
    response << body;
    
    std::string response_str = response.str();
    send(socket, response_str.c_str(), response_str.length(), MSG_NOSIGNAL);
}

void HTTPServer::cleanup_socket(int socket) {
    {
        std::lock_guard<std::mutex> lock(connections_mutex_);
        active_sockets_.erase(socket);
    }
    active_connections_.fetch_sub(1);
    ::close(socket);
}

bool HTTPServer::setup_server_socket() {
    server_socket_ = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket_ < 0) {
        return false;
    }
    
    // Set socket options
    int opt = 1;
    setsockopt(server_socket_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    setsockopt(server_socket_, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt));
    
    // Bind socket
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(config_.port);
    
    if (config_.host == "0.0.0.0" || config_.host.empty()) {
        addr.sin_addr.s_addr = INADDR_ANY;
    } else {
        if (inet_pton(AF_INET, config_.host.c_str(), &addr.sin_addr) <= 0) {
            ::close(server_socket_);
            server_socket_ = -1;
            return false;
        }
    }
    
    if (bind(server_socket_, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        ::close(server_socket_);
        server_socket_ = -1;
        return false;
    }
    
    // Start listening
    if (::listen(server_socket_, config_.backlog) < 0) {
        ::close(server_socket_);
        server_socket_ = -1;
        return false;
    }
    
    return true;
}

// ============================================================================
// HTTPServerRegistry Implementation
// ============================================================================

uint64_t HTTPServerRegistry::register_server(std::unique_ptr<HTTPServer> server) {
    std::lock_guard<std::mutex> lock(registry_mutex_);
    uint64_t server_id = next_server_id_.fetch_add(1);
    servers_[server_id] = std::move(server);
    return server_id;
}

HTTPServer* HTTPServerRegistry::get_server(uint64_t server_id) {
    std::lock_guard<std::mutex> lock(registry_mutex_);
    auto it = servers_.find(server_id);
    return (it != servers_.end()) ? it->second.get() : nullptr;
}

bool HTTPServerRegistry::remove_server(uint64_t server_id) {
    std::lock_guard<std::mutex> lock(registry_mutex_);
    auto it = servers_.find(server_id);
    if (it != servers_.end()) {
        it->second->close(); // Ensure server is stopped
        servers_.erase(it);
        return true;
    }
    return false;
}

void HTTPServerRegistry::shutdown_all_servers() {
    std::lock_guard<std::mutex> lock(registry_mutex_);
    for (auto& [id, server] : servers_) {
        server->close();
    }
    servers_.clear();
}

// ============================================================================
// Runtime Integration Functions
// ============================================================================

extern "C" {

void* __runtime_http_create_server_advanced(void* handler_ptr) {
    if (!handler_ptr) return nullptr;
    
    // Convert function pointer to HTTPRequestHandler
    auto handler_func = reinterpret_cast<void(*)(void*, void*)>(handler_ptr);
    
    HTTPRequestHandler handler = [handler_func](HTTPRequest& req, HTTPResponse& res) {
        handler_func(static_cast<void*>(&req), static_cast<void*>(&res));
    };
    
    auto server = std::make_unique<HTTPServer>();
    server->on_request(handler);
    
    uint64_t server_id = HTTPServerRegistry::register_server(std::move(server));
    return reinterpret_cast<void*>(server_id);
}

int64_t __runtime_http_server_listen_advanced(void* server_ptr, int64_t port, const char* host) {
    uint64_t server_id = reinterpret_cast<uint64_t>(server_ptr);
    HTTPServer* server = HTTPServerRegistry::get_server(server_id);
    
    if (!server) return 0;
    
    std::string host_str = host ? host : "0.0.0.0";
    return server->listen(static_cast<int>(port), host_str) ? 1 : 0;
}

bool __runtime_http_server_close(void* server_ptr) {
    uint64_t server_id = reinterpret_cast<uint64_t>(server_ptr);
    return HTTPServerRegistry::remove_server(server_id);
}

void* __runtime_http_request_get_method(void* request_ptr) {
    if (!request_ptr) return nullptr;
    
    HTTPRequest* req = static_cast<HTTPRequest*>(request_ptr);
    const char* method_str = "";
    
    switch (req->method()) {
        case HTTPMethod::GET: method_str = "GET"; break;
        case HTTPMethod::POST: method_str = "POST"; break;
        case HTTPMethod::PUT: method_str = "PUT"; break;
        case HTTPMethod::DELETE: method_str = "DELETE"; break;
        case HTTPMethod::HEAD: method_str = "HEAD"; break;
        case HTTPMethod::OPTIONS: method_str = "OPTIONS"; break;
        case HTTPMethod::PATCH: method_str = "PATCH"; break;
        default: method_str = "UNKNOWN"; break;
    }
    
    return reinterpret_cast<void*>(const_cast<char*>(method_str));
}

void* __runtime_http_request_get_url(void* request_ptr) {
    if (!request_ptr) return nullptr;
    
    HTTPRequest* req = static_cast<HTTPRequest*>(request_ptr);
    return reinterpret_cast<void*>(const_cast<char*>(req->url().c_str()));
}

void* __runtime_http_request_get_header(void* request_ptr, const char* name) {
    if (!request_ptr || !name) return nullptr;
    
    HTTPRequest* req = static_cast<HTTPRequest*>(request_ptr);
    std::string header_value = req->get_header(name);
    
    // Return pointer to static string (in real implementation, would need proper memory management)
    static std::string static_header;
    static_header = header_value;
    return reinterpret_cast<void*>(const_cast<char*>(static_header.c_str()));
}

void* __runtime_http_request_get_body(void* request_ptr) {
    if (!request_ptr) return nullptr;
    
    HTTPRequest* req = static_cast<HTTPRequest*>(request_ptr);
    return reinterpret_cast<void*>(const_cast<char*>(req->body().c_str()));
}

void __runtime_http_response_set_status(void* response_ptr, int64_t status) {
    if (!response_ptr) return;
    
    HTTPResponse* res = static_cast<HTTPResponse*>(response_ptr);
    res->set_status(static_cast<HTTPStatus>(status));
}

void __runtime_http_response_set_header(void* response_ptr, const char* name, const char* value) {
    if (!response_ptr || !name || !value) return;
    
    HTTPResponse* res = static_cast<HTTPResponse*>(response_ptr);
    res->set_header(name, value);
}

void __runtime_http_response_write(void* response_ptr, const char* data, int64_t length) {
    if (!response_ptr || !data || length <= 0) return;
    
    HTTPResponse* res = static_cast<HTTPResponse*>(response_ptr);
    res->write(data, static_cast<size_t>(length));
}

void __runtime_http_response_end(void* response_ptr, const char* data) {
    if (!response_ptr) return;
    
    HTTPResponse* res = static_cast<HTTPResponse*>(response_ptr);
    if (data) {
        res->end(data);
    } else {
        res->end();
    }
}

void __runtime_http_response_json(void* response_ptr, const char* json_data) {
    if (!response_ptr || !json_data) return;
    
    HTTPResponse* res = static_cast<HTTPResponse*>(response_ptr);
    res->json(json_data);
}

} // extern "C"


