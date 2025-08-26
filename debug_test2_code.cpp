#include <iostream>
#include <string>

int main() {
    std::string test2_code = R"(
function forLoopOptimization() {
    for (var i = 0; i < 10; i++) {
        var temp = items[i];
        var result = process(temp);
    }
    
    for (let j = 0; j < 10; j++) {
        let value = items[j];
        const processed = transform(value);
    }
}
)";
    
    std::cout << "Test2 code:" << std::endl;
    std::cout << test2_code << std::endl;
    
    // Simple scope counting
    int scope_level = 0;
    for (char c : test2_code) {
        if (c == '{') scope_level++;
        else if (c == '}') scope_level = std::max(0, scope_level - 1);
    }
    
    std::cout << "Final scope level: " << scope_level << std::endl;
    
    return 0;
}
