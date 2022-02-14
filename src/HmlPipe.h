#ifndef HML_PIPE_H
#define HML_PIPE_H

#include <vector>
#include <optional>
#include <functional>
#include <memory>

#include "HmlDevice.h"
#include "HmlCommands.h"
#include "HmlSwapchain.h"
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

    std::shared_ptr<HmlDevice> hmlDevice;
    std::shared_ptr<HmlCommands> hmlCommands;
    std::shared_ptr<HmlSwapchain> hmlSwapchain;

    std::vector<HmlStage> stages;
    std::vector<VkSemaphore> imageAvailableSemaphores; // for each frame in flight
    std::vector<VkSemaphore> renderFinishedSemaphores; // for each frame in flight
    std::vector<VkFence> inFlightFences; // for each frame in flight
    std::vector<std::vector<VkSemaphore>> semaphoresPerFrame;

    inline HmlPipe(
            std::shared_ptr<HmlDevice> hmlDevice,
            std::shared_ptr<HmlCommands> hmlCommands,
            std::shared_ptr<HmlSwapchain> hmlSwapchain,
            const std::vector<VkSemaphore>& imageAvailableSemaphores,
            const std::vector<VkSemaphore>& renderFinishedSemaphores,
            const std::vector<VkFence>& inFlightFences) noexcept
        : hmlDevice(hmlDevice), hmlCommands(hmlCommands), hmlSwapchain(hmlSwapchain),
          imageAvailableSemaphores(imageAvailableSemaphores),
          renderFinishedSemaphores(renderFinishedSemaphores),
          inFlightFences(inFlightFences) {}


    inline ~HmlPipe() noexcept {
        std::cout << ":> Destroying HmlPipe...\n";
        for (const auto& semaphores : semaphoresPerFrame) {
            for (const auto& semaphore : semaphores) {
                vkDestroySemaphore(hmlDevice->device, semaphore, nullptr);
            }
        }
    }


    void addStage(
        std::vector<std::shared_ptr<HmlDrawer>>&& drawers,
        std::vector<HmlRenderPass::ColorAttachment>&& colorAttachments,
        std::optional<HmlRenderPass::DepthStencilAttachment> depthAttachment,
        std::vector<HmlTransitionRequest>&& postTransitions,
        std::optional<std::function<void(uint32_t)>> postFunc) noexcept;
    bool verify() const noexcept;
    void assignRenderPasses() noexcept;
    bool createSemaphores(uint32_t maxFramesInFlight) noexcept;
    bool bake(uint32_t maxFramesInFlight) noexcept;
    void run(const HmlFrameData& frameData) noexcept;
};

#endif
