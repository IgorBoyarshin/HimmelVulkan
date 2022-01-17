#ifndef HML_MODEL
#define HML_MODEL

#include <vulkan.hpp>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>

#include "libs/tiny_obj_loader.h"

#include <unordered_map>


struct HmlSimpleModel {
    struct Vertex {
        glm::vec3 pos;
        glm::vec3 color;
        glm::vec2 texCoord;

        // Vertex() : pos({0.0f, 0.0f, 0.0f}), color({1.0f, 0.0f, 0.0f}), texCoord({0.0f, 0.0f}) {}
        // Vertex(const glm::vec2& pos, const glm::vec3& color, const glm::vec2& texCoord) : pos(pos), color(color), texCoord(texCoord) {}

        static std::vector<VkVertexInputBindingDescription> getBindingDescriptions() {
            std::vector<VkVertexInputBindingDescription> bindingDescriptions;

            VkVertexInputBindingDescription bindingDescription{};
            bindingDescription.binding = 0; // index of current Binding in the array of Bindings
            bindingDescription.stride = sizeof(Vertex);
            // VK_VERTEX_INPUT_RATE_VERTEX -- move to the next entry after each Vertex
            // VK_VERTEX_INPUT_RATE_INSTANCE -- move to the next entry after each Instance
            bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

            bindingDescriptions.push_back(bindingDescription);

            return bindingDescriptions;
        }


        static std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions() {
            std::vector<VkVertexInputAttributeDescription> attributeDescriptions{};
            attributeDescriptions.push_back(VkVertexInputAttributeDescription{
                .location = 0, // as in shader
                .binding = 0,
                .format = VK_FORMAT_R32G32B32_SFLOAT,
                .offset = offsetof(Vertex, pos)
            });
            attributeDescriptions.push_back(VkVertexInputAttributeDescription{
                .location = 1, // as in shader
                .binding = 0,
                .format = VK_FORMAT_R32G32B32_SFLOAT,
                .offset = offsetof(Vertex, color)
            });
            attributeDescriptions.push_back(VkVertexInputAttributeDescription{
                .location = 2, // as in shader
                .binding = 0,
                .format = VK_FORMAT_R32G32_SFLOAT,
                .offset = offsetof(Vertex, texCoord)
            });
            // Formats:
            // float: VK_FORMAT_R32_SFLOAT
            // vec2:  VK_FORMAT_R32G32_SFLOAT
            // vec3:  VK_FORMAT_R32G32B32_SFLOAT
            // vec4:  VK_FORMAT_R32G32B32A32_SFLOAT
            // double: VK_FORMAT_R64_SFLOAT
            // uvec4: VK_FORMAT_R32G32B32A32_UINT
            return attributeDescriptions;
        }


        bool operator==(const Vertex& other) const noexcept {
            return (pos == other.pos) && (color == other.color) && (texCoord == other.texCoord);
        }
    };


    // NOTE There is no reason to keep storing this model data after creation.
    // NOTE We only need the indices count.
    // std::vector<Vertex> vertices;
    // std::vector<uint16_t> indices;
};


template<> struct std::hash<HmlSimpleModel::Vertex> {
    size_t operator()(const HmlSimpleModel::Vertex& vertex) const {
        return ((hash<glm::vec3>()(vertex.pos) ^
                (hash<glm::vec3>()(vertex.color) << 1)) >> 1) ^
                (hash<glm::vec2>()(vertex.texCoord) << 1);
    }
};


bool loadSimpleModel(const char* objPath, std::vector<HmlSimpleModel::Vertex>& vertices, std::vector<uint32_t>& indices) {
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;

    if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, objPath)) {
        std::cerr << "::> Failed to load model " << objPath << ": " << warn << " " << err << "\n";
        return false;
    }

    std::unordered_map<HmlSimpleModel::Vertex, uint32_t> uniqueVertices{};
    for (const auto& shape : shapes) {
        for (const auto& index : shape.mesh.indices) {
            HmlSimpleModel::Vertex vertex{
                .pos = {
                    attrib.vertices[3 * index.vertex_index + 0],
                    attrib.vertices[3 * index.vertex_index + 1],
                    attrib.vertices[3 * index.vertex_index + 2]
                },
                .color = { 1.0f, 1.0f, 1.0f },
                .texCoord = {
                    attrib.texcoords[2 * index.texcoord_index + 0],
                    1.0f - attrib.texcoords[2 * index.texcoord_index + 1]
                }
            };

            if (uniqueVertices.count(vertex) == 0) {
                uniqueVertices[vertex] = static_cast<uint32_t>(vertices.size());
                vertices.push_back(vertex);
            }
            indices.push_back(uniqueVertices[vertex]);
        }
    }

    return true;
}


// XXX Other Models will have an inner Vertex struct as well

#endif
