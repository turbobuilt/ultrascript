#include "runtime_http_server.h"
#include "runtime.h"
#include "runtime_object.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <sstream>



// Simple HTTP Client Implementation without libcurl dependency
class HTTPClient {
public:
    struct HTTPResponse {
        std::string body;
        std::unordered_map<std::string, std::string> headers;
        long status_code;
        
        HTTPResponse() : status_code(0) {}
    };

    static HTTPResponse* make_request(const std::string& method, const std::string& url, 
                                    const std::unordered_map<std::string, std::string>& headers = {},
                                    const std::string& body = "") {
        
        // Simple HTTP client implementation without libcurl
        // Parse URL to extract host, port, and path
        std::string host, path;
        int port = 80;
        
        if (!parse_url(url, host, port, path)) {
            return nullptr;
        }
        
        // Create socket
        int sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0) return nullptr;
        
        // Resolve hostname
        struct hostent* server = gethostbyname(host.c_str());
        if (!server) {
            close(sock);
            return nullptr;
        }
        
        // Connect to server
        struct sockaddr_in serv_addr;
        memset(&serv_addr, 0, sizeof(serv_addr));
        serv_addr.sin_family = AF_INET;
        serv_addr.sin_port = htons(port);
        memcpy(&serv_addr.sin_addr.s_addr, server->h_addr, server->h_length);
        
        if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
            close(sock);
            return nullptr;
        }
        
        // Build HTTP request
        std::ostringstream request;
        request << method << " " << path << " HTTP/1.1\r\n";
        request << "Host: " << host << "\r\n";
        request << "Connection: close\r\n";
        
        for (const auto& header : headers) {
            request << header.first << ": " << header.second << "\r\n";
        }
        
        if (!body.empty()) {
            request << "Content-Length: " << body.length() << "\r\n";
        }
        
        request << "\r\n";
        if (!body.empty()) {
            request << body;
        }
        
        // Send request
        std::string request_str = request.str();
        if (send(sock, request_str.c_str(), request_str.length(), 0) < 0) {
            close(sock);
            return nullptr;
        }
        
        // Read response
        auto response = std::make_unique<HTTPResponse>();
        std::string response_data;
        char buffer[4096];
        ssize_t bytes_read;
        
        while ((bytes_read = recv(sock, buffer, sizeof(buffer), 0)) > 0) {
            response_data.append(buffer, bytes_read);
        }
        
        close(sock);
        
        // Parse response
        if (!parse_response(response_data, *response)) {
            return nullptr;
        }
        
        return response.release();
    }

private:
    static bool parse_url(const std::string& url, std::string& host, int& port, std::string& path) {
        // Simple URL parsing for http://host:port/path
        if (url.substr(0, 7) != "http://") {
            return false;
        }
        
        size_t start = 7; // Skip "http://"
        size_t slash_pos = url.find('/', start);
        size_t colon_pos = url.find(':', start);
        
        if (slash_pos == std::string::npos) {
            slash_pos = url.length();
            path = "/";
        } else {
            path = url.substr(slash_pos);
        }
        
        if (colon_pos != std::string::npos && colon_pos < slash_pos) {
            host = url.substr(start, colon_pos - start);
            port = std::stoi(url.substr(colon_pos + 1, slash_pos - colon_pos - 1));
        } else {
            host = url.substr(start, slash_pos - start);
            port = 80;
        }
        
        return true;
    }
    
    static bool parse_response(const std::string& response_data, HTTPResponse& response) {
        size_t header_end = response_data.find("\r\n\r\n");
        if (header_end == std::string::npos) {
            return false;
        }
        
        std::string headers = response_data.substr(0, header_end);
        std::string body = response_data.substr(header_end + 4);
        
        // Parse status line
        size_t first_space = headers.find(' ');
        size_t second_space = headers.find(' ', first_space + 1);
        if (first_space != std::string::npos && second_space != std::string::npos) {
            std::string status_str = headers.substr(first_space + 1, second_space - first_space - 1);
            response.status_code = std::stol(status_str);
        }
        
        // Parse headers
        std::istringstream header_stream(headers);
        std::string line;
        std::getline(header_stream, line); // Skip status line
        
        while (std::getline(header_stream, line) && !line.empty()) {
            if (line.back() == '\r') line.pop_back();
            
            size_t colon_pos = line.find(':');
            if (colon_pos != std::string::npos) {
                std::string name = line.substr(0, colon_pos);
                std::string value = line.substr(colon_pos + 1);
                
                // Trim whitespace
                name.erase(0, name.find_first_not_of(" \t"));
                name.erase(name.find_last_not_of(" \t") + 1);
                value.erase(0, value.find_first_not_of(" \t"));
                value.erase(value.find_last_not_of(" \t") + 1);
                
                response.headers[name] = value;
            }
        }
        
        response.body = body;
        return true;
    }
};

