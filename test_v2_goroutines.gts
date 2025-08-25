console.log("Testing UltraScript V2 Goroutine System");

let x = 5;
let result = await go function() {
    console.log("Hello from goroutine!");
    x = 6;
    return 42;
}
console.log(result, "x is", x)

console.log("Main thread continues");
