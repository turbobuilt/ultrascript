UltraScript is a special language. It is super high performance, first and foremost.

It, like Go, uses high performance "goroutines" that execute from a threadpool for lightweight concurrency.

Yet it's syntax is fully backward compatible with javascript and typescript.

Like go, it allows passing data back and forth between goroutines. But like javascript, goroutines return promises so that they are easy to use.

It additionally supports goMap which is shorthand for a promise.all surrounding a function called with `go`.

Syntax examples:

```ultraScript
function doSomething(x: int64) {
    return x;
}

go doSomething(100); // waits till complete, then progam exits

await go doSomething(100); //waits till complete, but this time it's waiting at the code level, not program level.

let results = await Promise.all([1,2,3].goMap(doSomething));
```

The cool thing is these execute in parallel.


Planned: The goroutines also need to be able to function in extremely high performance.  Therefore functions can have the "fast" keyword. This keyword means the function only uses memory on the stack, can't use async functions or timers. As a result it generate ultra high performance assembly. functions can also be inline fast functions which mean they are used inline for maximum performance.

```ultraScript
fast function x(a,b) {
    var a = 0;
    var b = 1;
    var c = [];
    for (var i =0; i < 100; ++i) {
        c.push([i])
    }
    return a + b;
}
let result = await go x();
inline fast function x(a,b) {
    var a = 0;
    var b = 1;
    return a + b;
}
let result = await go x();
```

The problem with javascript is that it is slow due dynamic typing. Therefore UltraScript natively uses static typing when specified.

Unlike javascript where "number" is a float64 by default, UltraScript allows specifying not only `number`, but also all types from 32 bit to 64 bit. Additional support will be added later.

Types autocast. This is complicated to reason about, but the compiler will compute the types in each computation prior to execution of the program. Then the proper casting will take place. Types "cast up". float32*int32 casts up to float64 due to precision loss or size loss between float32 and int32.  int64*float64 casts up to float64 trading precision for ability to handle decimals (allowing tiny numbers).

When not autocasting, types are predicted for all computations, generating ultra high performance assembly level code. There will be multiple "backends", starting with webassembly and x86. The webassembly backend will exclusively write to SharedArrayBuffer so that all memory can be freely accessed between goroutines.

GoRoutines are designed to be javascripty, so things that are dangerous work. For example, a goroutine has access to all globals and all variables in the lexical scope. Callbacks, closures and all work as you would expect with a typical event loop, single thread implementation. But the code internally is running on multiple cores.

This poses safety challenges, which are resolved under the hood with atomics, mutexes, etc. Most importantly, the program never crashes because thread safety is fully implemented in an extremely high performance way.

The code does not contain ANY low performance "interpretation". Instead everything is JIT compiled on the fly with a custom compiler to the correct backend.

The way the compiler works is that it attempts to make insanely high performance, optimized code by using strong typing if possible. Then it generates code nearly as performant as C.

When not possible due to the programmer not specifying types (this happens in development), the system always trades memory for speed. In other words, it writes as much code inline as possible to determine the type instead of hopping around to different memory locations exeuting functions to detect types. This means that dynamic code will use extra memory inlining things, but it will work substantially faster, nearer to native speed.

Promise.all is able to work on both regular promises in the event loop, and also goroutines. This is somewhat complicated to implement, as each goroutine will have it's own event loop (independent of others).  However, the main event loop should be able to handle anything, including Promise.all resolving both items in the current event loop as promises, and other goroutines. This scheduling is complex but doable.

Additional syntax improvements over javascript include:
    - for and if do not require parenthesis when the starting information is on one line.
    - for example:
```ultraScript
if x == 5
    console.log("x is 5")
for let i: int64 = 0; i < 100; ++i
    console.log("i is", i)
additionally loops support advanced features:
for let varName of list
    print varName // type inferred from list and hyper compiled for speed
j = 5 // variables don't need "var".
```

Additional feature: operator overloading

