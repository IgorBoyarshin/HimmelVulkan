#include "HmlDispatcher.h"


/*
 * "In flight" -- "in GPU rendering pipeline": either waiting to be rendered,
 * or already being rendered; but not finished rendering yet!
 * So, its commandBuffer has been submitted, but has not finished yet.
 * */
std::optional<HmlDispatcher::FrameResult> HmlDispatcher::doFrame() noexcept {
    static uint32_t frameInFlightIndex = 0;

    // Wait for next-in-order frame to become rendered (for its commandBuffer
    // to finish). This ensures that no more than MAX_FRAMES_IN_FLIGHT frames
    // are inside the rendering pipeline at the same time.
    const auto startWaitNextInFlightFrame = std::chrono::high_resolution_clock::now();
    vkWaitForFences(hmlContext->hmlDevice->device, 1, &finishedLastStageOf[frameInFlightIndex], VK_TRUE, UINT64_MAX);
    const auto endWaitNextInFlightFrame = std::chrono::high_resolution_clock::now();

    // vkAcquireNextImageKHR only specifies which image will be made
    // available next, so that we can e.g. start recording command
    // buffers that reference this image. However, the image itself may
    // not be available yet. So we use (preferably) a semaphore in order
    // to tell the commands when the image actually becomes available.
    // Thus we keep the syncronization on the GPU, improving the performance.
    // If we for some reason wanted to actually syncronize with CPU, we'd
    // use a fence.
    uint32_t imageIndex; // the available image we will be given by the presentation engine from the swapchain
    // The next-in-order swapchainImageAvailable has already retired because
    // its inFlightFence has just been waited upon.
    const auto startAcquire = std::chrono::high_resolution_clock::now();
    if (const VkResult result = vkAcquireNextImageKHR(hmlContext->hmlDevice->device, hmlContext->hmlSwapchain->swapchain, UINT64_MAX,
                swapchainImageAvailable[frameInFlightIndex], VK_NULL_HANDLE, &imageIndex);
            result == VK_ERROR_OUT_OF_DATE_KHR) {
        return { FrameResult{
            .stats = std::nullopt,
            .mustRecreateSwapchain = true,
        }};
    } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        std::cerr << "::> Failed to acquire swap chain image.\n";
        return std::nullopt;
    }
    const auto endAcquire = std::chrono::high_resolution_clock::now();

    const auto startWaitSwapchainImage = std::chrono::high_resolution_clock::now();
    if (imagesInFlight[imageIndex] != VK_NULL_HANDLE) [[likely]] {
        // For each image we submit, we track which inFlightFence was bound
        // to it; for cases when the to-be-acquired image is still in use by
        // the pipeline (too slow pipeline; out-of-order acquisition;
        // MAX_FRAMES_IN_FLIGHT > swapChainImages.size()) -- we wait until
        // this particular image exits the pipeline.
        vkWaitForFences(hmlContext->hmlDevice->device, 1, &imagesInFlight[imageIndex], VK_TRUE, UINT64_MAX);
    }
    const auto endWaitSwapchainImage = std::chrono::high_resolution_clock::now();

    // The image has at least finished being rendered.
    // Mark the image as now being in use by this frame
    imagesInFlight[imageIndex] = finishedLastStageOf[frameInFlightIndex];

    // Once we know what image we work with...

    updateForImage(imageIndex);
// ============================================================================
    const auto doStagesResult = doStages(HmlFrameData{
        .frameInFlightIndex = frameInFlightIndex,
        .swapchainImageIndex = imageIndex,
        .currentFrameIndex = hmlContext->currentFrame,
        .generalDescriptorSet_0 = generalDescriptorSet_0_perImage[imageIndex],
    });
// ============================================================================
    bool mustRecreateSwapchain = false;
    const auto startPresent = std::chrono::high_resolution_clock::now();
    {
        VkSemaphore waitSemaphores[] = { renderToSwapchainImageFinished[frameInFlightIndex] };

        VkPresentInfoKHR presentInfo = {};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = waitSemaphores;
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = &(hmlContext->hmlSwapchain->swapchain);
        presentInfo.pImageIndices = &imageIndex;

        // NOTE We recreate the swapchain twice: first we get notified through
        // vkQueuePresentKHR and on the next iteration through framebufferResizeRequested.
        // The second call is probably redundant but non-harmful.
        if (const VkResult result = vkQueuePresentKHR(hmlContext->hmlDevice->presentQueue, &presentInfo);
                result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || hmlContext->hmlWindow->framebufferResizeRequested) {
            hmlContext->hmlWindow->framebufferResizeRequested = false;
            mustRecreateSwapchain = true;
        } else if (result != VK_SUCCESS) {
            std::cerr << "::> Failed to present swapchain image.\n";
            return std::nullopt;
        }
    }
    const auto endPresent = std::chrono::high_resolution_clock::now();
