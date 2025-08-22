class TestObject {
    name: string;
    constructor(name: string) {
        this.name = name;
    }
}

console.log("=== Simple Runtime Test ===");

var obj1 = new TestObject("obj1");
console.log("Created obj1");

var addr1 = obj1.memoryAddress;
console.log("memoryAddress:", addr1);

console.log("About to call runtime.referenceCounter.getRefCount");
var ref_count = runtime.referenceCounter.getRefCount(addr1);
console.log("Got ref_count:", ref_count);

console.log("Test complete");
