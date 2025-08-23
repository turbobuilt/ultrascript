class Dog {
    age: int64;
    name: string;
    destructor() {
        console.log("KILLING")
    }
}

go function() {
    let d = new Dog();
    d.age = 10;
    d.name = "fido"; // uncommenting this prevents segfault.
    d.dynamicProperty = "dynamic property!";
    console.log("dynamic property is", d.dynamicProperty)
}
console.log("main done")