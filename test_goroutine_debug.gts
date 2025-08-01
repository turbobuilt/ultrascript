function doSomething() {
    console.log('GOROUTINE EXECUTING: doSomething called');
}

function main() {
    console.log('Before goroutine spawn');
    go doSomething();
    console.log('After goroutine spawn');
    console.log('Main function finished');
}
