// Test that regular free (deep) throws an error
function test() {
    var x = [1, 2, 3];
    free x;  // This should cause a parse error
}

test();
