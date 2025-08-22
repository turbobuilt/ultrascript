class Dog {
    age: int64;
}

function main() {
    let d = new Dog();
    d.age = 10;
    d.dynamicProperty = "dynamic property!";
}
main();
console.log("main done")
