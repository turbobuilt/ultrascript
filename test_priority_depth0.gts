var x;
var y;
var z;
function outer() {
  var a;
  function inner() {
   return a + x + y + z + x + y + z;
  }
  return inner();
}

let result = outer();
