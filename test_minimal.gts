class Dog {
    destructor() {
        console.log("KILLING")
    }
}

function main() {
    let d = new Dog();
    // Don't call any other functions that might corrupt the stack
}
main();
console.log("main done")
