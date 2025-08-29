{
    let outerVar = 42;
    let shared = "outer";
    
    {
        let innerVar = outerVar + 10;
        let shared = "inner";
        console.log(innerVar);
        console.log(shared);
        
        {
            let deepVar = innerVar + outerVar;
            console.log(deepVar);
            console.log(shared);
        }
        
        console.log(innerVar);
    }
    
    console.log(shared);
    console.log(outerVar);
}
