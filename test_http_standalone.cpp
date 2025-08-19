// Standalone HTTP Server Test
// Tests the HTTP server without full UltraScript runtime dependencies

#include "runtime_http_server.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <signal.h>



// Global server pointer for signal handling
HTTPServer* global_server = nullptr;

void signal_handler(int signal) {
    std::cout << "\n🛑 Received signal " << signal << ", shutting down server..." << std::endl;
    if (global_server) {
        global_server->close();
    }
    exit(0);
}

// Simple request handler for testing
void handle_test_request(HTTPRequest& req, HTTPResponse& res) {
    std::cout << "[Thread " << std::this_thread::get_id() << "] " 
              << (req.method() == HTTPMethod::GET ? "GET" : 
                  req.method() == HTTPMethod::POST ? "POST" : "OTHER") 
              << " " << req.path() << std::endl;
    
    if (req.path() == "/") {
        res.html(R"(<!DOCTYPE html>
<html>
<head><title>UltraScript HTTP Test</title></head>
<body>
    <h1>🚀 UltraScript HTTP Server Test</h1>
    <p>The server is working correctly!</p>
    <ul>
        <li><a href="/test">Test JSON API</a></li>
        <li><a href="/info">Server Info</a></li>
        <li><a href="/performance">Performance Test</a></li>
    </ul>
</body>
</html>)");
        
    } else if (req.path() == "/test") {
        res.json(R"({
            "status": "success",
            "message": "HTTP server is working!",
            "thread_id": ")" + std::to_string(std::hash<std::thread::id>{}(std::this_thread::get_id())) + R"(",
            "timestamp": )" + std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count()) + R"(
        })");
        
    } else if (req.path() == "/info") {
        std::string method_str = (req.method() == HTTPMethod::GET ? "GET" : "POST");
        std::string json_response = R"({
            "server": "UltraScript HTTP Server",
            "version": "1.0.0-standalone", 
            "method": ")" + method_str + R"(",
            "headers_count": )" + std::to_string(req.headers().size()) + R"(,
            "user_agent": ")" + req.get_header("user-agent") + R"("
        })";
        res.json(json_response);
    } else if (req.path() == "/performance") {
        // Simulate some work
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        
        res.json(R"({
            "performance_test": "completed",
            "processing_time_ms": 10,
            "concurrent_capable": true,
            "optimized": true
        })");
        
    } else if (req.path() == "/echo" && req.method() == HTTPMethod::POST) {
        std::string echo_response = R"({"echo": ")" + req.body() + R"("})";
        res.set_header("Content-Type", "application/json");
        res.end(echo_response);
        
    } else {
        res.set_status(HTTPStatus::NOT_FOUND);
        std::string error_response = R"({"error": "Not Found", "path": ")" + req.path() + R"("})";
        res.json(error_response);
    }
}

int main() {
    std::cout << "🧪 UltraScript HTTP Server Standalone Test" << std::endl;
    
    // Set up signal handling
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // Create server with test configuration
    HTTPServerConfig config;
    config.port = 8080;
    config.host = "0.0.0.0";
    config.thread_pool_size = 4;
    config.max_connections = 100;
    
    HTTPServer server(config);
    global_server = &server;
    
    server.on_request(handle_test_request);
    
    std::cout << "🚀 Starting server on http://localhost:" << config.port << std::endl;
    
    if (!server.listen(config)) {
        std::cerr << "❌ Failed to start server" << std::endl;
        return 1;
    }
    
    std::cout << "✅ Server started successfully!" << std::endl;
    std::cout << "📊 Configuration:" << std::endl;
    std::cout << "   - Port: " << config.port << std::endl;
    std::cout << "   - Threads: " << config.thread_pool_size << std::endl;
    std::cout << "   - Max connections: " << config.max_connections << std::endl;
    
    std::cout << "\n🌐 Test URLs:" << std::endl;
    std::cout << "   http://localhost:8080/ - Main page" << std::endl;
    std::cout << "   http://localhost:8080/test - JSON API" << std::endl;
    std::cout << "   http://localhost:8080/info - Server info" << std::endl;
    std::cout << "   http://localhost:8080/performance - Performance test" << std::endl;
    
    std::cout << "\n📡 Test with curl:" << std::endl;
    std::cout << "   curl http://localhost:8080/test" << std::endl;
    std::cout << "   curl -X POST -d 'hello' http://localhost:8080/echo" << std::endl;
    
    std::cout << "\n⏳ Server running... Press Ctrl+C to stop" << std::endl;
    
    // Keep server running and show stats
    int seconds = 0;
    while (server.is_running()) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        seconds++;
        
        // Show stats every 30 seconds
        if (seconds % 30 == 0) {
            std::cout << "📈 Uptime: " << seconds << "s, Active connections: " 
                      << server.active_connection_count() << std::endl;
        }
    }
    
    std::cout << "🛑 Server stopped after " << seconds << " seconds" << std::endl;
    return 0;
}
