function outer(x) {
    let captured = x * 2;
    
    function inner() {
        return captured + 5;
    }
    
    return inner;
}

let closure = outer(10);
let result = closure();
console.log(result);
