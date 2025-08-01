function delayed_task() {
    console.log('Goroutine task started');
    console.log('Goroutine task completed');
}

function main() {
    console.log('Main function start');
    
    go delayed_task();
    console.log('Goroutine spawned');
    
    go delayed_task();
    console.log('Second goroutine spawned');
    
    console.log('Main function end');
}

main()
