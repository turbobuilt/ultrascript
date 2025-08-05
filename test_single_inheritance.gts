class Animal {
    constructor(name) {
        this.name = name;
    }
    
    speak() {
        console.log("Animal speaks");
    }
}

class Dog extends Animal {
    constructor(name, breed) {
        super(name);
        this.breed = breed;
    }
    
    speak() {
        console.log("Woof! I am " + this.name);
    }
}

let d = new Dog("Buddy", "Golden Retriever");
d.speak();
