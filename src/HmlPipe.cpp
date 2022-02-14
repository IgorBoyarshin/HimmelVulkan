#include "HmlPipe.h"


void HmlPipe::addStage(
        std::vector<std::shared_ptr<HmlDrawer>>&& drawers,
        std::vector<HmlRenderPass::ColorAttachment>&& colorAttachments,
        std::optional<HmlRenderPass::DepthStencilAttachment> depthAttachment,
        std::vector<HmlTransitionRequest>&& postTransitions,
        std::optional<std::function<void(uint32_t)>> postFunc
        ) noexcept {
    stages.push_back(HmlStage{
        .drawers = std::move(drawers),
        .commandBuffers = hmlCommands->allocatePrimary(hmlSwapchain->imageCount(), hmlCommands->commandPoolOnetimeFrames),
        .renderPass = HmlRenderPass::create(hmlDevice, hmlCommands, HmlRenderPass::Config{
            .colorAttachments = std::move(colorAttachments),
            .depthStencilAttachment = depthAttachment,
            .extent = hmlSwapchain->extent,
        }),
        .postTransitions = std::move(postTransitions),
        .postFunc = postFunc,
    });
}


bool HmlPipe::verify() const noexcept {
    for (size_t i = 0; i < stages.size(); i++) {
        if (!stages[i].renderPass) {
            std::cerr << "::> Failed to create a HmlRenderPass for stage #" << i << ".\n";
            return false;
        }
    }
    return true;
}


void HmlPipe::assignRenderPasses() noexcept {
    for (const auto& stage : stages) {
        for (const auto& drawer : stage.drawers) {
            drawer->clearRenderPasses();
        }
    }

    for (const auto& stage : stages) {
        for (const auto& drawer : stage.drawers) {
            drawer->addRenderPass(stage.renderPass);
        }
    }
}


bool HmlPipe::createSemaphores(uint32_t maxFramesInFlight) noexcept {
    VkSemaphoreCreateInfo semaphoreInfo = {};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    for (uint32_t frame = 0; frame < maxFramesInFlight; frame++) {
        std::vector<VkSemaphore> semaphores(stages.size() - 1);
        for (size_t stage = 0; stage < stages.size() - 1; stage++) {
            if (vkCreateSemaphore(hmlDevice->device, &semaphoreInfo, nullptr, &semaphores[stage]) != VK_SUCCESS) {
                std::cerr << "::> Failed to create a VkSemaphore for HmlPipe.\n";
                return false;
            }
        }
        semaphoresPerFrame.push_back(std::move(semaphores));
    }
    return true;
}


bool HmlPipe::bake(uint32_t maxFramesInFlight) noexcept {
    if (!verify()) {
        std::cerr << "::> Not all stages have HmlRenderPass inside HmlPipe.\n";
        return false;
    }
    assignRenderPasses();
    if (!createSemaphores(maxFramesInFlight)) return false;
    return true;
}


void HmlPipe::run(const HmlFrameData& frameData) noexcept {
    const auto& semaphoresFinishedStage = semaphoresPerFrame[frameData.frameIndex];

    for (size_t stageIndex = 0; stageIndex < stages.size(); stageIndex++) {
        const auto& stage = stages[stageIndex];
        const bool firstStage = stageIndex == 0;
        const bool lastStage  = stageIndex == stages.size() - 1;

        // ============== Submit
        {
            const auto commandBuffer = stage.commandBuffers[frameData.imageIndex];
            stage.renderPass->begin(commandBuffer, frameData.imageIndex);
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


            VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
            VkSemaphore waitSemaphores[]   = { firstStage ? imageAvailableSemaphores[frameData.frameIndex] : semaphoresFinishedStage[stageIndex - 1] };
            VkSemaphore signalSemaphores[] = { lastStage  ? renderFinishedSemaphores[frameData.frameIndex] : semaphoresFinishedStage[stageIndex] };

            VkSubmitInfo submitInfo{
                .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                    .pNext = nullptr,
                    .waitSemaphoreCount = 1,
                    .pWaitSemaphores = waitSemaphores,
                    .pWaitDstStageMask = waitStages,
                    .commandBufferCount = 1,
                    .pCommandBuffers = &commandBuffer,
                    .signalSemaphoreCount = 1,
                    .pSignalSemaphores = signalSemaphores,
            };

            VkFence signalFence = VK_NULL_HANDLE;
            if (lastStage) {
                // The specified fence will get signaled when the command buffer finishes executing.
                vkResetFences(hmlDevice->device, 1, &inFlightFences[frameData.frameIndex]);
                signalFence = inFlightFences[frameData.frameIndex];
            }
            if (vkQueueSubmit(hmlDevice->graphicsQueue, 1, &submitInfo, signalFence) != VK_SUCCESS) {
                std::cerr << "::> Failed to submit draw command buffer.\n";
                return;
            }
        }
        // ============== Post-stage transitions
        {
            const auto commandBuffer = hmlCommands->beginLongTermSingleTimeCommand();
            for (const auto& transition : stage.postTransitions) {
                transition.resourcePerImage[frameData.imageIndex]->transitionLayoutTo(transition.dstLayout, commandBuffer);
            }
            hmlCommands->endLongTermSingleTimeCommand(commandBuffer);
        }
        // ============== Post-stage funcs
        if (stage.postFunc) (*stage.postFunc)(frameData.imageIndex);
    }
}
