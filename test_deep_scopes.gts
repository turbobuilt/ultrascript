// Test nested scope access patterns
function testNestedScopes() {
    let level1 = "level1";
    
    {
        let level2 = "level2";
        
        {
            let level3 = "level3";
            
            function deepFunction() {
                // This should trigger multiple scope pointer accesses
                console.log(level1); // 3 levels up
                console.log(level2); // 2 levels up  
                console.log(level3); // 1 level up
                
                let localVar = "local";
                console.log(localVar); // same level
            }
            
            deepFunction();
        }
    }
}

testNestedScopes();
