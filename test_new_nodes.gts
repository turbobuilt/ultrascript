// Test file for new AST node implementations

// Test ObjectLiteral
let obj = {
    name: "test",
    value: 42,
    active: true
};

// Test array literals
let numbers = [1, 2, 3, 4, 5];

// Test control flow
if (obj.active) {
    console.log("Object is active");
} else {
    console.log("Object is not active");
}

// Test for loop
for (let i = 0; i < 3; i++) {
    console.log("Loop iteration:", i);
}

// Test while loop
let count = 0;
while (count < 2) {
    console.log("While count:", count);
    count++;
}

// Test binary operations
let sum = 10 + 20;
let product = sum * 2;
let isEqual = sum == 30;

console.log("Sum:", sum);
console.log("Product:", product);
console.log("Is equal:", isEqual);
