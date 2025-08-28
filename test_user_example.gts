var x;
var y;
function outer() {
  var a;
  var b; 
  var c;
  function inner() {
   // 3 accesses from depth 1: a, b, c
   // 2 accesses from depth 0: x, y
   return a + b + c + x + y;
  }
  return inner();
}

let result = outer();