// ============================================================================
    frameInFlightIndex = (frameInFlightIndex + 1) % hmlContext->maxFramesInFlight;
// ============================================================================
    return { FrameResult{
        .stats = FrameResult::Stats{
            .elapsedMicrosWaitNextInFlightFrame = static_cast<float>(std::chrono::duration_cast<std::chrono::microseconds>(endWaitNextInFlightFrame - startWaitNextInFlightFrame).count()) / 1.0f,
            .elapsedMicrosAcquire = static_cast<float>(std::chrono::duration_cast<std::chrono::microseconds>(endAcquire - startAcquire).count()) / 1.0f,
            .elapsedMicrosWaitSwapchainImage = static_cast<float>(std::chrono::duration_cast<std::chrono::microseconds>(endWaitSwapchainImage - startWaitSwapchainImage).count()) / 1.0f,
            .elapsedMicrosRecord = doStagesResult ? doStagesResult->elapsedMicrosRecord : 0.0f,
            .elapsedMicrosSubmit = doStagesResult ? doStagesResult->elapsedMicrosSubmit : 0.0f,
            .elapsedMicrosPresent = static_cast<float>(std::chrono::duration_cast<std::chrono::microseconds>(endPresent - startPresent).count()) / 1.0f,
        },
        .mustRecreateSwapchain = mustRecreateSwapchain,
    }};
}


