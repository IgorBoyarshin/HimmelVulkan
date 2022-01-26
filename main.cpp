#include <iostream>

#define TINYOBJLOADER_IMPLEMENTATION

#include "Himmel.h"


int main() {
    std::cout << "==================================== BEGIN ============================\n";
    {
        Himmel himmel;
        if (!himmel.init()) {
            std::cerr << "======== Failed to init Himmel ========\n";
            return -1;
        }
        if (!himmel.run()) {
            std::cerr << "======== Himmel terminated due to an error ========\n";
            return -1;
        }
    }
    std::cout << "====================================  END  ============================\n";
    return 0;
}
