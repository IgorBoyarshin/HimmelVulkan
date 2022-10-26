#ifndef HML_DEFERRED_RENDERER
#define HML_DEFERRED_RENDERER

#include <memory>
#include <vector>
#include <iostream>
#include <unordered_map>
#include <optional>

#include "settings.h"
#include "HmlContext.h"
#include "HmlPipeline.h"
#include "HmlModel.h"
#include "HmlRenderPass.h"
#include "renderer.h"


// NOTE
// In theory, it is easily possible to also recreate desriptor stuff upon
// swapchain recreation by recreating the whole Renderer object.
// NOTE
struct HmlDeferredRenderer : HmlDrawer {
    VkDescriptorPool descriptorPool;
    std::vector<VkDescriptorSet>  descriptorSet_textures_1_perImage;
    // std::vector<VkDescriptorSetLayout> descriptorSetLayouts; // NOTE stores only 1 (for textures)
    VkDescriptorSetLayout descriptorSetLayoutTextures;

    std::vector<std::shared_ptr<HmlImageResource>> imageResources;


    static constexpr uint32_t G_COUNT = 4; // XXX must match the shader


    std::vector<std::unique_ptr<HmlPipeline>> createPipelines(
        std::shared_ptr<HmlRenderPass> hmlRenderPass, const std::vector<VkDescriptorSetLayout>& descriptorSetLayouts) noexcept override;
    static std::unique_ptr<HmlDeferredRenderer> create(
        std::shared_ptr<HmlContext> hmlContext,
        VkDescriptorSetLayout viewProjDescriptorSetLayout) noexcept;
    ~HmlDeferredRenderer() noexcept;
    void specify(const std::array<std::vector<std::shared_ptr<HmlImageResource>>, G_COUNT>& resources) noexcept;
    VkCommandBuffer draw(const HmlFrameData& frameData) noexcept override;
};

#endif
