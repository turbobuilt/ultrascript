class Animal {
    age: int64;

    growl() {
        console.log("grrr");
    }
}

class Friend {
    friendYears: int64;

    printFriendYears() {
        console.log("friend years", this.friendYears);
    }
}

class Dog extends Animal {
    name: string = "'ol pal"
    bark() {
        console.log("woof")
    }
}
let d = new Dog();
console.log("initial name", d.name)
d.name = "fido"
console.log(d.name);
d.bark();
d.growl();

for (let key in d) {
    console.log(key);
}