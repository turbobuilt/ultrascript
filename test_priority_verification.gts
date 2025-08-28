var x;
var y; // scope depth 0 
function outer() {
  var a,b,c; // scope depth 1
  function inner() {
   console.log(a);
   console.log(b);  
   console.log(c);
   console.log(x);
   console.log(y);
  }
}

outer();
