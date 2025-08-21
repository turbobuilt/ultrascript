// Test reference counting system

class Person {
    name: string;
    age: int64;
}

function main() {
    console.log("Testing reference counting system...");
    
    // Create an object
    let p = new Person();
    p.name = "Alice";
    p.age = 30;
    
    console.log("Created person:", p.name, "age", p.age);
    
    // Note: In actual UltraScript, objects would be automatically reference counted
    // For now, we're just testing the object creation with the new layout
    
    return 0;
}

main();
