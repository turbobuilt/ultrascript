class Point {
    x: float64 = 0;
    y: float64 = 0;
}

let p = new Point();
console.log("Before assignment: p.x =", p.x);
p.x += 5;
console.log("After assignment: p.x =", p.x);
console.log("Assignment successful!");