// ============================================================================
// Additional Runtime HTTP Functions
// ============================================================================

extern "C" {

// HTTP Client functions
void* __runtime_http_request_advanced(const char* method, const char* url, void* headers_ptr, const char* body) {
    if (!method || !url) return nullptr;
    
    std::unordered_map<std::string, std::string> headers;
    // TODO: Parse headers from headers_ptr if provided
    
    std::string body_str = body ? body : "";
    
    // Make the request in a goroutine for non-blocking behavior
    auto response = HTTPClient::make_request(method, url, headers, body_str);
    return static_cast<void*>(response);
}

void* __runtime_http_get_advanced(const char* url) {
    if (!url) return nullptr;
    
    auto response = HTTPClient::make_request("GET", url);
    return static_cast<void*>(response);
}

void* __runtime_http_post_advanced(const char* url, const char* data) {
    if (!url) return nullptr;
    
    std::string body = data ? data : "";
    std::unordered_map<std::string, std::string> headers;
    headers["Content-Type"] = "application/json";
    
    auto response = HTTPClient::make_request("POST", url, headers, body);
    return static_cast<void*>(response);
}

// HTTP Response object methods (for client responses)
int64_t __runtime_http_response_get_status(void* response_ptr) {
    if (!response_ptr) return 0;
    
    auto response = static_cast<HTTPClient::HTTPResponse*>(response_ptr);
    return response->status_code;
}

void* __runtime_http_response_get_body(void* response_ptr) {
    if (!response_ptr) return nullptr;
    
    auto response = static_cast<HTTPClient::HTTPResponse*>(response_ptr);
    return const_cast<char*>(response->body.c_str());
}

void* __runtime_http_response_get_header_client(void* response_ptr, const char* name) {
    if (!response_ptr || !name) return nullptr;
    
    auto response = static_cast<HTTPClient::HTTPResponse*>(response_ptr);
    auto it = response->headers.find(name);
    if (it != response->headers.end()) {
        return const_cast<char*>(it->second.c_str());
    }
    return nullptr;
}

void __runtime_http_response_free(void* response_ptr) {
    if (!response_ptr) return;
    
    auto response = static_cast<HTTPClient::HTTPResponse*>(response_ptr);
    delete response;
}

// Additional HTTP Response methods for server responses
void __runtime_http_response_html(void* response_ptr, const char* html_data) {
    if (!response_ptr || !html_data) return;
    
    HTTPResponse* res = static_cast<HTTPResponse*>(response_ptr);
    res->html(html_data);
}

void __runtime_http_response_send_file(void* response_ptr, const char* file_path) {
    if (!response_ptr || !file_path) return;
    
    HTTPResponse* res = static_cast<HTTPResponse*>(response_ptr);
    res->send_file(file_path);
}

// Helper function to create server with simplified callback
void* __runtime_http_create_server_simple(void* callback_ptr) {
    if (!callback_ptr) return nullptr;
    
    // Create server with goroutine-optimized handler
    auto server = std::make_unique<HTTPServer>();
    
    // Convert callback to HTTPRequestHandler
    auto callback_func = reinterpret_cast<void(*)(void*, void*)>(callback_ptr);
    
    HTTPRequestHandler handler = [callback_func](HTTPRequest& req, HTTPResponse& res) {
        // Execute callback in current goroutine context
        callback_func(static_cast<void*>(&req), static_cast<void*>(&res));
    };
    
    server->on_request(handler);
    
    uint64_t server_id = HTTPServerRegistry::register_server(std::move(server));
    return reinterpret_cast<void*>(server_id);
}

} // extern "C"


