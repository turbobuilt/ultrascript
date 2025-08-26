console.log('Hello UltraScript with Register-Based Lexical Scope');
var x = 5;

let result = go function() {
    var y = 0;
    console.log(y);
    console.log("X is", x);
}