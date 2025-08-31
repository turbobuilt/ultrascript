function createFunc() {
    function inner() {
        console.log("Inner called");
    }
    return inner;
}

createFunc();
