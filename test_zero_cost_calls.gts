// Test zero-cost function calls with the new patching system
function add(a, b) {
    return a + b;
}

function main() {
    let result = add(5, 3);
    console.log("Result:", result);
    return result;
}

main();
