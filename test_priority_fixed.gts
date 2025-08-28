var x;
var y;
function outer() {
  var a;
  var b;
  var c;
  function inner() {
   return a + b + c + x + y;
  }
  return inner();
}

let result = outer();
