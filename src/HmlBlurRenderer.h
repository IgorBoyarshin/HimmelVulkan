#if 0

#ifndef HML_BLUR_RENDERER
#define HML_BLUR_RENDERER

#include <memory>
#include <vector>
#include <iostream>
#include <unordered_map>
#include <optional>

#include "HmlWindow.h"
#include "HmlPipeline.h"
#include "HmlCommands.h"
#include "HmlModel.h"
#include "HmlDevice.h"
#include "HmlResourceManager.h"
#include "HmlDescriptors.h"
#include "HmlRenderPass.h"
#include "renderer.h"


struct HmlBlurRenderer : HmlDrawer {
    struct PushConstant {
        uint32_t isHorizontal;
    };


    std::shared_ptr<HmlWindow> hmlWindow;
    // std::shared_ptr<HmlDevice> hmlDevice;
    std::shared_ptr<HmlCommands> hmlCommands;
    // std::shared_ptr<HmlRenderPass> hmlRenderPass;
    std::shared_ptr<HmlResourceManager> hmlResourceManager;
    std::shared_ptr<HmlDescriptors> hmlDescriptors;

    // std::unique_ptr<HmlPipeline> hmlPipeline;

    VkDescriptorPool descriptorPool;
    std::vector<VkDescriptorSet>  descriptorSet_first_0_perImage;
    std::vector<VkDescriptorSet>  descriptorSet_second_0_perImage;
    // std::vector<VkDescriptorSetLayout> descriptorSetLayouts; // NOTE stores only 1 (for textures)
    VkDescriptorSetLayout descriptorSetLayoutTextures;

    std::vector<std::shared_ptr<HmlImageResource>> imageResources;

    bool isVertical = true;
    uint32_t framesInFlight; // TODO XXX


    std::vector<std::vector<VkCommandBuffer>> commandBuffersPerRenderPass;

    inline void addRenderPass(std::shared_ptr<HmlRenderPass> newHmlRenderPass) noexcept override {
        auto hmlPipeline = createPipeline(hmlDevice, newHmlRenderPass->extent, newHmlRenderPass->renderPass, descriptorSetLayouts);
        pipelineForRenderPassStorage.emplace_back(newHmlRenderPass, std::move(hmlPipeline));
        currentRenderPass = newHmlRenderPass;
        commandBuffersPerRenderPass.push_back(hmlCommands->allocateSecondary(framesInFlight, hmlCommands->commandPoolOnetimeFrames));
    }

    inline virtual void clearRenderPasses() noexcept {
        currentRenderPass.reset();
        pipelineForRenderPassStorage.clear();
        commandBuffersPerRenderPass.clear();
    }



    std::unique_ptr<HmlPipeline> createPipeline(std::shared_ptr<HmlDevice> hmlDevice, VkExtent2D extent,
        VkRenderPass renderPass, const std::vector<VkDescriptorSetLayout>& descriptorSetLayouts) noexcept override;
    static std::unique_ptr<HmlBlurRenderer> create(
        std::shared_ptr<HmlWindow> hmlWindow,
        std::shared_ptr<HmlDevice> hmlDevice,
        std::shared_ptr<HmlCommands> hmlCommands,
        // std::shared_ptr<HmlRenderPass> hmlRenderPass,
        std::shared_ptr<HmlResourceManager> hmlResourceManager,
        std::shared_ptr<HmlDescriptors> hmlDescriptors,
        uint32_t imageCount,
        uint32_t framesInFlight) noexcept;
    ~HmlBlurRenderer() noexcept;
    void specify(
        const std::vector<std::shared_ptr<HmlImageResource>>& firstTextures,
        const std::vector<std::shared_ptr<HmlImageResource>>& secondTextures) noexcept;
    void modeHorizontal() noexcept;
    void modeVertical() noexcept;
    VkCommandBuffer draw(const HmlFrameData& frameData) noexcept override;
    // void replaceRenderPass(std::shared_ptr<HmlRenderPass> newHmlRenderPass) noexcept override;
};

#endif

#endif
