function doSomething() {
    console.log('doSomething called: ', 42);
}

function main() {
    console.log('Before goroutine');
    go doSomething();
    console.log('After goroutine');
}
