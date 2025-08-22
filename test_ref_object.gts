class Person {
  constructor(name: string) {
    this.name = name;
  }
}

let x = new Person("Alice");
let y = x;  // Assignment should increment ref count
x = new Person("Bob");  // Should decrement Alice's ref count, create new Bob
console.log("Test completed");
