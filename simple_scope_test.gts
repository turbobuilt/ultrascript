function outer() {
    console.log("entering outer");
    function inner() {
        console.log("entering inner");
        var y = "inner var";
        console.log(y);
    }
    inner();
    var x = "outer var";
    console.log(x);
}
outer();
