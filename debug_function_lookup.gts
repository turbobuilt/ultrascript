function testFunc() {
  console.log("Hello from function");
}
console.log("Before assignment");
let f = testFunc;
console.log("After assignment");
f();
