// UltraScript HTTP Server Example
// Shows how the runtime.http API would be used in UltraScript code

// Basic HTTP server creation using runtime.http
const server = runtime.http.createServer((req: HTTPRequest, res: HTTPResponse) => {
    // All request handling happens in goroutines automatically
    go async function handleRequest() {
        console.log(`${req.method} ${req.url}`);
        
        if (req.url === '/') {
            res.html(`
                <h1>Welcome to UltraScript!</h1>
                <p>Ultra-fast HTTP server with goroutine support</p>
                <a href="/api/test">Test API</a>
            `);
        }
        else if (req.url === '/api/test') {
            // Simulate some async work
            await runtime.time.sleep(10); // 10ms delay
            
            res.json({
                message: "Hello from UltraScript!",
                timestamp: runtime.time.now(),
                goroutine: true,
                performance: "ultra-fast"
            });
        }
        else if (req.url === '/api/upload' && req.method === 'POST') {
            const data = JSON.parse(req.body);
            
            // Process upload in parallel goroutines
            const results = await Promise.all([
                go processFile(data.file1),
                go validateData(data),
                go logRequest(req)
            ]);
            
            res.json({
                status: "success",
                processed: results[0],
                validation: results[1],
                logged: results[2]
            });
        }
        else {
            res.setStatus(404);
            res.json({ error: "Not found" });
        }
    }();
});

// Advanced server configuration
const serverConfig = {
    port: 8080,
    host: "0.0.0.0",
    maxConnections: 1000,
    threadPoolSize: 8,
    keepAliveTimeout: 30
};

// Start server with goroutine
go async function startServer() {
    await server.listen(serverConfig.port, serverConfig.host);
    console.log(`🚀 Server running on http://localhost:${serverConfig.port}`);
    
    // Server is now accepting connections in background goroutines
    // Each request gets its own goroutine automatically
}();

// Helper functions that run in goroutines
async function processFile(file: any): Promise<any> {
    // Simulate file processing
    await runtime.time.sleep(50);
    return { processed: true, size: file.size };
}

async function validateData(data: any): Promise<boolean> {
    // Simulate validation
    await runtime.time.sleep(20);
    return data !== null;
}

async function logRequest(req: HTTPRequest): Promise<void> {
    // Log to file system asynchronously
    const logEntry = `${runtime.time.now()} - ${req.method} ${req.url}\n`;
    await runtime.fs.writeFileAsync('/tmp/access.log', logEntry, { flag: 'a' });
}

// HTTP Client examples using runtime.http
go async function clientExamples() {
    // Simple GET request
    const response1 = await runtime.http.get('https://api.github.com/users/ultrascript');
    console.log('GitHub API response:', response1.body);
    
    // POST request with JSON data
    const postData = JSON.stringify({ name: "UltraScript", version: "1.0" });
    const response2 = await runtime.http.post('https://httpbin.org/post', postData);
    console.log('POST response:', response2.body);
    
    // Advanced request with custom headers
    const response3 = await runtime.http.request({
        method: 'PUT',
        url: 'https://httpbin.org/put',
        headers: {
            'Content-Type': 'application/json',
            'Authorization': 'Bearer token123'
        },
        body: JSON.stringify({ updated: true })
    });
    console.log('PUT response status:', response3.status);
}();

// WebSocket-style long polling with goroutines
go async function longPollingExample() {
    const clients: HTTPResponse[] = [];
    
    // Endpoint for long polling
    const pollServer = runtime.http.createServer((req, res) => {
        if (req.url === '/poll') {
            // Add client to polling list
            clients.push(res);
            
            // Set timeout to close connection after 30 seconds
            const timeoutId = setTimeout(() => {
                const index = clients.indexOf(res);
                if (index > -1) {
                    clients.splice(index, 1);
                    res.json({ timeout: true });
                }
            }, 30000);
            
            // Clean up on client disconnect
            req.on('close', () => {
                clearTimeout(timeoutId);
                const index = clients.indexOf(res);
                if (index > -1) clients.splice(index, 1);
            });
        }
        else if (req.url === '/broadcast' && req.method === 'POST') {
            // Broadcast to all waiting clients
            const message = JSON.parse(req.body);
            
            clients.forEach(clientRes => {
                clientRes.json({ message, timestamp: runtime.time.now() });
            });
            
            clients.length = 0; // Clear clients array
            res.json({ sent: true, clientCount: clients.length });
        }
    });
    
    await pollServer.listen(8081);
    console.log('📡 Long polling server running on port 8081');
}();

// Demonstrate goMap for parallel HTTP requests
go async function parallelRequestsExample() {
    const urls = [
        'https://httpbin.org/delay/1',
        'https://httpbin.org/delay/2', 
        'https://httpbin.org/delay/1',
        'https://httpbin.org/json'
    ];
    
    // Execute all requests in parallel using goMap
    console.time('Parallel HTTP requests');
    const responses = await urls.goMap(runtime.http.get);
    console.timeEnd('Parallel HTTP requests');
    
    console.log(`Completed ${responses.length} requests in parallel`);
    responses.forEach((response, index) => {
        console.log(`Request ${index + 1}: Status ${response.status}`);
    });
}();

// Performance monitoring
setInterval(() => {
    const stats = {
        memory: runtime.process.memoryUsage(),
        uptime: runtime.process.uptime(),
        activeHandles: runtime.process.activeHandles(),
        timestamp: runtime.time.now()
    };
    
    console.log('📊 Server performance:', stats);
}, 10000); // Every 10 seconds
