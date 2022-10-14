#ifndef RENDERER_H
#define RENDERER_H

#include "HmlRenderPass.h"
#include "HmlPipeline.h"
#include "HmlDevice.h"
#include "HmlContext.h"


struct HmlFrameData {
    uint32_t frameIndex;
    uint32_t imageIndex;
    VkDescriptorSet generalDescriptorSet_0; // per image
};

struct HmlDrawer {
    virtual VkCommandBuffer draw(const HmlFrameData& frameData) noexcept = 0;


    std::shared_ptr<HmlContext> hmlContext;

    // This allows not passing the RenderPass as the argument to the draw().
    // But we have to call selectRenderPass() before each draw.
    std::shared_ptr<HmlRenderPass> currentRenderPass = { nullptr };

    // There can be multiple Pipelines for a RenderPass.
    // TODO do we really need to store the RenderPasses, cant we just do a simple vector as for commands??
    std::vector<std::pair<std::shared_ptr<HmlRenderPass>, std::vector<std::unique_ptr<HmlPipeline>>>> pipelineForRenderPassStorage;

    inline const std::vector<std::unique_ptr<HmlPipeline>>& getCurrentPipelines() const noexcept {
        for (const auto& [renderPass, pipelines] : pipelineForRenderPassStorage) {
            if (renderPass == currentRenderPass) return pipelines;
        }
        assert(false && "::> Failed to look up a HmlPipeline for the current HmlRenderPass.\n");
    }

    inline size_t getCurrentRenderPassIndex() const noexcept {
        size_t i = 0;
        for (const auto& [renderPass, _pipelines] : pipelineForRenderPassStorage) {
            if (renderPass == currentRenderPass) return i;
            i++;
        }
        assert(false && "::> Failed to look up the index for the current HmlRenderPass.\n");
    }

    inline const std::vector<VkCommandBuffer>& getCurrentCommands() const noexcept {
        assert(getCurrentRenderPassIndex() < commandBuffersForRenderPass.size()
            && "::> Current RenderPass index is out of bounds for commandBuffersForRenderPass.\n");
        return commandBuffersForRenderPass[getCurrentRenderPassIndex()];
    }

    std::shared_ptr<HmlDevice> hmlDevice;
    std::vector<VkDescriptorSetLayout> descriptorSetLayouts;

    std::vector<std::vector<VkCommandBuffer>> commandBuffersForRenderPass;

    virtual std::vector<std::unique_ptr<HmlPipeline>> createPipelines(
        std::shared_ptr<HmlRenderPass> hmlRenderPass, const std::vector<VkDescriptorSetLayout>& descriptorSetLayouts) noexcept = 0;

    // TODO remove function and add argument to draw??
    inline virtual void selectRenderPass(std::shared_ptr<HmlRenderPass> renderPass) noexcept {
        currentRenderPass = renderPass;
    }

    inline virtual void addRenderPass(std::shared_ptr<HmlRenderPass> newHmlRenderPass) noexcept {
        currentRenderPass = newHmlRenderPass;

        auto hmlPipelines = createPipelines(newHmlRenderPass, descriptorSetLayouts);
        pipelineForRenderPassStorage.emplace_back(newHmlRenderPass, std::move(hmlPipelines));

        const auto count = hmlContext->framesInFlight();
        const auto pool = hmlContext->hmlCommands->commandPoolOnetimeFrames;
        auto commands = hmlContext->hmlCommands->allocateSecondary(count, pool);
        commandBuffersForRenderPass.push_back(std::move(commands));
    }

    inline virtual void clearRenderPasses() noexcept {
        currentRenderPass.reset();
        pipelineForRenderPassStorage.clear();
        commandBuffersForRenderPass.clear();
    }
};

#endif
