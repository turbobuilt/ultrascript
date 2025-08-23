class Dog {
    destructor() {
        console.log("KILLING")
    }
}

function main() {
    let d = new Dog();
    console.log("Dog created")
}
main();
console.log("main done")
