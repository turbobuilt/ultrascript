// Test only int32 property without string

class IntTest {
    value: int32;
}

let test = new IntTest();
test.value = 42;
console.log("value:", test.value);
