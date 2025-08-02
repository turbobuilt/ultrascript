// Test all data types in property assignments

class AllTypesTest {
    // Integer types
    int8_value: int8;
    uint8_value: uint8;
    int16_value: int16;
    uint16_value: uint16;
    int32_value: int32;
    uint32_value: uint32;
    int64_value: int64;
    uint64_value: uint64;
    
    // Floating point types  
    float32_value: float32;
    float64_value: float64;
    
    // Boolean type
    bool_value: boolean;
    
    // String type
    string_value: string;
}

let test = new AllTypesTest();

// Test all integer types
test.int8_value = 127;
test.uint8_value = 255;
test.int16_value = 32767;
test.uint16_value = 65535;
test.int32_value = 2147483647;
test.uint32_value = 4294967295;
test.int64_value = 9223372036854775807;
test.uint64_value = 18446744073709551615;

// Test floating point types
test.float32_value = 3.14159;
test.float64_value = 2.718281828459045;

// Test boolean type
test.bool_value = true;

// Test string type
test.string_value = "Hello, World!";

// Output all values
console.log("int8_value:", test.int8_value);
console.log("uint8_value:", test.uint8_value);
console.log("int16_value:", test.int16_value);
console.log("uint16_value:", test.uint16_value);
console.log("int32_value:", test.int32_value);
console.log("uint32_value:", test.uint32_value);
console.log("int64_value:", test.int64_value);
console.log("uint64_value:", test.uint64_value);
console.log("float32_value:", test.float32_value);
console.log("float64_value:", test.float64_value);
console.log("bool_value:", test.bool_value);
console.log("string_value:", test.string_value);
