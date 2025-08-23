// UltraScript HTTP Server Test Example
// This demonstrates the high-performance HTTP server integrated with goroutines

#include "runtime_http_server.h"
#include "runtime_object.h"
#include "goroutine_system.h"
#include <iostream>
#include <thread>
#include <chrono>



// Example request handler that shows UltraScript-style async/goroutine integration
void handle_request(HTTPRequest& req, HTTPResponse& res) {
    std::cout << "[Goroutine " << std::this_thread::get_id() << "] Handling " 
              << (req.method() == HTTPMethod::GET ? "GET" : "POST") 
              << " request to " << req.path() << std::endl;
    
    // Simulate some async work with goroutine integration
    if (req.path() == "/") {
        res.html(R"(
<!DOCTYPE html>
<html>
<head>
    <title>UltraScript HTTP Server</title>
    <style>
        body { font-family: Arial, sans-serif; margin: 40px; }
        .container { max-width: 800px; margin: 0 auto; }
        .feature { background: #f0f8ff; padding: 20px; margin: 20px 0; border-radius: 8px; }
        code { background: #f5f5f5; padding: 2px 5px; border-radius: 3px; }
    </style>
</head>
<body>
    <div class="container">
        <h1>🚀 UltraScript HTTP Server</h1>
        <p>High-performance HTTP server optimized for goroutines and ultra-fast execution!</p>
        
        <div class="feature">
            <h3>⚡ Performance Features</h3>
            <ul>
                <li>Goroutine-optimized request handling</li>
                <li>Multi-threaded connection processing</li>
                <li>Zero-copy string operations where possible</li>
                <li>Lock-free request queuing</li>
                <li>Integrated with UltraScript runtime</li>
            </ul>
        </div>
        
        <div class="feature">
            <h3>🔗 Test Endpoints</h3>
            <p><a href="/api/test">GET /api/test</a> - JSON API test</p>
            <p><a href="/api/info">GET /api/info</a> - Server information</p>
            <p><a href="/api/goroutine">GET /api/goroutine</a> - Goroutine demo</p>
        </div>
        
        <div class="feature">
            <h3>📝 Usage Example</h3>
            <pre><code>// UltraScript syntax
let server = runtime.http.createServer((req, res) => {
    go async function() {
        let data = await processRequest(req);
        res.json(data);
    }();
});

await server.listen(8080);
console.log("Server running on http://localhost:8080");
</code></pre>
        </div>
    </div>
</body>
</html>
        )");
        
    } else if (req.path() == "/api/test") {
        res.json(R"({
            "status": "success",
            "message": "UltraScript HTTP server is working!",
            "timestamp": ")" + std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count()) + R"(",
            "goroutine_id": ")" + std::to_string(std::hash<std::thread::id>{}(std::this_thread::get_id())) + R"(",
            "features": ["goroutines", "high-performance", "zero-copy", "concurrent"]
        })");
        
    } else if (req.path() == "/api/info") {
        res.json(R"({
            "server": "UltraScript HTTP Server",
            "version": "1.0.0",
            "language": "UltraScript (C++ runtime)",
            "performance": {
                "goroutine_optimized": true,
                "multi_threaded": true,
                "connection_pooling": true,
                "request_pipelining": true
            },
            "headers_received": )" + std::to_string(req.headers().size()) + R"(,
            "method": ")" + (req.method() == HTTPMethod::GET ? "GET" : "POST") + R"(",
            "user_agent": ")" + req.get_header("user-agent") + R"("
        })");
        
    } else if (req.path() == "/api/goroutine") {
        // Demonstrate goroutine spawning for async processing
        std::string goroutine_info = R"({
            "message": "This response was processed in a goroutine!",
            "current_thread": ")" + std::to_string(std::hash<std::thread::id>{}(std::this_thread::get_id())) + R"(",
            "demo": "goroutine_async_processing",
            "ultra_fast": true
        })";
        
        res.json(goroutine_info);
        
    } else {
        res.set_status(HTTPStatus::NOT_FOUND);
        res.json(R"({
            "error": "Not Found",
            "message": "The requested endpoint does not exist",
            "available_endpoints": ["/", "/api/test", "/api/info", "/api/goroutine"]
        })");
    }
}

int main() {
    std::cout << "🚀 Starting UltraScript HTTP Server Test..." << std::endl;
    
    // Initialize the runtime system
    initialize_runtime_object();
    
    // Create HTTP server with goroutine-optimized handler
    HTTPServerConfig config;
    config.port = 8080;
    config.host = "0.0.0.0";
    config.thread_pool_size = 8;
    config.max_connections = 1000;
    
    HTTPServer server(config);
    server.on_request(handle_request);
    
    // Start server
    if (!server.listen(config)) {
        std::cerr << "❌ Failed to start server on port " << config.port << std::endl;
        return 1;
    }
    
    std::cout << "✅ UltraScript HTTP Server running on http://localhost:" << config.port << std::endl;
    std::cout << "📊 Configuration:" << std::endl;
    std::cout << "   - Thread pool size: " << config.thread_pool_size << std::endl;
    std::cout << "   - Max connections: " << config.max_connections << std::endl;
    std::cout << "   - Goroutine optimized: Yes" << std::endl;
    std::cout << "   - High performance mode: Enabled" << std::endl;
    std::cout << "\n🌐 Visit http://localhost:8080 to see it in action!" << std::endl;
    std::cout << "📚 API endpoints:" << std::endl;
    std::cout << "   - GET /api/test - JSON API test" << std::endl;
    std::cout << "   - GET /api/info - Server information" << std::endl;
    std::cout << "   - GET /api/goroutine - Goroutine demo" << std::endl;
    
    // Test with some concurrent requests to demonstrate goroutine handling
    std::cout << "\n🧪 Running concurrent request test..." << std::endl;
    
    // Spawn test goroutines to make requests
    for (int i = 0; i < 5; ++i) {
        spawn_goroutine([i]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(100 * i));
            std::cout << "Test goroutine " << i << " would make HTTP request here" << std::endl;
        });
    }
    
    // Keep server running
    std::cout << "\n⏳ Server is running. Press Ctrl+C to stop..." << std::endl;
    
    // Simple event loop to keep main thread alive
    while (server.is_running()) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        
        // Print server stats every 10 seconds
        static int counter = 0;
        if (++counter % 10 == 0) {
            std::cout << "📈 Server stats - Active connections: " 
                      << server.active_connection_count() << std::endl;
        }
    }
    
    std::cout << "🛑 Server stopped." << std::endl;
    return 0;
}
