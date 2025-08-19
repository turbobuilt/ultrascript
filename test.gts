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
d.unknown = "unknown";
console.log("d.unknown", d.unknown)
console.log("name is", d.name);
console.log('d["name"]', d["name"])
var prop = "name";
console.log(d[prop])
d.bark();
d.growl();

//for (let key in d) {
//    console.log(key);
//}