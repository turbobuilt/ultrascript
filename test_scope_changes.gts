function outer() {
    let x = 10;  
    function inner() {
        console.log(x); 
        return x + 5;
    }
    return inner();
}

let result = outer();
console.log("Result: " + result);
