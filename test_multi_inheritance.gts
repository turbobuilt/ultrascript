class Flyable {
    altitude: int64;
    
    fly() {
        console.log("Flying at altitude:", altitude);
    }
}

class Swimmable {
    depth: int64;
    
    swim() {
        console.log("Swimming at depth:", depth);
    }
}

class Walkable {
    speed: int64;
    
    walk() {
        console.log("Walking at speed:", speed);
    }
}

class Duck extends Flyable, Swimmable, Walkable {
    name: string = "Daffy";
    
    quack() {
        console.log(name, "says quack!");
    }
}

let duck = new Duck();
duck.name = "Donald";
duck.altitude = 100;
duck.depth = 5;
duck.speed = 10;

console.log("Duck name:", duck.name);
duck.quack();
duck.fly();
duck.swim();
duck.walk();
