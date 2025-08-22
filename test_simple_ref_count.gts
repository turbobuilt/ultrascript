class Dog {
    destructor() {
        console.log("dog destroyed");
    }
}

let x = new Dog();
let address: uint64 = x.memoryAddress;
console.log("Address:", address);

// Test without the problematic runtime call for now
console.log("Test complete");
