function worker1() {
    console.log("Worker 1 running");
}

function worker2() {
    console.log("Worker 2 running");
}

function worker3() {
    console.log("Worker 3 running");
}

console.log("Spawning multiple goroutines");

go worker1();
go worker2();
go worker3();

console.log("All goroutines spawned");
