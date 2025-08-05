#include "runtime.h"
#include <iostream>
int main() {
    std::cout << "UltraScriptString size: " << sizeof(ultraScript::UltraScriptString) << " bytes" << std::endl;
    std::cout << "SSO_THRESHOLD: " << ultraScript::UltraScriptString::SSO_THRESHOLD << " bytes" << std::endl;
    return 0;
}
