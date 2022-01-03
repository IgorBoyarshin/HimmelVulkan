#include <iostream>

#include "Himmel.h"


int main() {
    std::cout << "==================================== BEGIN ============================\n";
    {
        Himmel himmel;
        himmel.init();
    }
    std::cout << "====================================  END  ============================\n";
}
