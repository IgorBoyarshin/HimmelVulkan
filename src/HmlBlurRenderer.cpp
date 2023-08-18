#include "HmlBlurRenderer.h"


std::vector<std::unique_ptr<HmlPipeline>> HmlBlurRenderer::createPipelines(
        std::shared_ptr<HmlRenderPass> hmlRenderPass, const std::vector<VkDescriptorSetLayout>& descriptorSetLayouts) noexcept {
    std::vector<std::unique_ptr<HmlPipeline>> pipelines;

    HmlGraphicsPipelineConfig config{
        .bindingDescriptions   = {},
        .attributeDescriptions = {},
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        .hmlShaders = HmlShaders()
            .addVertex("../shaders/out/blur.vert.spv")
            .addFragment("../shaders/out/blur.frag.spv"),
        .renderPass = hmlRenderPass->renderPass,
        .extent = hmlRenderPass->extent,
        .polygoneMode = VK_POLYGON_MODE_FILL,
        // .cullMode = VK_CULL_MODE_BACK_BIT,
        .cullMode = VK_CULL_MODE_NONE,
        .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
        .descriptorSetLayouts = descriptorSetLayouts,
        .pushConstantsStages = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        .pushConstantsSizeBytes = sizeof(PushConstant),
        .tessellationPatchPoints = 0,
        .lineWidth = 1.0f,
        .colorAttachmentCount = hmlRenderPass->colorAttachmentCount,
        .withBlending = false,
        .withDepthTest = false,
    };

    pipelines.push_back(HmlPipeline::createGraphics(hmlContext->hmlDevice, std::move(config)));
    return pipelines;
}


std::unique_ptr<HmlBlurRenderer> HmlBlurRenderer::create(std::shared_ptr<HmlContext> hmlContext) noexcept {
    auto hmlRenderer = std::make_unique<HmlBlurRenderer>();
    hmlRenderer->hmlContext = hmlContext;

    const auto imageCount = hmlContext->imageCount();

    hmlRenderer->descriptorPool = hmlContext->hmlDescriptors->buildDescriptorPool()
        .withTextures(imageCount * 2)
        .maxDescriptorSets(imageCount * 2)
        .build(hmlContext->hmlDevice);
    if (!hmlRenderer->descriptorPool) return { nullptr };

    hmlRenderer->descriptorSetLayoutTextures = hmlContext->hmlDescriptors->buildDescriptorSetLayout()
        .withTextureAt(0, VK_SHADER_STAGE_FRAGMENT_BIT)
        .build(hmlContext->hmlDevice);
    if (!hmlRenderer->descriptorSetLayoutTextures) return { nullptr };
    hmlRenderer->descriptorSetLayouts.push_back(hmlRenderer->descriptorSetLayoutTextures);

    hmlRenderer->descriptorSet_first_0_perImage = hmlContext->hmlDescriptors->createDescriptorSets(imageCount,
        hmlRenderer->descriptorSetLayoutTextures, hmlRenderer->descriptorPool);
    if (hmlRenderer->descriptorSet_first_0_perImage.empty()) return { nullptr };

    hmlRenderer->descriptorSet_second_0_perImage = hmlContext->hmlDescriptors->createDescriptorSets(imageCount,
        hmlRenderer->descriptorSetLayoutTextures, hmlRenderer->descriptorPool);
    if (hmlRenderer->descriptorSet_second_0_perImage.empty()) return { nullptr };


    // hmlRenderer->hmlPipeline = createPipeline(hmlContext->hmlDevice, hmlRenderPass->extent,
    //     hmlRenderPass->renderPass, hmlRenderer->descriptorSetLayouts);
    // if (!hmlRenderer->hmlPipeline) return { nullptr };


    // hmlRenderer->commandBuffers = hmlCommands->allocateSecondary(framesInFlight, hmlCommands->commandPoolOnetimeFrames);

    return hmlRenderer;
}


HmlBlurRenderer::~HmlBlurRenderer() noexcept {
#if LOG_DESTROYS
    std::cout << ":> Destroying HmlBlurRenderer...\n";
#endif

    // NOTE depends on swapchain recreation, but because it only depends on the
    // NOTE number of images, which most likely will not change, we ignore it.
    // DescriptorSets are freed automatically upon the deletion of the pool
    vkDestroyDescriptorPool(hmlContext->hmlDevice->device, descriptorPool, nullptr);
    vkDestroyDescriptorSetLayout(hmlContext->hmlDevice->device, descriptorSetLayoutTextures, nullptr);
}