std::optional<HmlDispatcher::DoStagesResult> HmlDispatcher::doStages(const HmlFrameData& frameData) noexcept {
#if MERGE_CMD_SUBMITS || MERGE_CMD_SUBMITS_BUT_SPLIT_ACQUIRE
    std::vector<VkCommandBuffer> primaryCommandBuffers;
#endif
    const auto startRecord = std::chrono::high_resolution_clock::now();
    for (size_t stageIndex = 0; stageIndex < stages.size(); stageIndex++) {
        const auto& stage = stages[stageIndex];

        // ================ Pre-stage funcs ================
        if (stage.preFunc) (*stage.preFunc)(false, frameData);

        // ================ Submit ================
        {
            // ======== Record secondary command buffers ========
            const auto commandBuffer = stage.commandBuffers[frameData.swapchainImageIndex];
            stage.renderPass->begin(commandBuffer, frameData.swapchainImageIndex);
            {
                // NOTE These are parallelizable
                std::vector<VkCommandBuffer> secondaryCommandBuffers;
                for (const auto& drawer : stage.drawers) {
                    drawer->selectRenderPass(stage.renderPass);
                    secondaryCommandBuffers.push_back(drawer->draw(frameData));
                }
                vkCmdExecuteCommands(commandBuffer, secondaryCommandBuffers.size(), secondaryCommandBuffers.data());
            }
            stage.renderPass->end(commandBuffer);


#if MERGE_CMD_SUBMITS || MERGE_CMD_SUBMITS_BUT_SPLIT_ACQUIRE
            primaryCommandBuffers.push_back(commandBuffer);
#endif
#if !MERGE_CMD_SUBMITS || MERGE_CMD_SUBMITS_BUT_SPLIT_ACQUIRE
            // ======== Submit primary command buffer ========
            std::vector<VkPipelineStageFlags> waitStages;
            std::vector<VkSemaphore> waitSemaphores;
            std::vector<VkSemaphore> signalSemaphores;

            const bool waitForSwapchainImage = stage.flags & STAGE_FLAG_FIRST_USAGE_OF_SWAPCHAIN_IMAGE;
            if (waitForSwapchainImage) {
                // Before starting to write into the swapchain image (so at
                // VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT), wait for it
                // to become available.
                waitSemaphores.push_back(swapchainImageAvailable[frameData.frameInFlightIndex]);
                waitStages.push_back(VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
            }

            const bool signalSwapchainImageReady = stage.flags & STAGE_FLAG_LAST_USAGE_OF_SWAPCHAIN_IMAGE;
            if (signalSwapchainImageReady) {
                // Use this semaphore to tell the presentation engine when we have finished rendering into the swapchain image
                signalSemaphores.push_back(renderToSwapchainImageFinished[frameData.frameInFlightIndex]);
            }

            VkFence signalFence = VK_NULL_HANDLE;
            const bool lastStage = stageIndex == stages.size() - 1;
            if (lastStage) {
                // Use this fence to know when we can start dispatching a new frame in flight
                vkResetFences(hmlContext->hmlDevice->device, 1, &finishedLastStageOf[frameData.frameInFlightIndex]);
                signalFence = finishedLastStageOf[frameData.frameInFlightIndex];
            }

#if MERGE_CMD_SUBMITS_BUT_SPLIT_ACQUIRE
            if (waitForSwapchainImage || signalSwapchainImageReady || lastStage) {
#endif // MERGE_CMD_SUBMITS_BUT_SPLIT_ACQUIRE
                VkSubmitInfo submitInfo{
                    .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                    .pNext = nullptr,
                    .waitSemaphoreCount = static_cast<uint32_t>(waitSemaphores.size()),
                    .pWaitSemaphores = waitSemaphores.data(),
                    .pWaitDstStageMask = waitStages.data(),
#if MERGE_CMD_SUBMITS_BUT_SPLIT_ACQUIRE
                    .commandBufferCount = primaryCommandBuffers.size(),
                    .pCommandBuffers = primaryCommandBuffers.data(),
#else
                    .commandBufferCount = 1,
                    .pCommandBuffers = &commandBuffer,
#endif // MERGE_CMD_SUBMITS_BUT_SPLIT_ACQUIRE
                    .signalSemaphoreCount = static_cast<uint32_t>(signalSemaphores.size()),
                    .pSignalSemaphores = signalSemaphores.data(),
                };


                if (vkQueueSubmit(hmlContext->hmlDevice->graphicsQueue, 1, &submitInfo, signalFence) != VK_SUCCESS) {
                    std::cerr << "::> HmlDispatcher: failed to submit command buffer for stage "
                        << stageIndex << "/" << (stages.size() - 1) << ".\n";
#if EXIT_ON_ERROR
                    // To quickly crash the application and not wait for it to un-hang
                    exit(-1);
#endif
                    return std::nullopt;
                }

#if MERGE_CMD_SUBMITS_BUT_SPLIT_ACQUIRE
                primaryCommandBuffers.clear();
            }
#endif // MERGE_CMD_SUBMITS_BUT_SPLIT_ACQUIRE
#endif // !MERGE_CMD_SUBMITS || MERGE_CMD_SUBMITS_BUT_SPLIT_ACQUIRE
        }

        // ================ Post-stage funcs ================
        if (stage.postFunc) (*stage.postFunc)(frameData);
    }
    const auto endRecord = std::chrono::high_resolution_clock::now();

    const auto startSubmit = std::chrono::high_resolution_clock::now();
#if MERGE_CMD_SUBMITS
    // ======== Submit primary command buffer ========
    std::vector<VkPipelineStageFlags> waitStages;
    std::vector<VkSemaphore> waitSemaphores;
    std::vector<VkSemaphore> signalSemaphores;

    // Before starting to write into the swapchain image (so at
    // VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT), wait for it
    // to become available.
    waitSemaphores.push_back(swapchainImageAvailable[frameData.frameInFlightIndex]);
    waitStages.push_back(VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);

    // Use this semaphore to tell the presentation engine when we have finished rendering into the swapchain image
    signalSemaphores.push_back(renderToSwapchainImageFinished[frameData.frameInFlightIndex]);

    // Use this fence to know when we can start dispatching a new frame in flight
    vkResetFences(hmlContext->hmlDevice->device, 1, &finishedLastStageOf[frameData.frameInFlightIndex]);
    VkFence signalFence = finishedLastStageOf[frameData.frameInFlightIndex];

    VkSubmitInfo submitInfo{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .pNext = nullptr,
        .waitSemaphoreCount = static_cast<uint32_t>(waitSemaphores.size()),
        .pWaitSemaphores = waitSemaphores.data(),
        .pWaitDstStageMask = waitStages.data(),
        .commandBufferCount = static_cast<uint32_t>(primaryCommandBuffers.size()),
        .pCommandBuffers = primaryCommandBuffers.data(),
        .signalSemaphoreCount = static_cast<uint32_t>(signalSemaphores.size()),
        .pSignalSemaphores = signalSemaphores.data(),
    };

    if (vkQueueSubmit(hmlContext->hmlDevice->graphicsQueue, 1, &submitInfo, signalFence) != VK_SUCCESS) {
        std::cerr << "::> HmlDispatcher: failed to submit command buffer.\n";
#if EXIT_ON_ERROR
        // To quickly crash the application and not wait for it to un-hang
        exit(-1);
#endif
        return std::nullopt;
    }
#endif // MERGE_CMD_SUBMITS
    const auto endSubmit = std::chrono::high_resolution_clock::now();

    return { DoStagesResult{
        .elapsedMicrosRecord = static_cast<float>(std::chrono::duration_cast<std::chrono::microseconds>(endRecord - startRecord).count()) / 1.0f,
        .elapsedMicrosSubmit = static_cast<float>(std::chrono::duration_cast<std::chrono::microseconds>(endSubmit - startSubmit).count()) / 1.0f,
    }};
}


bool HmlDispatcher::createSyncObjects() noexcept {
    swapchainImageAvailable.resize(hmlContext->maxFramesInFlight);
    renderToSwapchainImageFinished.resize(hmlContext->maxFramesInFlight);
    finishedLastStageOf.resize(hmlContext->maxFramesInFlight);
    imagesInFlight.resize(hmlContext->imageCount(), VK_NULL_HANDLE); // initially not a single frame is using an image

    VkSemaphoreCreateInfo semaphoreInfo = {};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo = {};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    // So that the fences start in signaled state (which means the frame is
    // available to be acquired by CPU). Subsequent frames are signaled by
    // Vulkan upon command buffer execution finish.
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (size_t i = 0; i < hmlContext->maxFramesInFlight; i++) {
        if (vkCreateSemaphore(hmlContext->hmlDevice->device, &semaphoreInfo, nullptr, &swapchainImageAvailable[i]) != VK_SUCCESS ||
            vkCreateSemaphore(hmlContext->hmlDevice->device, &semaphoreInfo, nullptr, &renderToSwapchainImageFinished[i]) != VK_SUCCESS ||
            vkCreateFence    (hmlContext->hmlDevice->device, &fenceInfo,     nullptr, &finishedLastStageOf[i])           != VK_SUCCESS) {

            std::cerr << "::> Failed to create sync objects.\n";
            return false;
        }
    }

    return true;
}


bool HmlDispatcher::addStage(StageCreateInfo&& stageCreateInfo) noexcept {
    std::shared_ptr<HmlRenderPass> hmlRenderPass = HmlRenderPass::create(hmlContext->hmlDevice, hmlContext->hmlCommands, HmlRenderPass::Config{
        .colorAttachments = std::move(stageCreateInfo.colorAttachments),
        .depthStencilAttachment = stageCreateInfo.depthAttachment,
        .extent = hmlContext->hmlSwapchain->extent(),
        .subpassDependencies = std::move(stageCreateInfo.subpassDependencies),
    });

    if (!hmlRenderPass) {
        std::cerr << "::> HmlDispatcher: failed to create a HmlRenderPass for stage #" << stages.size() << ".\n";
        return false;
    }


    // NOTE This is done here to allow HmlDrawer re-configuration in-between addStages
    // NOTE there is no valid frame data for the prep stage, and funcs should not
    // use that data in prep stage anyway.
    HmlFrameData frameData = {};
    if (stageCreateInfo.preFunc) (*stageCreateInfo.preFunc)(true, frameData);
    for (auto& drawer : stageCreateInfo.drawers) {
        if (!clearedDrawers.contains(drawer)) {
            clearedDrawers.insert(drawer);
            drawer->clearRenderPasses();
        }
        drawer->addRenderPass(hmlRenderPass);
    }
    if (stageCreateInfo.postFunc) (*stageCreateInfo.postFunc)(frameData);

    const auto count = hmlContext->imageCount();
    const auto pool = hmlContext->hmlCommands->commandPoolOnetimeFrames;
    stages.push_back(Stage{
        .drawers = std::move(stageCreateInfo.drawers),
        .commandBuffers = hmlContext->hmlCommands->allocatePrimary(count, pool),
        .renderPass = std::move(hmlRenderPass),
        // .postTransitions = std::move(postTransitions),
        .preFunc = std::move(stageCreateInfo.preFunc),
        .postFunc = std::move(stageCreateInfo.postFunc),
        .flags = stageCreateInfo.flags,
    });

    // We need (stagesCount - 1) batches of semaphores, so skip the first one
    // if (stages.size() == 1) return true;
    // return addSemaphoresForNewStage();
    return true;
}
