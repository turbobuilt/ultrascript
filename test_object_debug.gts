function main() {
    let d = new Dog();
    console.log("Object address debug:", d);  // This should print the object
}

class Dog {
    destructor() {
        console.log("KILLING")
    }
}

main();
