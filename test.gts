class Dog {
    age: int64;
    name: string;
}

function main() {
    let d = new Dog();
    d.age = 10;
    d.name = "fido"; // uncommenting this prevents segfault.
    d.dynamicProperty = "dynamic property!";
}
main();
console.log("main done")