#ifndef RENDERER_H
#define RENDERER_H

#include "HmlRenderPass.h"
#include "HmlPipeline.h"
#include "HmlDevice.h"


struct HmlFrameData {
    uint32_t frameIndex;
    uint32_t imageIndex;
    VkDescriptorSet generalDescriptorSet_0; // per image
};

struct HmlDrawer {
    virtual VkCommandBuffer draw(const HmlFrameData& frameData) noexcept = 0;
    // virtual void replaceRenderPass(std::shared_ptr<HmlRenderPass> newHmlRenderPass) noexcept = 0;

    std::shared_ptr<HmlRenderPass> currentRenderPass = { nullptr };
    std::vector<std::pair<std::shared_ptr<HmlRenderPass>, std::unique_ptr<HmlPipeline>>> pipelineForRenderPassStorage;
    inline const std::unique_ptr<HmlPipeline>& getCurrentPipeline() const noexcept {
        for (const auto& [renderPass, pipeline] : pipelineForRenderPassStorage) {
            if (renderPass == currentRenderPass) return pipeline;
        }
        assert(false && "::> Failed to look up a HmlPipeline for the current HmlRenderPass.\n");
    }

    inline size_t getCurrentRenderPassIndex() const noexcept {
        size_t i = 0;
        for (const auto& [renderPass, _pipeline] : pipelineForRenderPassStorage) {
            if (renderPass == currentRenderPass) return i;
            i++;
        }
        assert(false && "::> Failed to look up the index for the current HmlRenderPass.\n");
    }

    // virtual void selectRenderPass(std::shared_ptr<HmlRenderPass> renderPass) noexcept;
    // virtual void addRenderPass(std::shared_ptr<HmlRenderPass> newHmlRenderPass) noexcept;
    // virtual void clearRenderPasses() noexcept;

    std::shared_ptr<HmlDevice> hmlDevice;
    std::vector<VkDescriptorSetLayout> descriptorSetLayouts;

    virtual std::unique_ptr<HmlPipeline> createPipeline(std::shared_ptr<HmlDevice> hmlDevice, VkExtent2D extent,
        VkRenderPass renderPass, const std::vector<VkDescriptorSetLayout>& descriptorSetLayouts) noexcept = 0;

    inline virtual void selectRenderPass(std::shared_ptr<HmlRenderPass> renderPass) noexcept {
        currentRenderPass = renderPass;
    }

    inline virtual void addRenderPass(std::shared_ptr<HmlRenderPass> newHmlRenderPass) noexcept {
        auto hmlPipeline = createPipeline(hmlDevice, newHmlRenderPass->extent, newHmlRenderPass->renderPass, descriptorSetLayouts);
        pipelineForRenderPassStorage.emplace_back(newHmlRenderPass, std::move(hmlPipeline));
        currentRenderPass = newHmlRenderPass;
    }

    inline virtual void clearRenderPasses() noexcept {
        currentRenderPass.reset();
        pipelineForRenderPassStorage.clear();
    }
};

#endif
