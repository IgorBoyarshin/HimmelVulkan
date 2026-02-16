#include <iostream>

#define TINYOBJLOADER_IMPLEMENTATION

#include "Himmel.h"


// #include "../libs/stb_image_write.h"

int main() {
    // int width, height, channels;
    // // NOTE Will force alpha even if it is not present
    // stbi_uc* pixels1 = stbi_load("../models/CoolCube/cube_normal-ogl.png", &width, &height, &channels, 3);
    // stbi_uc* pixels2 = stbi_load("../models/CoolCube/cube_height.png", &width, &height, &channels, 3);
    // assert(pixels1);
    // assert(pixels2);
    // stbi_uc* pixels3 = new stbi_uc[width * height * 4];
    // for (int y = 0; y < height; y+=1) {
    //     for (int x = 0; x < width; x++) {
    //         pixels3[4*(y * width + x) + 0] = pixels1[3*(y * width + x) + 0];
    //         pixels3[4*(y * width + x) + 1] = pixels1[3*(y * width + x) + 1];
    //         pixels3[4*(y * width + x) + 2] = pixels1[3*(y * width + x) + 2];
    //         pixels3[4*(y * width + x) + 3] = pixels2[3*(y * width + x) + 0];
    //     }
    // }
    // auto res = stbi_write_png("../models/CoolCube/cube_normal_height.png", width, height, 4, pixels3, width*4);
    // assert(res == 1);
    // stbi_image_free(pixels1);
    // stbi_image_free(pixels2);
    // delete[] pixels3;

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
