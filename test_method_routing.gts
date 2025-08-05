class Animal {
    speak() {
        console.log("Animal speaks");
    }
}

class Dog extends Animal {
    bark() {
        console.log("Woof!");
    }
}

let d = new Dog();
d.speak();  // This should call Animal.speak method
d.bark();   // This should call Dog.bark method
