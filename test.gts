var x = [1];
console.log(x[0]);

var y: [int64] = [1];
console.log(y[0]);

function a() {
    console.log("in it is", x[0])
}

go a(x);