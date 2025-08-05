// Test inheritance optimization performance

// Single inheritance - should reuse parent methods (fast!)
class Animal {
    age: number;
    
    getAge() {
        return this.age;
    }
    
    speak() {
        console.log("Animal speaks");
    }
}

class Dog extends Animal {
    breed: string;
    
    bark() {
        console.log("Woof!");
    }
}

// Multiple inheritance - needs specialized methods
class Friend {
    friendYears: number;
    
    printFriendYears() {
        console.log("friend years " + this.friendYears);
    }
}

class Cat extends Animal, Friend {
    indoor: boolean;
    
    meow() {
        console.log("Meow!");
    }
}

function main() {
    // Test single inheritance (should use optimization)
    let d = new Dog();
    d.age = 5;
    d.breed = "Golden Retriever";
    
    console.log("=== SINGLE INHERITANCE TEST ===");
    console.log("Dog age: " + d.getAge()); // Should use parent method directly
    d.speak(); // Should use parent method directly
    d.bark();
    
    // Test multiple inheritance (should use specialized methods)
    let c = new Cat();
    c.age = 3;
    c.friendYears = 10;
    c.indoor = true;
    
    console.log("=== MULTIPLE INHERITANCE TEST ===");
    console.log("Cat age: " + c.getAge()); // Should use specialized method
    c.speak(); // Should use specialized method
    c.printFriendYears(); // Should use specialized method  
    c.meow();
    
    console.log("Tests completed!");
}
