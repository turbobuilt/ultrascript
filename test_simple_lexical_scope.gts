// Simple test of the lexical scope system
function test() {
    let outerVar = "outer";
    
    if (true) {
        let blockVar = "block";
        console.log(outerVar); // accessing outer scope
        
        for (let i = 0; i < 2; i++) {
            let loopVar = "loop";
            console.log(blockVar); // accessing block scope
            console.log(outerVar); // accessing outer scope
        }
    }
    
    while (false) {
        let whileVar = "while";
        console.log(outerVar);
    }
    
    return outerVar;
}

test();
