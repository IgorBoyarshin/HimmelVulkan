#include "HmlBloomRenderer.h"


std::vector<std::unique_ptr<HmlPipeline>> HmlBloomRenderer::createPipelines(
        std::shared_ptr<HmlRenderPass> hmlRenderPass, const std::vector<VkDescriptorSetLayout>& descriptorSetLayouts) noexcept {
    std::vector<std::unique_ptr<HmlPipeline>> pipelines;

    {
        HmlGraphicsPipelineConfig config{
            .bindingDescriptions   = {},
            .attributeDescriptions = {},
            .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
            .hmlShaders = HmlShaders()
                .addVertex("../shaders/out/bloom.vert.spv")
                .addFragment("../shaders/out/bloom.frag.spv"),
            .renderPass = hmlRenderPass->renderPass,
            .extent = hmlRenderPass->extent,
            .polygoneMode = VK_POLYGON_MODE_FILL,
            // .cullMode = VK_CULL_MODE_BACK_BIT,
            .cullMode = VK_CULL_MODE_NONE,
            .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
            .descriptorSetLayouts = descriptorSetLayouts,
            .pushConstantsStages = 0,
            .pushConstantsSizeBytes = 0,
            .tessellationPatchPoints = 0,
            .lineWidth = 1.0f,
            .colorAttachmentCount = hmlRenderPass->colorAttachmentCount,
            .withBlending = false,
            .withDepthTest = true,
        };

        pipelines.push_back(HmlPipeline::createGraphics(hmlContext->hmlDevice, std::move(config)));
    }

    return pipelines;
}


std::unique_ptr<HmlBloomRenderer> HmlBloomRenderer::create(std::shared_ptr<HmlContext> hmlContext) noexcept {
    auto hmlRenderer = std::make_unique<HmlBloomRenderer>();
    hmlRenderer->hmlContext = hmlContext;

    const auto imageCount = hmlContext->imageCount();

    hmlRenderer->descriptorPool = hmlContext->hmlDescriptors->buildDescriptorPool()
        .withTextures(imageCount * 2)
        .maxDescriptorSets(imageCount)
        .build(hmlContext->hmlDevice);
    if (!hmlRenderer->descriptorPool) return { nullptr };

    hmlRenderer->descriptorSetLayoutTextures = hmlContext->hmlDescriptors->buildDescriptorSetLayout()
        .withTextureAt(0, VK_SHADER_STAGE_FRAGMENT_BIT)
        .withTextureAt(1, VK_SHADER_STAGE_FRAGMENT_BIT)
        .build(hmlContext->hmlDevice);
    if (!hmlRenderer->descriptorSetLayoutTextures) return { nullptr };
    hmlRenderer->descriptorSetLayouts.push_back(hmlRenderer->descriptorSetLayoutTextures);

    hmlRenderer->descriptorSet_textures_0_perImage = hmlContext->hmlDescriptors->createDescriptorSets(imageCount,
        hmlRenderer->descriptorSetLayoutTextures, hmlRenderer->descriptorPool);
    if (hmlRenderer->descriptorSet_textures_0_perImage.empty()) return { nullptr };


    // hmlRenderer->hmlPipeline = createPipeline(hmlContext->hmlDevice, hmlRenderPass->extent,
    //     hmlRenderPass->renderPass, hmlRenderer->descriptorSetLayouts);
    // if (!hmlRenderer->hmlPipeline) return { nullptr };


    // hmlRenderer->commandBuffers = hmlCommands->allocateSecondary(framesInFlight, hmlCommands->commandPoolOnetimeFrames);

    return hmlRenderer;
}


HmlBloomRenderer::~HmlBloomRenderer() noexcept {
#if LOG_DESTROYS
    std::cout << ":> Destroying HmlBloomRenderer...\n";
#endif

    // NOTE depends on swapchain recreation, but because it only depends on the
    // NOTE number of images, which most likely will not change, we ignore it.
    // DescriptorSets are freed automatically upon the deletion of the pool
    vkDestroyDescriptorPool(hmlContext->hmlDevice->device, descriptorPool, nullptr);
    vkDestroyDescriptorSetLayout(hmlContext->hmlDevice->device, descriptorSetLayoutTextures, nullptr);
}


void HmlBloomRenderer::specify(
        const std::vector<std::shared_ptr<HmlImageResource>>& mainTextures,
        const std::vector<std::shared_ptr<HmlImageResource>>& brightnessTextures
        ) noexcept {
    const auto imageCount = mainTextures.size();
    for (size_t imageIndex = 0; imageIndex < imageCount; imageIndex++) {
        HmlDescriptorSetUpdater(descriptorSet_textures_0_perImage[imageIndex])
            .textureAt(0, mainTextures[imageIndex]->sampler, mainTextures[imageIndex]->view)
            .textureAt(1, brightnessTextures[imageIndex]->sampler, brightnessTextures[imageIndex]->view)
            .update(hmlContext->hmlDevice);
    }
}


VkCommandBuffer HmlBloomRenderer::draw(const HmlFrameData& frameData) noexcept {
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
    hml::DebugLabel debugLabel{commandBuffer, "Bloom"};
#endif
#if USE_TIMESTAMP_QUERIES
    hmlContext->hmlQueries->registerEvent("HmlBloomRenderer: begin", "BLw", commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT);
#endif

    assert(getCurrentPipelines().size() == 1 && "::> Expected only a single pipeline in HmlBloomRenderer.\n");
    const auto& hmlPipeline = getCurrentPipelines()[0];
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, hmlPipeline->pipeline);

    std::array<VkDescriptorSet, 1> descriptorSets = {
        descriptorSet_textures_0_perImage[frameData.swapchainImageIndex]
    };
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
        hmlPipeline->layout, 0, descriptorSets.size(), descriptorSets.data(), 0, nullptr);

    const uint32_t instanceCount = 1;
    const uint32_t firstInstance = 0;
    const uint32_t vertexCount = 6; // a simple square
    const uint32_t firstVertex = 0;
    vkCmdDraw(commandBuffer, vertexCount, instanceCount, firstVertex, firstInstance);

#if USE_TIMESTAMP_QUERIES
    hmlContext->hmlQueries->registerEvent("HmlBloomRenderer: end", "BL", commandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);
#endif
#if USE_DEBUG_LABELS
    debugLabel.end();
#endif
    hmlContext->hmlCommands->endRecording(commandBuffer);
    return commandBuffer;
}
