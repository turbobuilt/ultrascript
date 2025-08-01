function test() {
    console.log('TEST FUNCTION CALLED');
}

function main() {
    console.log('Before goroutine');
    go test();
    console.log('After goroutine');
    // Add a simple delay to let goroutine execute
    for (let i = 0; i < 1000000; i++) {
        // Simple delay loop
    }
    console.log('Main finished');
}
