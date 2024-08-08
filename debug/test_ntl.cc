#include <iostream>
#include <NTL/ZZ.h>

using namespace NTL;

int main() {
    std::cout << "Size of NTL::ZZ: " << sizeof(NTL::ZZ) << " bytes" << std::endl;
    return 0;
}