```ultraScript
class Point {
    x: float64;
    y: float64 = 0;

    // typed version is super performant
    operator + (a: Point, b:Point) {
        return new Point{ x: a.x + b.x, y: a.y + b.y }
    }

    // jit compiler understands waht to do based on differnt types
    operator + (a: Point, b: float64) {
        return new Point{ x: a.x + b, y: a.y + b } 
    }
    
    // untyped versions work with anything. However, a is ALWAYS statically typed as a Point so that we know to use operator overloading 
    // slower, but sometimes the flexibility is really nice 
    operator + (a, b) { // a is Point, by is any 
        // coder can do whatever the heck they want with it here.
    }

    operator [] (p: Point, contents: string) {
        console.log(contents); // "anything goes here"
    }

    operator [:] (p: Point, slice: Slice) {
        // handles pytorch-style slicing: start:end:step
        return new Point{ x: p.x, y: p.y }  // example implementation
    }
    
    operator [] (p: Point, b) {
        console.log(b); // fallback handles any type
    }
}

let x = new Point();

x[anything goes here]
```

It may cause a few issues omitting var, but it's important for newbies that it exists like that.

The program has a built in "Array" type which mirrors pytorch perfectly. It contains array slicing, etc. For example

```ultraScript
var x: array = []; // any type
x.push(1) // pushes an element onto 0th dimension.
x.pop() // removes last element from 0th position
x.shape // returns [0]
x.length // returns x.shape[0]
// all other pytorch ops work
var y: [int64] = Array.zeros([10,4,5]) // create empty tensor with zeros of given shape
var y = Array.linspace(...)
var y = new Array(values, shape) // create tensor of shape with given values

// typed array
var y: [int32] = [1,2,3];

// support dot product, etc.

// Pytorch-style slicing with [:] operator
var z = y[:, 1:3, ::2];  // slice all first dim, 1-3 second dim, every 2nd third dim
var w = y[1:];           // slice from index 1 to end
var v = y[:5];           // slice from start to index 5
var u = y[::2];          // slice every 2nd element

// Slice type definition
type Slice = {
    start?: int64;    // optional start index (default: 0)
    end?: int64;      // optional end index (default: length)
    step?: int64;     // optional step size (default: 1)
}

// Usage examples:
var slice1 = new Slice{ start: 1, end: 5 };       // 1:5
var slice2 = new Slice{ step: 2 };                // ::2 
var slice3 = new Slice{ start: 10 };              // 10:
```

push/pop/length only supported on 1d array.

It supports keyword function parameters

```
function test(a,b,c) {
    console.log(a,b,c)
}

test(5, c=10) // b would be undefined
```

It supports creating objects with values like dart

```ultraScript
var x = new Person{ name: "bob" } // sets x.name to bob internally
```

It supports simple looping over object or array

```ultraScript
for each index|key, value in item { // 

}

Everything is carefully analyzed for types to generate correct assembly code on the fly. there is no "interpretation", meaning everything runs as direct machine code.

The system also extends the Date with with these important methods from momentjs.

new Date(...).add(1, "day|days").subtract(...).isBefore(...).isAfter(...).clone().format("...")

The formatter can be a bit tough, use the same substitions as momentjs.

When types are unspecified, a bit of data is stored next to the variable value regarding how to handle it. For example:

```ultraScript
var x = 5;
x += "world";
```

Here the compiler would infer that x starts as a number and converts to a string. If this happens, first it will store next to the value of x that x is a float64 (default number in js), and then it will store next to it that it is a string after added.

The code will support eval, compiling internally.

The code base is written in c++.

you write

```bash
ultraScript file.gts
```

and it executes it.

For checks and errors, we will have debug and production mode. 

```
./ultraScript -p (or --production) file.gts
```

Production mode will jit without checks for array access etc. Still do immediate crashes with stack trace on errors
Regular will jit generate code with checks for better debugging.

To test npm packages, copy the ultraScript binary to ./tests, cd into tests and run from there

When asked to implement ECMAscript things, you can reference ./ecmascript_reference.txt. It is a 10,000 line file, so searching is wise.

NodeJS compatibility. We are not going to implement any modules. Instead we will create a global called runtime. Then runtime will be an object with every possible call you will need to implement every node module. This object will then be used to create node modules in ultraScript.

Use all cores when building