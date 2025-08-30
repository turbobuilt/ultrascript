function test() {
    let x = 42;
    return x;
}

function withClosure() {
    let a = 10;
    
    function inner() {
        let b = 20;
        
        function deepInner() {
            return a + b + 30;
        }
        
        return deepInner();
    }
    
    return inner();
}

let arrow = () => {
    let y = 100;
    return y;
};

console.log("Testing function instance size computation");
