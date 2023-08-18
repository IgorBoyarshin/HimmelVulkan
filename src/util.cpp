#include "util.h"

namespace hml {}

// #include "../libs/stb_image_write.h"
// Combine different channels
// int width, height, channels;
// // NOTE Will force alpha even if it is not present
// stbi_uc* pixels = stbi_load("../models/CoolCube/rusty-ribbed-metal_metallic.png", &width, &height, &channels, 3);
// stbi_uc* pixels2 = stbi_load("../models/CoolCube/rusty-ribbed-metal_roughness.png", &width, &height, &channels, 3);
// assert(pixels);
// assert(pixels2);
// for (int y = 0; y < height; y+=1) {
//     for (int x = 0; x < width; x++) {
//         pixels[3*(y * width + x) + 0] = 0;
//         pixels[3*(y * width + x) + 1] = pixels2[3*(y * width + x) + 1];
//         // pixels[3*(y * width + x) + 2] = 0;
//     }
// }
// auto res = stbi_write_png("../models/CoolCube/igorek.png", width, height, 3, pixels, width*3);
// stbi_image_free(pixels);
// stbi_image_free(pixels2);
