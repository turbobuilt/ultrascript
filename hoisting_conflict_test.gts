function testHoisting() {
    console.log(x());  // Should print function definition
}
function x() {   // Function declaration 
    console.log("inside function x");
}

testHoisting();
