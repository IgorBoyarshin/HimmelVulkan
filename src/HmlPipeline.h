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
    // const char* compute;

    inline HmlShaders& addVertex(const char* fileNameSpv) noexcept {
        vertex = fileNameSpv;
        return *this;
    }

    inline HmlShaders& addTessellationControl(const char* fileNameSpv) noexcept {
        tessellationControl = fileNameSpv;
        return *this;
    }

    inline HmlShaders& addTessellationEvaluation(const char* fileNameSpv) noexcept {
        tessellationEvaluation = fileNameSpv;
        return *this;
    }

    inline HmlShaders& addGeometry(const char* fileNameSpv) noexcept {
        geometry = fileNameSpv;
        return *this;
    }

    inline HmlShaders& addFragment(const char* fileNameSpv) noexcept {
        fragment = fileNameSpv;
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
};


struct HmlPipeline {
    VkPipelineLayout layout;
    VkPipeline pipeline;

    VkShaderStageFlags pushConstantsStages;

    std::shared_ptr<HmlDevice> hmlDevice;


    static std::unique_ptr<HmlPipeline> createGraphics(
            std::shared_ptr<HmlDevice> hmlDevice,
            HmlGraphicsPipelineConfig&& hmlPipelineConfig) noexcept;

    ~HmlPipeline() noexcept;

    private:

    std::optional<std::pair<std::vector<VkShaderModule>, std::vector<VkPipelineShaderStageCreateInfo>>>
        createShaders(const HmlShaders& hmlShaders) noexcept;
    static VkPipelineShaderStageCreateInfo createShaderStageInfo(VkShaderStageFlagBits stage, VkShaderModule shaderModule) noexcept;
    VkShaderModule createShaderModule(std::vector<char>&& code) noexcept;
    static std::vector<char> readFile(const char* fileName) noexcept;
};

#endif
