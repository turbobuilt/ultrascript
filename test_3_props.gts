class Dog {
    age: int64;
    name: string;
    weight: int64;
}

function main() {
    let d = new Dog();
    d.age = 10;
    d.name = "fido";
    d.weight = 50;
    d.dynamicProperty = "dynamic property!";
}
main();
console.log("main done")
