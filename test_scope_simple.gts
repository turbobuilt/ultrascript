function outer() {
    let x = 10;  
    function inner() {
        return x + 5;
    }
    return inner();
}

let result = outer();
