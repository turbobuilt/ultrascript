class TestObject {
    name: string;
    constructor(name: string) {
        this.name = name;
    }
}

console.log("=== Simple memoryAddress Test ===");

var obj1 = new TestObject("obj1");
console.log("Created obj1");

var addr1 = obj1.memoryAddress;
console.log("addr1:", addr1);

// Don't call runtime.referenceCounter.getRefCount yet - just test memoryAddress first
console.log("Test complete - no runtime call");
