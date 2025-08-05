class Animal {
    age: int64;
}

class Dog extends Animal {
    name: string;
}
let d = new Dog();
d.name = "fido"
d.age = 15
console.log(d.name);
console.log(d.age);