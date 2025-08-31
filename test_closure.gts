function outer(x) {
    function inner() {
        console.log("Outer value: ", x);
    }
    return inner;
}

let closure = outer(42);
closure();
