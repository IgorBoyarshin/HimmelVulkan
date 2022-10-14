#ifndef HML_PIPE_H
#define HML_PIPE_H

#include <vector>
#include <optional>
#include <functional>
#include <memory>
#include <unordered_set>

#include "HmlContext.h"
#include "HmlRenderPass.h"
#include "renderer.h"


struct HmlPipe {
    struct HmlTransitionRequest {
        std::vector<std::shared_ptr<HmlImageResource>> resourcePerImage;
        VkImageLayout dstLayout;
    };

    struct HmlStage {
        std::vector<std::shared_ptr<HmlDrawer>> drawers;
        std::vector<VkCommandBuffer> commandBuffers;
        std::shared_ptr<HmlRenderPass> renderPass;
        std::vector<HmlTransitionRequest> postTransitions;
        std::optional<std::function<void(uint32_t)>> postFunc;
    };

    std::shared_ptr<HmlContext> hmlContext;

    std::unordered_set<std::shared_ptr<HmlDrawer>> clearedDrawers;

    std::vector<HmlStage> stages;
    std::vector<VkSemaphore> imageAvailableSemaphores; // for each frame in flight
    std::vector<VkSemaphore> renderFinishedSemaphores; // for each frame in flight
    std::vector<VkFence> inFlightFences; // for each frame in flight
    // NOTE It is a flattened array of arrays (in order to allow easy addition of new semaphores)
    std::vector<VkSemaphore> semaphoresForFrames;

    inline HmlPipe(
            std::shared_ptr<HmlContext> hmlContext,
            const std::vector<VkSemaphore>& imageAvailableSemaphores,
            const std::vector<VkSemaphore>& renderFinishedSemaphores,
            const std::vector<VkFence>& inFlightFences) noexcept
        : hmlContext(hmlContext),
          imageAvailableSemaphores(imageAvailableSemaphores),
          renderFinishedSemaphores(renderFinishedSemaphores),
          inFlightFences(inFlightFences) {}


    inline ~HmlPipe() noexcept {
        std::cout << ":> Destroying HmlPipe...\n";
        for (const auto& semaphore : semaphoresForFrames) {
            vkDestroySemaphore(hmlContext->hmlDevice->device, semaphore, nullptr);
        }
    }


    bool addStage(
        std::vector<std::shared_ptr<HmlDrawer>>&& drawers,
        std::vector<HmlRenderPass::ColorAttachment>&& colorAttachments,
        std::optional<HmlRenderPass::DepthStencilAttachment> depthAttachment,
        std::vector<HmlTransitionRequest>&& postTransitions,
        std::optional<std::function<void(uint32_t)>> postFunc) noexcept;
    void run(const HmlFrameData& frameData) noexcept;

    private:
    bool addSemaphoresForNewStage() noexcept;
    inline VkSemaphore semaphoreFinishedStageOfFrame(size_t stage, size_t frame) const noexcept {
        return semaphoresForFrames[frame * (stages.size() - 1) + stage];
    }
};

#endif
