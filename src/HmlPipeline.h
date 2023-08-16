#ifndef HML_PIPELINE
#define HML_PIPELINE

#include <optional>
#include <iostream>
#include <vector>
#include <memory>
#include <fstream>

#include "settings.h"
#include "HmlDevice.h"


struct HmlShaders {
    const char* vertex                 = nullptr;
    const char* fragment               = nullptr;
    const char* tessellationControl    = nullptr;
    const char* tessellationEvaluation = nullptr;
    const char* geometry               = nullptr;
    std::optional<VkSpecializationInfo> vertexSpecializationInfo;
    std::optional<VkSpecializationInfo> fragmentSpecializationInfo;
    std::optional<VkSpecializationInfo> tessellationControlSpecializationInfo;
    std::optional<VkSpecializationInfo> tessellationEvaluationSpecializationInfo;
    std::optional<VkSpecializationInfo> geometrySpecializationInfo;

    inline HmlShaders& addVertex(const char* fileNameSpv,
            std::optional<VkSpecializationInfo> specializationInfo = std::nullopt) noexcept {
        vertex = fileNameSpv;
        vertexSpecializationInfo = specializationInfo;
        return *this;
    }

    inline HmlShaders& addTessellationControl(const char* fileNameSpv,
            std::optional<VkSpecializationInfo> specializationInfo = std::nullopt) noexcept {
        tessellationControl = fileNameSpv;
        tessellationControlSpecializationInfo = specializationInfo;
        return *this;
    }

    inline HmlShaders& addTessellationEvaluation(const char* fileNameSpv,
            std::optional<VkSpecializationInfo> specializationInfo = std::nullopt) noexcept {
        tessellationEvaluation = fileNameSpv;
        tessellationEvaluationSpecializationInfo = specializationInfo;
        return *this;
    }

    inline HmlShaders& addGeometry(const char* fileNameSpv,
            std::optional<VkSpecializationInfo> specializationInfo = std::nullopt) noexcept {
        geometry = fileNameSpv;
        geometrySpecializationInfo = specializationInfo;
        return *this;
    }

    inline HmlShaders& addFragment(const char* fileNameSpv,
            std::optional<VkSpecializationInfo> specializationInfo = std::nullopt) noexcept {
        fragment = fileNameSpv;
        fragmentSpecializationInfo = specializationInfo;
        return *this;
    }
};


struct HmlGraphicsPipelineConfig {
    std::vector<VkVertexInputBindingDescription>   bindingDescriptions;
    std::vector<VkVertexInputAttributeDescription> attributeDescriptions;
    VkPrimitiveTopology topology;
    HmlShaders hmlShaders;
    VkRenderPass renderPass;
    VkExtent2D extent;
    // VK_POLYGON_MODE_FILL: fill the area of the polygon with fragments
    // VK_POLYGON_MODE_LINE: polygon edges are drawn as lines. REQUIRES GPU FEATURE
    // VK_POLYGON_MODE_POINT: polygon vertices are drawn as points. REQUIRES GPU FEATURE
    VkPolygonMode polygoneMode;
    // VK_CULL_MODE_BACK_BIT
    // VK_CULL_MODE_NONE
    VkCullModeFlags cullMode;
    // VK_FRONT_FACE_COUNTER_CLOCKWISE
    // VK_FRONT_FACE_CLOCKWISE
    VkFrontFace frontFace;
    std::vector<VkDescriptorSetLayout> descriptorSetLayouts;

    // TODO Later will probably be an optional
    VkShaderStageFlags pushConstantsStages;
    uint32_t pushConstantsSizeBytes;

    // TODO Later will probably be an optional
    uint32_t tessellationPatchPoints;
    float lineWidth;

    uint32_t colorAttachmentCount;

    bool withBlending;
    bool withDepthTest;
};


struct HmlPipeline {
    using Id = uint32_t;
    Id id;

    VkPipelineLayout layout;
    VkPipeline pipeline;

    VkShaderStageFlags pushConstantsStages;

    std::shared_ptr<HmlDevice> hmlDevice;


    static std::unique_ptr<HmlPipeline> createGraphics(
            std::shared_ptr<HmlDevice> hmlDevice,
            HmlGraphicsPipelineConfig&& hmlPipelineConfig) noexcept;

    ~HmlPipeline() noexcept;

    inline bool operator==(const HmlPipeline& other) const {
        return (layout == other.layout)
            && (pipeline == other.pipeline)
            && (pushConstantsStages == other.pushConstantsStages);
    }

    private:

    std::optional<std::pair<std::vector<VkShaderModule>, std::vector<VkPipelineShaderStageCreateInfo>>>
        createShaders(const HmlShaders& hmlShaders) noexcept;
    static VkPipelineShaderStageCreateInfo createShaderStageInfo(VkShaderStageFlagBits stage,
        VkShaderModule shaderModule, const std::optional<VkSpecializationInfo>& specializationInfoOpt) noexcept;
    VkShaderModule createShaderModule(std::vector<char>&& code) noexcept;
    static std::vector<char> readFile(const char* fileName) noexcept;
    static Id newId() noexcept;
};


// template<> struct std::hash<HmlPipeline> {
//     inline size_t operator()(const HmlPipeline& hmlPipeline) const {
//         std::size_t res = 17;
//         res = res * 31 + hash<VkPipelineLayout>()(hmlPipeline.layout);
//         res = res * 31 + hash<VkPipeline>()(hmlPipeline.pipeline);
//         res = res * 31 + hash<VkShaderStageFlags>()(hmlPipeline.pushConstantsStages);
//         return res;
//     }
// };

#endif
