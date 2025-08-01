function test() {
    console.log('TEST FUNCTION CALLED');
}

function main() {
    console.log('Before goroutine');
    console.log('About to spawn goroutine');
    go test();
    console.log('After goroutine spawn');
}
