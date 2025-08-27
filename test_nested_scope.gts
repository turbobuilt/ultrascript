let x = 10;
function test() {
    let y = 20;
    {
        let z = 30;
        console.log(x + y + z);
    }
}
test();
