#ifndef HML_MODEL
#define HML_MODEL

#include <vulkan.hpp>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>


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
            bindingDescriptions.push_back(std::move(bindingDescription));

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
    };


    // NOTE There is no reason to keep storing this model data after creation.
    // NOTE We only need the indices count.
    // std::vector<Vertex> vertices;
    // std::vector<uint16_t> indices;
};


// XXX Other Models will have an inner Vertex struct as well


#endif
