#ifndef HML_UI_RENDERER
#define HML_UI_RENDERER

#include <memory>
#include <vector>
#include <iostream>
#include <unordered_map>
#include <optional>

#include "settings.h"
#include "util.h"
#include "HmlContext.h"
#include "HmlPipeline.h"
#include "HmlModel.h"
#include "HmlRenderPass.h"
#include "renderer.h"


// NOTE
// In theory, it is easily possible to also recreate desriptor stuff upon
// swapchain recreation by recreating the whole Renderer object.
// NOTE
struct HmlUiRenderer : HmlDrawer {
    struct PushConstant {
        int32_t textureIndex;
        float shift;
    };


    VkDescriptorPool descriptorPool;
    std::vector<VkDescriptorSet>  descriptorSet_textures_0_perImage;
    // std::vector<VkDescriptorSetLayout> descriptorSetLayouts; // NOTE stores only 1 (for textures)
    VkDescriptorSetLayout descriptorSetLayoutTextures;

    // std::unique_ptr<HmlImageResource> textureResource;
    std::vector<std::shared_ptr<HmlImageResource>> imageResources;


    static constexpr uint32_t MAX_TEXTURES_COUNT = 5; // XXX must match the shader
    // static constexpr int32_t NO_TEXTURE_MARK = -1;
    uint32_t texturesCount = 0;

    // std::shared_ptr<HmlModelResource> anyModelWithTexture;
    // std::unordered_map<HmlModelResource::Id, int32_t> textureIndexFor;
    // std::unordered_map<HmlModelResource::Id, std::vector<std::shared_ptr<Entity>>> entitiesToRenderForModel;


    std::vector<std::unique_ptr<HmlPipeline>> createPipelines(
        std::shared_ptr<HmlRenderPass> hmlRenderPass, const std::vector<VkDescriptorSetLayout>& descriptorSetLayouts) noexcept override;
    static std::unique_ptr<HmlUiRenderer> create(std::shared_ptr<HmlContext> hmlContext) noexcept;
    virtual ~HmlUiRenderer() noexcept;
    void specify(const std::vector<std::vector<std::shared_ptr<HmlImageResource>>>& resources) noexcept;
    VkCommandBuffer draw(const HmlFrameData& frameData) noexcept override;
};

#endif
