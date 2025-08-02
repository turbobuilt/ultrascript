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

Like JavaScript where "number" is a float64 by default, UltraScript treats all `number` type annotations as `float64`. UltraScript also allows specifying specific types from 32 bit to 64 bit. Additional support will be added later.

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

The program has a unified "Array" class that provides ultra-high performance for typed arrays while maintaining flexibility for untyped arrays. The Array class automatically detects whether it should use the typed or untyped path based on how it's constructed.

# Class architecture

ok so our jit could do this

class Person {
  name: string;
}
let bob: Person = new Person():
console.log(bob.name);

any time known properties are referenced, this could be jited just like c++ with direct offset for zero cost data access.

JS has to support accessing props by string as well like this:

var prop = "name";
console.log(bob[prop]);

To support this each instance of a class will additionally  store a map containing the prop name and the offset for each poperty name but would have to do a lookup to get the offest and use it.

JS also supports dynamic property addition and access. To support this, each object must also have a map on it that is map<string, DynamicValue>. Any properties stored on it should be converted by the JIT DynamicValue when copied with type and value;

If the JIT finds a property not initially registered on the class, it goes the slower path of looking it up in the map, returning undefined if it doesn't exist

bob.unregistered = "test"; // stores in map

we would know when doing the jit that this was an unregistered property (not on class).

So for fully typed class instances, the only overhead would be the extra map, but we could initialize that lazily.

In our system, all properties that are classes will be stored as pointers, requiring a bit more asm, but maintaining compatibility with js.

Inheritance will be supported. So the offsets work properly and you would check unregistered properties on the map recursively starting with the child and going to the parent.

## Unified Array Implementation

### Two Array Paths:

**1. Typed Arrays (Ultra Performance):**
```ultraScript
// Explicitly typed arrays - stored as contiguous memory like PyTorch tensors
var x: [int64] = [];  // Empty typed int64 array
var y: [float32] = [1.5, 2.5, 3.5];  // Typed float32 array

// Factory methods with dtype inference - automatically creates typed arrays
var zeros = Array.zeros([10,4,5], "int64");  // Creates typed int64 array
var ones = Array.ones([3, 3], "float32");    // Creates typed float32 array
var range = Array.arange(0, 100, 1, "int32"); // Creates typed int32 array
var linear = Array.linspace(0, 1, 50, "float64"); // Creates typed float64 array
```

**2. Untyped Arrays (Flexible):**
```ultraScript
// Dynamic arrays - can store any type, trades some performance for flexibility
var x = [];  // Untyped array - can store anything
x.push(1);    // Stores as number
x.push("hello");  // Can store strings too
x.push(3.14);     // Can store any type

// Factory methods without dtype create flexible arrays
var flexible = Array.zeros([5]);  // Creates untyped array filled with zeros
```

### Intelligent Type Inference:

The system uses a typed array if a type is specified
var x: [int64] = [];
It uses untyped if not specified
var x = [];


### Array Operations:

```ultraScript
// 1D operations (push/pop only work on 1D arrays)
var arr = [1, 2, 3];
arr.push(4);         // arr is now [1, 2, 3, 4]
var last = arr.pop(); // last = 4, arr is now [1, 2, 3]

// Properties work on any array
console.log(arr.length);  // 3 (size of first dimension)
console.log(arr.size);    // 3 (total number of elements)
console.log(arr.shape);   // [3] (shape array)
console.log(arr.ndim);    // 1 (number of dimensions)

// Multi-dimensional arrays
var matrix = Array.zeros([3, 4], { dtype: "float64" });
console.log(matrix.shape);  // [3, 4]
console.log(matrix.size);   // 12
console.log(matrix.ndim);   // 2

// Element access
var val1 = arr[0];           // 1D access: arr[index]
var val2 = matrix.at([1, 2]); // Multi-D access: matrix.at([row, col])

// Statistical operations (ultra-fast for typed arrays)
var data = Array.arange(1, 101, 1, { dtype: "float64" });
console.log(data.sum());     // 5050.0
console.log(data.mean());    // 50.5
console.log(data.max());     // 100.0
console.log(data.min());     // 1.0
```

### Performance Characteristics:

- **Typed arrays**: Use contiguous memory layout, SIMD optimizations, and zero-overhead type conversions
- **Untyped arrays**: Use variant storage for flexibility but still optimized for common operations
- **Type conversion**: JIT-compiled type casting with optimal performance paths
- **Memory management**: Automatic capacity doubling for push operations, efficient memory reuse

### PyTorch-style Slicing (Coming Soon):
```ultraScript
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
```

### Key Features:

1. **Single Array class** - handles both typed and untyped arrays seamlessly
2. **Automatic type inference** - from constructors like `Array.zeros([10], { dtype: "int64" })`
3. **Ultra performance** - typed arrays use optimal memory layout and SIMD operations
4. **Type safety** - typed arrays enforce type compatibility with intelligent casting
5. **JIT optimization** - type conversions are compiled to ultra-fast assembly code
6. **Memory efficiency** - contiguous storage for typed arrays, smart capacity management

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

Here the compiler would infer that x starts as a float64 and converts to a string. If this happens, first it will store next to the value of x that x is a float64 (the standard numeric type), and then it will store next to it that it is a string after added.

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

### HTTP Server Support

UltraScript now includes a high-performance HTTP server optimized for goroutines:

```ultraScript
// Create HTTP server with goroutine-optimized request handling
const server = runtime.http.createServer((req, res) => {
    go async function() {
        if (req.url === '/') {
            res.html('<h1>Hello UltraScript!</h1>');
        } else if (req.url === '/api/data') {
            // Process in parallel goroutines
            const [data1, data2] = await Promise.all([
                go fetchData1(),
                go fetchData2()
            ]);
            res.json({ data1, data2 });
        } else {
            res.setStatus(404);
            res.json({ error: "Not found" });
        }
    }();
});

await server.listen(8080);
console.log('Server running on http://localhost:8080');
```

The HTTP server features:
- **Multi-threaded connection handling** with configurable thread pools
- **Goroutine-optimized request processing** - each request runs in its own goroutine
- **High-performance request/response objects** with zero-copy operations
- **Full HTTP/1.1 support** including keep-alive, multiple methods, headers, body parsing
- **JSON/HTML response helpers** and static file serving
- **HTTP client functionality** for outbound requests
- **Promise-based APIs** that work seamlessly with `await` and `goMap`

Usage examples:
```ultraScript
// Parallel HTTP requests
const urls = ['http://api1.com', 'http://api2.com'];  
const responses = await urls.goMap(runtime.http.get);

// Server with parallel processing
const server = runtime.http.createServer((req, res) => {
    go async function() {
        if (req.method === 'POST' && req.url === '/process') {
            const data = JSON.parse(req.body);
            const results = await Promise.all([
                go processData(data),
                go validateInput(data),
                go logRequest(req)
            ]);
            res.json({ results });
        }
    }();
});
```

Use all cores when building make -j$(nproc)

For performance

- we use direct function pointers
- we use typed functions
- variables of type ANY store their type as part of DynamicValue
- when variables are explicity typed, or ar initialized as the return value of a function that has typed return, the type are kept while compiling and typed versions of functions are emitted for max perf directly


