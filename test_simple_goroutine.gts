function test() {
    console.log('TEST FUNCTION CALLED');
}

function main() {
    console.log('Before goroutine');
    go test();
    console.log('After goroutine');
}

main()