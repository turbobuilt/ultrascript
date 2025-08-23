function worker(id) {
    console.log("Worker", id, "starting");
    console.log("Worker", id, "finished");
}

console.log("Spawning multiple goroutines");

go worker(1);
go worker(2);
go worker(3);

console.log("All goroutines spawned");
