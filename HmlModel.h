#ifndef HML_MODEL
#define HML_MODEL


#include <unordered_map>
#include <iostream>
#include <vector>
#include <vulkan/vulkan.hpp>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>

#include "libs/tiny_obj_loader.h"


struct HmlSimpleModel {
    struct Vertex {
        glm::vec3 pos;
        glm::vec3 color;
        glm::vec2 texCoord;

        static std::vector<VkVertexInputBindingDescription> getBindingDescriptions() noexcept;
        static std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions() noexcept;

        inline bool operator==(const Vertex& other) const noexcept {
            return (pos == other.pos) && (color == other.color) && (texCoord == other.texCoord);
        }
    };


    static bool load(const char* objPath, std::vector<HmlSimpleModel::Vertex>& vertices, std::vector<uint32_t>& indices) noexcept;
};


template<> struct std::hash<HmlSimpleModel::Vertex> {
    inline size_t operator()(const HmlSimpleModel::Vertex& vertex) const {
        return ((hash<glm::vec3>()(vertex.pos) ^
                (hash<glm::vec3>()(vertex.color) << 1)) >> 1) ^
                (hash<glm::vec2>()(vertex.texCoord) << 1);
    }
};


// XXX Other Models will have an inner Vertex struct as well

#endif
