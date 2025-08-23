class Dog {
    constructor() {
        console.log("Dog created");
    }
    destructor() {
        console.log("KILLING");
    }
}

console.log("Global code before main");

function main() {
    console.log("main start");
    var d = new Dog();
    console.log("dog created, about to exit main");
}

console.log("About to call main");
main();
console.log("main function returned");
