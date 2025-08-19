#include "runtime.h"
#include <iostream>
int main() {
    std::cout << "UltraScriptString size: " << sizeof(UltraScriptString) << " bytes" << std::endl;
    std::cout << "SSO_THRESHOLD: " << UltraScriptString::SSO_THRESHOLD << " bytes" << std::endl;
    return 0;
}
