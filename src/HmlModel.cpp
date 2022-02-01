#include "HmlModel.h"



std::vector<VkVertexInputBindingDescription> HmlSimpleModel::Vertex::getBindingDescriptions() noexcept {
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


std::vector<VkVertexInputAttributeDescription> HmlSimpleModel::Vertex::getAttributeDescriptions() noexcept {
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
        .format = VK_FORMAT_R32G32_SFLOAT,
        .offset = offsetof(Vertex, texCoord)
    });
    attributeDescriptions.push_back(VkVertexInputAttributeDescription{
        .location = 2, // as in shader
        .binding = 0,
        .format = VK_FORMAT_R32G32B32_SFLOAT,
        .offset = offsetof(Vertex, normal)
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


bool HmlSimpleModel::load(const char* objPath, std::vector<HmlSimpleModel::Vertex>& vertices, std::vector<uint32_t>& indices) noexcept {
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;

    if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, objPath)) {
        std::cerr << "::> Failed to load model " << objPath << ": " << warn << " " << err << "\n";
        return false;
    }

    const bool withTexture = !attrib.texcoords.empty();
    std::unordered_map<HmlSimpleModel::Vertex, uint32_t> uniqueVertices{};
    for (const auto& shape : shapes) {
        for (const auto& index : shape.mesh.indices) {
            HmlSimpleModel::Vertex vertex{
                .pos = {
                    attrib.vertices[3 * index.vertex_index + 0],
                    attrib.vertices[3 * index.vertex_index + 1],
                    attrib.vertices[3 * index.vertex_index + 2]
                },
                // .color = { 1.0f, 1.0f, 1.0f },
                .texCoord = {
                    withTexture ? (       attrib.texcoords[2 * index.texcoord_index + 0]) : 0.0f,
                    withTexture ? (1.0f - attrib.texcoords[2 * index.texcoord_index + 1]) : 0.0f
                },
                .normal = {
                    attrib.normals[3 * index.normal_index + 0],
                    attrib.normals[3 * index.normal_index + 1],
                    attrib.normals[3 * index.normal_index + 2]
                },
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