void HmlBlurRenderer::specify(
        const std::vector<std::shared_ptr<HmlImageResource>>& firstTextures,
        const std::vector<std::shared_ptr<HmlImageResource>>& secondTextures) noexcept {
    const auto imageCount = firstTextures.size();
    for (size_t imageIndex = 0; imageIndex < imageCount; imageIndex++) {
        HmlDescriptorSetUpdater(descriptorSet_first_0_perImage[imageIndex])
            .textureAt(0, firstTextures[imageIndex]->sampler, firstTextures[imageIndex]->view).update(hmlContext->hmlDevice);
        HmlDescriptorSetUpdater(descriptorSet_second_0_perImage[imageIndex])
            .textureAt(0, secondTextures[imageIndex]->sampler, secondTextures[imageIndex]->view).update(hmlContext->hmlDevice);
    }
}


void HmlBlurRenderer::modeHorizontal() noexcept {
    isVertical = 0;
}


void HmlBlurRenderer::modeVertical() noexcept {
    isVertical = 1;
}


VkCommandBuffer HmlBlurRenderer::draw(const HmlFrameData& frameData) noexcept {
    auto commandBuffer = getCurrentCommands()[frameData.frameInFlightIndex];
    const auto inheritanceInfo = VkCommandBufferInheritanceInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO,
        .pNext = VK_NULL_HANDLE,
        .renderPass = currentRenderPass->renderPass,
        .subpass = 0, // we only have a single one
        .framebuffer = currentRenderPass->framebuffers[frameData.swapchainImageIndex],
        .occlusionQueryEnable = VK_FALSE,
        .queryFlags = static_cast<VkQueryControlFlags>(0),
        .pipelineStatistics = static_cast<VkQueryPipelineStatisticFlags>(0)
    };
    hmlContext->hmlCommands->beginRecordingSecondaryOnetime(commandBuffer, &inheritanceInfo);

#if USE_DEBUG_LABELS
    hml::DebugLabel debugLabel(commandBuffer, isVertical ? "Blur vertical" : "Blur horizontal");
#endif
#if USE_TIMESTAMP_QUERIES
    if (isVertical) {
        hmlContext->hmlQueries->registerEvent("HmlBlurRenderer: vertical pass", "BVw", commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT);
    } else {
        hmlContext->hmlQueries->registerEvent("HmlBlurRenderer: horizontal pass", "BHw", commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT);
    }
#endif

    const auto& hmlPipelines = getCurrentPipelines();
    assert(hmlPipelines.size() == 1 && "::> Expected only a single pipeline in HmlBlurRenderer.\n");
    const auto& hmlPipeline = hmlPipelines[0];
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, hmlPipeline->pipeline);

    std::array<VkDescriptorSet, 1> descriptorSets = {
        (isVertical == 0) ?
            descriptorSet_first_0_perImage[frameData.swapchainImageIndex] :
            descriptorSet_second_0_perImage[frameData.swapchainImageIndex]
    };
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
        hmlPipeline->layout, 0, descriptorSets.size(), descriptorSets.data(), 0, nullptr);

    PushConstant pushConstant{
        .isHorizontal = !isVertical,
    };
    vkCmdPushConstants(commandBuffer, hmlPipeline->layout,
        hmlPipeline->pushConstantsStages, 0, sizeof(PushConstant), &pushConstant);

    const uint32_t instanceCount = 1;
    const uint32_t firstInstance = 0;
    const uint32_t vertexCount = 6; // a simple square
    const uint32_t firstVertex = 0;
    vkCmdDraw(commandBuffer, vertexCount, instanceCount, firstVertex, firstInstance);

#if USE_TIMESTAMP_QUERIES
    if (isVertical) {
        hmlContext->hmlQueries->registerEvent("HmlBlurRenderer: vertical pass", "BV", commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT);
    } else {
        hmlContext->hmlQueries->registerEvent("HmlBlurRenderer: horizontal pass", "BH", commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT);
    }
#endif

    hmlContext->hmlCommands->endRecording(commandBuffer);
    return commandBuffer;
}


// void selectRenderPass(std::shared_ptr<HmlRenderPass> renderPass) noexcept {
//     currentRenderPass = renderPass;
// }
//
//
// void addRenderPass(std::shared_ptr<HmlRenderPass> newHmlRenderPass) noexcept {
//     auto hmlPipeline = createPipeline(hmlContext->hmlDevice, newHmlRenderPass->extent, newHmlRenderPass, descriptorSetLayouts);
//     pipelineForRenderPassStorage.emplace_back(newHmlRenderPass, std::move(hmlPipeline));
// }
//
//
// void clearRenderPasses() noexcept {
//     currentRenderPass.reset();
//     pipelineForRenderPassStorage.clear();
// }


// void HmlBlurRenderer::replaceRenderPass(std::shared_ptr<HmlRenderPass> newHmlRenderPass) noexcept {
//     hmlRenderPass = newHmlRenderPass;
//     hmlPipeline = createPipeline(hmlContext->hmlDevice, hmlRenderPass->extent, hmlRenderPass->renderPass, descriptorSetLayouts);
//     // NOTE The command pool is reset for all renderers prior to calling this function.
//     // NOTE commandBuffers must be rerecorded -- is done during baking
// }
