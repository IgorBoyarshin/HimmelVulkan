#ifndef HML_DISPATCHER_H
#define HML_DISPATCHER_H

#include <chrono>
#include <vector>
#include <optional>
#include <functional>
#include <memory>

#include "HmlContext.h"
#include "HmlRenderPass.h"
#include "renderer.h"
#include "settings.h"


// Shared (CPU-GPU) data must be [maxFramesInFlight].
// Transient data (gBuffer, ...) can be singular.

// currentFrame == frameIndex ---> frameInFlightIndex
// imageIndex                 ---> swapchainImageIndex
struct HmlDispatcher {
    enum StageFlags {
        STAGE_NO_FLAGS = 0,
        STAGE_FLAG_FIRST_USAGE_OF_SWAPCHAIN_IMAGE = 0x1,
        STAGE_FLAG_LAST_USAGE_OF_SWAPCHAIN_IMAGE = 0x2,
    };


    struct DoStagesResult {
        float elapsedMicrosRecord;
        float elapsedMicrosSubmit;
    };


    struct FrameResult {
        struct Stats {
            float elapsedMicrosWaitNextInFlightFrame;
            float elapsedMicrosAcquire;
            float elapsedMicrosWaitSwapchainImage;
            float elapsedMicrosRecord;
            float elapsedMicrosSubmit;
            float elapsedMicrosPresent;
        };

        std::optional<Stats> stats;
        bool mustRecreateSwapchain;
    };


    struct StageCreateInfo {
        std::optional<std::function<void(bool, const HmlFrameData& frameData)>> preFunc;
        std::vector<std::shared_ptr<HmlDrawer>> drawers;
        std::optional<VkExtent2D> differentExtent;
        // std::vector<HmlRenderPass::ColorAttachment> inputAttachments;
        std::vector<HmlRenderPass::ColorAttachment> colorAttachments;
        std::optional<HmlRenderPass::DepthStencilAttachment> depthAttachment;
        std::vector<VkSubpassDependency> subpassDependencies;
        // std::vector<HmlTransitionRequest> postTransitions;
        std::optional<std::function<void(const HmlFrameData& frameData)>> postFunc;
        StageFlags flags;
    };


    // struct Transition {
    //     std::vector<std::shared_ptr<HmlImageResource>> imageResources;
    //     VkImageLayout postLayout;
    // };


    struct Stage {
        std::vector<std::shared_ptr<HmlDrawer>> drawers;
        std::vector<VkCommandBuffer> commandBuffers;
        std::shared_ptr<HmlRenderPass> renderPass;
        // std::vector<HmlTransitionRequest> postTransitions;
        std::optional<std::function<void(bool, const HmlFrameData& frameData)>> preFunc;
        std::optional<std::function<void(const HmlFrameData& frameData)>> postFunc;
        StageFlags flags;
        // std::vector<Transition> postTransitions;
    };


    std::shared_ptr<HmlContext> hmlContext;

    std::vector<VkDescriptorSet> generalDescriptorSet_0_perImage;

    std::unordered_set<std::shared_ptr<HmlDrawer>> clearedDrawers;
    std::vector<Stage> stages;

    std::function<void(uint32_t)> updateForImage;


    // NOTE former imageAvailableSemaphores
    std::vector<VkSemaphore> swapchainImageAvailable; // for each frame in flight
    // NOTE former renderFinishedSemaphores
    std::vector<VkSemaphore> renderToSwapchainImageFinished; // for each frame in flight
    // NOTE former inFlightFences
    std::vector<VkFence> finishedLastStageOf;               // for each frame in flight
    std::vector<VkFence> imagesInFlight;               // for each swapChainImage


    inline HmlDispatcher(std::shared_ptr<HmlContext> hmlContext,
            const std::vector<VkDescriptorSet>& generalDescriptorSet_0_perImage,
            std::function<void(uint32_t)>&& updateForImage) noexcept
            : hmlContext(hmlContext),
              generalDescriptorSet_0_perImage(generalDescriptorSet_0_perImage),
              updateForImage(std::move(updateForImage)) {
        createSyncObjects();
    }

    inline ~HmlDispatcher() noexcept {
#if LOG_DESTROYS
        std::cout << ":> Destroying HmlDispatcher...\n";
#endif
        for (auto semaphore : renderToSwapchainImageFinished) vkDestroySemaphore(hmlContext->hmlDevice->device, semaphore, nullptr);
        for (auto semaphore : swapchainImageAvailable) vkDestroySemaphore(hmlContext->hmlDevice->device, semaphore, nullptr);
        for (auto fence     : finishedLastStageOf) vkDestroyFence(hmlContext->hmlDevice->device, fence, nullptr);
    }

    bool addStage(StageCreateInfo&& stageCreateInfo) noexcept;
    std::optional<FrameResult> doFrame() noexcept;

    private:
    bool createSyncObjects() noexcept;
    std::optional<DoStagesResult> doStages(const HmlFrameData& frameData) noexcept;
};


#endif
