#include <iostream>

#define TINYOBJLOADER_IMPLEMENTATION

#include "Himmel.h"


int main() {
    std::cout << "==================================== BEGIN ============================\n";
    {
        Himmel himmel;
        himmel.init();
        himmel.run();
    }
    std::cout << "====================================  END  ============================\n";
}
