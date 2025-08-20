class Dog {
    age: int64;
    name: string;
}
let d = new Dog();
d.age = 10;
d.name = "fido"; // uncommenting this prevents segfault.
d.dynamicProperty = "dynamic property!";

var str: string;
console.log("Starting for-in loop");
for let key in d {
    console.log("key is", key, "value is", d[key]);
    if (key == "name")
        str = d[key];
}
console.log("Finished for-in loop");
console.log("str is", str);
for let char of "bob" {
    console.log(char)
}