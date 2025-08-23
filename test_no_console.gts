function main() {
    let d = new Dog();
    // No console.log call - just assignment and cleanup
}

class Dog {
    destructor() {
        console.log("KILLING")
    }
}

main();
