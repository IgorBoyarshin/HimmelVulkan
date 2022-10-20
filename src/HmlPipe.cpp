#include "HmlPipe.h"


bool HmlPipe::addStage(
        std::vector<std::shared_ptr<HmlDrawer>>&& drawers,
        std::vector<HmlRenderPass::ColorAttachment>&& colorAttachments,
        std::optional<HmlRenderPass::DepthStencilAttachment> depthAttachment,
        std::vector<HmlTransitionRequest>&& postTransitions,
        std::optional<std::function<void(uint32_t)>> postFunc
        ) noexcept {
    std::shared_ptr<HmlRenderPass> hmlRenderPass = HmlRenderPass::create(hmlContext->hmlDevice, hmlContext->hmlCommands, HmlRenderPass::Config{
        .colorAttachments = std::move(colorAttachments),
        .depthStencilAttachment = depthAttachment,
        .extent = hmlContext->hmlSwapchain->extent,
    });

    if (!hmlRenderPass) {
        std::cerr << "::> Failed to create a HmlRenderPass for stage #" << stages.size() << ".\n";
        return false;
    }


    // NOTE This is done here to allow HmlDrawer re-configuration in-between addStages
    for (auto& drawer : drawers) {
        if (!clearedDrawers.contains(drawer)) {
            clearedDrawers.insert(drawer);
            drawer->clearRenderPasses();
        }
        drawer->addRenderPass(hmlRenderPass);
    }

    const auto count = hmlContext->imageCount();
    const auto pool = hmlContext->hmlCommands->commandPoolOnetimeFrames;
    stages.push_back(HmlStage{
        .drawers = std::move(drawers),
        .commandBuffers = hmlContext->hmlCommands->allocatePrimary(count, pool),
        .renderPass = hmlRenderPass,
        .postTransitions = std::move(postTransitions),
        .postFunc = postFunc,
    });

    // We need (stagesCount - 1) batches of semaphores, so skip the first one
    if (stages.size() == 1) return true;
    return addSemaphoresForNewStage();
}


bool HmlPipe::addSemaphoresForNewStage() noexcept {
    VkSemaphoreCreateInfo semaphoreInfo = {};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    const size_t addCount = hmlContext->framesInFlight();
    semaphoresForFrames.resize(semaphoresForFrames.size() + addCount);

    for (auto it = semaphoresForFrames.end() - addCount; it != semaphoresForFrames.end(); ++it) {
        if (vkCreateSemaphore(hmlContext->hmlDevice->device, &semaphoreInfo, nullptr, &*it) != VK_SUCCESS) {
            std::cerr << "::> Failed to create a VkSemaphore for HmlPipe.\n";
            return false;
        }
    }

    return true;
}


void HmlPipe::run(const HmlFrameData& frameData) noexcept {
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
            VkSemaphore waitSemaphores[]   = { firstStage ?
                imageAvailableSemaphores[frameData.frameIndex] : semaphoreFinishedStageOfFrame(stageIndex - 1, frameData.frameIndex) };
            VkSemaphore signalSemaphores[] = { lastStage  ?
                renderFinishedSemaphores[frameData.frameIndex] : semaphoreFinishedStageOfFrame(stageIndex,     frameData.frameIndex) };

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
                vkResetFences(hmlContext->hmlDevice->device, 1, &inFlightFences[frameData.frameIndex]);
                signalFence = inFlightFences[frameData.frameIndex];
            }
            if (vkQueueSubmit(hmlContext->hmlDevice->graphicsQueue, 1, &submitInfo, signalFence) != VK_SUCCESS) {
                std::cerr << "::> Failed to submit draw command buffer.\n";
#if DEBUG
                // To quickly crash the application and not wait for it to un-hang
                exit(-1);
#endif
                return;
            }
        }
        // ============== Post-stage transitions
        {
            if (!stage.postTransitions.empty()) {
                const auto commandBuffer = hmlContext->hmlCommands->beginLongTermSingleTimeCommand();
                for (const auto& transition : stage.postTransitions) {
                    transition.resourcePerImage[frameData.imageIndex]->transitionLayoutTo(transition.dstLayout, commandBuffer);
                }
                hmlContext->hmlCommands->endLongTermSingleTimeCommand(commandBuffer);
            }
        }
        // ============== Post-stage funcs
        if (stage.postFunc) (*stage.postFunc)(frameData.imageIndex);
    }
}
