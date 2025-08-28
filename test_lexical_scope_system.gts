// Test of the new unified lexical scope system
function test() {
    let outerVar = "outer";
    
    if (true) {
        let blockVar = "block";
        console.log(outerVar); // accessing outer scope
        
        for (let i = 0; i < 3; i++) {
            let loopVar = "loop";
            console.log(blockVar); // accessing block scope
            console.log(outerVar); // accessing outer scope
        }
    }
    
    const arrowFunc = (param) => {
        let arrowVar = "arrow";
        console.log(param);     // parameter access
        console.log(outerVar);  // outer scope access
        return param + arrowVar;
    };
    
    return arrowFunc("test");
}

test();
