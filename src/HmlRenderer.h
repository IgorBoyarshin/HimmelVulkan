#ifndef HML_RENDERER
#define HML_RENDERER

#include <memory>
#include <vector>
#include <iostream>
#include <unordered_map>
#include <optional>

#include "HmlContext.h"
#include "HmlPipeline.h"
#include "HmlModel.h"
#include "HmlRenderPass.h"
#include "renderer.h"


// NOTE
// In theory, it is easily possible to also recreate desriptor stuff upon
// swapchain recreation by recreating the whole Renderer object.
// NOTE
struct HmlRenderer : HmlDrawer {
    struct PushConstant {
        alignas(16) glm::mat4 model;
        glm::vec4 color;
        int32_t textureIndex;
    };


    struct Entity {
        // NOTE We prefer to access the modelResource via a pointer rather than by
        // id because it allows for O(1) access speed.
        std::shared_ptr<HmlModelResource> modelResource;
        // HmlResourceManager::HmlModelResource::Id modelId;
        glm::mat4 modelMatrix;
        glm::vec3 color = { 1.0f, 1.0f, 1.0f };


        inline Entity(std::shared_ptr<HmlModelResource> modelResource)
            : modelResource(modelResource) {}
        inline Entity(std::shared_ptr<HmlModelResource> modelResource, const glm::vec3& color)
            : modelResource(modelResource), color(color) {}
        inline Entity(std::shared_ptr<HmlModelResource> modelResource, const glm::mat4& modelMatrix)
            : modelResource(modelResource), modelMatrix(modelMatrix) {}
    };


    VkDescriptorPool descriptorPool;
    VkDescriptorSet  descriptorSet_textures_1;
    // std::vector<VkDescriptorSetLayout> descriptorSetLayouts;
    VkDescriptorSetLayout descriptorSetLayoutTextures;


    static constexpr uint32_t MAX_TEXTURES_COUNT = 32; // XXX must match the shader. NOTE can be increased (80+)
    static constexpr int32_t NO_TEXTURE_MARK = -1;
    uint32_t nextFreeTextureIndex = 0;

    std::shared_ptr<HmlModelResource> anyModelWithTexture;
    std::unordered_map<HmlModelResource::Id, int32_t> textureIndexFor;
    std::unordered_map<HmlModelResource::Id, std::vector<std::shared_ptr<Entity>>> entitiesToRenderForModel;


    std::vector<std::unique_ptr<HmlPipeline>> createPipelines(
        std::shared_ptr<HmlRenderPass> hmlRenderPass, const std::vector<VkDescriptorSetLayout>& descriptorSetLayouts) noexcept override;
    static std::unique_ptr<HmlRenderer> create(std::shared_ptr<HmlContext> hmlContext, VkDescriptorSetLayout generalDescriptorSetLayout) noexcept;
    ~HmlRenderer() noexcept;
    void specifyEntitiesToRender(const std::vector<std::shared_ptr<Entity>>& entities) noexcept;
    void updateDescriptorSetTextures() noexcept;
    VkCommandBuffer draw(const HmlFrameData& frameData) noexcept override;
};

#endif
