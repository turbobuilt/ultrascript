// Test case for descendant dependencies and priority sorting
let globalVar = "global";
let anotherGlobal = "another";

function outerFunction() {
    let outerVar = "outer";
    let outerVar2 = "outer2";
    
    function innerFunction() {
        let innerVar = "inner";
        
        // Access variables from different depths with varying frequencies
        console.log(globalVar);      // depth 0, accessed 1 time from depth 2
        console.log(globalVar);      // depth 0, accessed 2 times from depth 2  
        console.log(globalVar);      // depth 0, accessed 3 times from depth 2
        console.log(outerVar);       // depth 1, accessed 1 time from depth 2
        console.log(outerVar);       // depth 1, accessed 2 times from depth 2
        console.log(anotherGlobal);  // depth 0, accessed 1 time from depth 2
        
        {
            // Block scope at depth 3
            let blockVar = "block";
            
            // More accesses from deeper scope
            console.log(globalVar);      // depth 0, accessed 1 time from depth 3
            console.log(outerVar);       // depth 1, accessed 1 time from depth 3
            console.log(outerVar2);      // depth 1, accessed 1 time from depth 3
            console.log(innerVar);       // depth 2, accessed 1 time from depth 3
            console.log(anotherGlobal);  // depth 0, accessed 1 time from depth 3
            console.log(anotherGlobal);  // depth 0, accessed 2 times from depth 3
        }
        // Expected for depth 2: globalVar(4), anotherGlobal(3), outerVar(3), outerVar2(1), innerVar(1)
        // Priority should be: 0 (globalVar+anotherGlobal), 1 (outerVar+outerVar2), 2 (innerVar)
    }
    
    // Another function to test propagation
    function anotherInner() {
        console.log(outerVar2);      // depth 1, accessed 1 time from depth 2
        console.log(globalVar);      // depth 0, accessed 1 time from depth 2
    }
    
    innerFunction();
    anotherInner();
}

outerFunction();
