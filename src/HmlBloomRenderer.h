#ifndef HML_BLOOM_RENDERER
#define HML_BLOOM_RENDERER

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


struct HmlBloomRenderer : HmlDrawer {
    VkDescriptorPool descriptorPool;
    std::vector<VkDescriptorSet>  descriptorSet_textures_0_perImage;
    // std::vector<VkDescriptorSetLayout> descriptorSetLayouts; // NOTE stores only 1 (for textures)
    VkDescriptorSetLayout descriptorSetLayoutTextures;

    std::vector<std::shared_ptr<HmlImageResource>> imageResources;


    std::vector<std::unique_ptr<HmlPipeline>> createPipelines(
        std::shared_ptr<HmlRenderPass> hmlRenderPass, const std::vector<VkDescriptorSetLayout>& descriptorSetLayouts) noexcept override;
    static std::unique_ptr<HmlBloomRenderer> create(std::shared_ptr<HmlContext> hmlContext) noexcept;
    ~HmlBloomRenderer() noexcept;
    void specify(
        const std::vector<std::shared_ptr<HmlImageResource>>& firstTextures,
        const std::vector<std::shared_ptr<HmlImageResource>>& secondTextures) noexcept;
    // void specify(const std::vector<std::shared_ptr<HmlImageResource>>& inputTextures) noexcept;
    VkCommandBuffer draw(const HmlFrameData& frameData) noexcept override;
};

#endif
