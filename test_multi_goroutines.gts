console.log("Testing Multiple Goroutines with V2 System");

go function() {
    console.log("Goroutine 1 starting");
    return 1;
}

go function() {
    console.log("Goroutine 2 starting"); 
    return 2;
}

go function() {
    console.log("Goroutine 3 starting");
    return 3;
}

console.log("All goroutines spawned, main continues");
