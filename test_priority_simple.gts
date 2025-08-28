var x;
var y;
function outer() {
  var a, b, c;
  function inner() {
   return a + b + c + x + y;
  }
  return inner();
}

let result = outer();
